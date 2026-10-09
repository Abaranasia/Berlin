# Design: Settings Dialog (UI Redesign Slice 4/6)

## Technical Approach

`MainComponent` implements a small `StandaloneHost` interface and passes `this` to `createBerlinEditor(processor, host)`. The plugin passes `nullptr` (default argument). `WebEditor` registers `getAudioSettings` and `applyAudioSettings` only when the host is non-null. All parsing, validation, serialization and JSON shaping live in a JUCE-core-only unit (`Source/settings/StandaloneSettings`) that `BerlinTests` covers headless. `MainComponent` keeps only the JUCE glue: device enumeration, the stopped-audio bracket, and file I/O. `MidiOutputSink` gains a port reopen path and an atomic channel that remaps outgoing channel-voice messages in place. In React, `Bridge` gets an optional `settings` member. It exists only when `__juce__functions` includes `getAudioSettings`, and it gates both the Settings button and the dialog.

## Architecture Decisions

| # | Choice | Rejected | Rationale |
|---|---|---|---|
| D1 | `StandaloneHost` is an abstract class with `getAudioSettings(const var& query)` and `applyAudioSettings(const var& request)`, both returning `juce::var`. It is message-thread only and synchronous. | Passing `AudioDeviceManager&` to `WebEditor` | Keeps `WebEditor` free of device and MIDI knowledge. The plugin path is unchanged. |
| D2 | `MainComponent` member order becomes `processor, midiSink, settingsFile state, editor, midiBlock`, so the editor (and the browser lambdas that hold `host`) is destroyed before anything the host uses. The new lambdas also return `busy` when `shuttingDown`. | Resetting `editor` manually in `~MainComponent` | Declaration order is the existing convention (`processor` MUST precede `editor`). Both calls are synchronous, so no `SafePointer` is needed. The existing dialog `SafePointer` paths stay as they are. |
| D3 | Pure unit `StandaloneSettings.{h,cpp}` (juce_core): `parseApplyRequest`, `validateApplyRequest(request, DeviceOptions)` returning a token or `""`, `settingsToXml`/`settingsFromXml`, `resolveMidiPort`, `audioSettingsToVar`, `okResult`/`errorResult`. | Validation inline in `MainComponent` | BerlinTests can't link `juce_audio_devices`. The JUCE side only gathers `DeviceOptions` (plain arrays) and applies the result. |
| D4 | Apply order: guard `applying` → parse (`invalid-request`) → gather options for the requested type/device → validate (in the order device, rate, buffer, port) → `deviceManager.closeAudioDevice()` (`releaseResources` flushes the pending note-off on the old port/channel) → MIDI swap (`openDevice`, `setChannel`) → audio: `setCurrentAudioDeviceType` + `setAudioDeviceSetup(setup, true)` if changed, else `restartLastAudioDevice()` → save. | Applying the audio setup first | Satisfies the sink contract: open/close only happens on the message thread with audio stopped. A post-validation failure (open fails) rolls back the previous port, channel, type and setup, then returns `device-unavailable` or `midi-port-unavailable`. |
| D5 | `MidiOutputSink`: `std::atomic<int> channel` (relaxed). `openDevice(id)` closes first (CC123-last kept), and `""` means None. `setChannel(ch)` sends CC123 on the old channel after `clearAllPendingMessages`. `dispatch(MidiBuffer&)` and `sendImmediately` call `remapMidiChannel` when ch≠1. `openFirstAvailableDevice` is removed (one caller). | Remapping in the processor's translator | The const translator, the plugin and export stay on channel 1 (D3 proposal). The remap rewrites status bytes in the buffer's own storage, so it does not allocate. The note-off flush goes out on the same channel as its note-on. |
| D6 | `remapChannelStatus` (constexpr, `MidiMessageBytes.h`) only touches statuses 0x80–0xEF. `remapMidiChannel(juce::MidiBuffer&, int)` is header-only in `Source/midi/MidiChannelRemap.h` (juce_audio_basics, already in BerlinTests). It writes through the iterator's data pointer, with a documented `const_cast` on non-const storage. | Copying into a second buffer | No allocation on the audio thread. SysEx and realtime messages pass through untouched. |
| D7 | Persistence: `<BerlinSettings version="1"><AUDIO>…createStateXml…</AUDIO><MIDI id name none="1"? channel/></BerlinSettings>` in `userApplicationDataDirectory/Berlin/settings.xml`, written with `XmlElement::writeTo(File)` (temp file, then replace). It is saved only after a successful apply. A failed write still returns `{ok:true}`. | `ApplicationProperties`/`PropertiesFile` | One versioned tree (juce-app-dev rule). The pure round trip is testable. |
| D8 | Launch: load → `setChannel(saved)` → `openDevice(resolveMidiPort(saved, ports))` → `setAudioChannels(2, 2, savedAudio.get())`, with `selectDefaultDeviceOnFailure=true` covering a missing device. `resolveMidiPort`: saved None → None; id found → id; name found → that id; else the first port; else None. Missing, corrupt or unknown-version files → defaults (the same as today). | Id-only matching | Windows MIDI ids can change between boots. The name fallback keeps the user's choice. |
| D9 | `getAudioSettings(query?)` takes an optional `{deviceType, device}`. Without it, it reports the current setup. When the device differs from the open one, rates and buffers come from a temporary `createDevice` that is never opened. Each call runs `scanForDevices()` (re-listing on open, no hot-plug refresh). MIDI ports start with `{id:"", name:"None"}`. | Disabling rate/buffer until Apply | The dialog needs the lists for the device the user picked. The query argument is additive to the shared contract. |
| D10 | React: `Bridge.settings?: SettingsBridge`. `nativeBridge` sets it only when `getAudioSettings` is registered, and `createMockBridge({standalone})` provides a mock. `App` passes `onOpenSettings` to `TopBar` only when `store.bridge.settings` exists, so the button is not rendered in the plugin. `SettingsDialog` is a `div role="dialog" aria-modal` (as in the mockup). Escape, Close and Cancel call `onClose` without applying. | `<dialog>.showModal` | jsdom support for it is unreliable. Capability detection comes from the bridge, not a global. |
| D11 | The dialog's output level is a `Slider` that sends `store.send('masterLevel', setMasterLevel)`, the same key as `Transport`, so the change is live. Cancel does not revert it. `formatDb(level)` in `lib/db.ts`: `level<=0 → "−∞ dB"`, else `20·log10` with 1 decimal. | Staging the level inside Apply | The level is already preset-persisted and live in the TopBar (proposal D1). One source of truth. |

## Data Flow

    SettingsDialog ─get(query?)─▶ nativeBridge.settings ─▶ WebEditor fn ─▶ StandaloneHost(MainComponent)
         │                                                     gather DeviceOptions ─▶ audioSettingsToVar
         └─apply(req)─▶ ... ─▶ parse ─▶ validate(pure) ─▶ closeAudioDevice ─▶ sink.openDevice/setChannel
                                                           ─▶ setAudioDeviceSetup | restartLastAudioDevice ─▶ save xml
    audio thread: processBlock(midiBlock ch1) ─▶ sink.dispatch: remap(ch atomic) ─▶ sendBlockOfMessages

## File Changes (authored line estimates)

| File | Action | ~Lines |
|---|---|---|
| `Source/ui/StandaloneHost.h` | Create | 25 |
| `Source/settings/StandaloneSettings.{h,cpp}` | Create (pure) | 170 |
| `Source/midi/MidiMessageBytes.h`, `Source/midi/MidiChannelRemap.h` | Modify/Create | 30 |
| `Source/midi/MidiOutputSink.{h,cpp}` | Modify | 50 |
| `Source/MainComponent.{h,cpp}` | Modify: host impl, restore, save | 150 |
| `Source/ui/EditorFactory.{h,cpp}`, `Source/ui/WebEditor.{h,cpp}` | Modify | 40 |
| `Berlin.jucer`, `Tests/BerlinTests.jucer` | Modify | 10 |
| `Tests/Source/StandaloneSettingsTests.cpp`, `MidiChannelRemapTests.cpp` | Create | 190 |
| `ui/src/bridge/{protocol,bridge,native,mock}.ts` | Modify | 100 |
| `ui/src/lib/db.ts`, `ui/src/components/SettingsDialog.tsx` | Create | 160 |
| `ui/src/components/TopBar.tsx`, `ui/src/App.tsx`, `ui/src/main.tsx`, `ui/src/styles/components.css` | Modify | 75 |
| `ui/src/{lib/db,components/SettingsDialog}.test.ts(x)`, `bridge/native.test.ts`, `App.test.tsx` | Create/Modify | 190 |

## Interfaces / Contracts

```ts
type SettingsError = 'invalid-request'|'device-unavailable'|'sample-rate-unsupported'|'buffer-size-unsupported'|'midi-port-unavailable'|'busy';
interface AudioSettings { deviceTypes: string[]; deviceType: string; devices: string[]; device: string;
  sampleRates: number[]; sampleRate: number; bufferSizes: number[]; bufferSize: number;
  midiPorts: { id: string; name: string }[]; midiPortId: string; midiChannel: number } // midiPortId "" = None
type ApplyAudioSettingsRequest = Pick<AudioSettings, 'deviceType'|'device'|'sampleRate'|'bufferSize'|'midiPortId'|'midiChannel'>;
type ApplyAudioSettingsResult = { ok: true } | { ok: false; error: SettingsError };
interface SettingsBridge { get(query?: { deviceType: string; device: string }): Promise<AudioSettings>; apply(r: ApplyAudioSettingsRequest): Promise<ApplyAudioSettingsResult> }
```
```cpp
struct StandaloneHost { virtual ~StandaloneHost() = default;
  virtual juce::var getAudioSettings (const juce::var& query) = 0; virtual juce::var applyAudioSettings (const juce::var& request) = 0; };
std::unique_ptr<juce::AudioProcessorEditor> createBerlinEditor (BerlinAudioProcessor&, StandaloneHost* host = nullptr);
```

## Testing Strategy (RED first)

| Layer | Cases |
|---|---|
| BerlinTests | `parseApplyRequest`: valid, missing field, wrong type, channel 0/17, rate ≤0 → `invalid-request`. `validateApplyRequest`: each token and the check order. XML: round trip; None round trip; a missing, corrupt, `version="2"` or channel-99 file → defaults. `resolveMidiPort`: all 5 branches. `audioSettingsToVar`: field names, None first. `remapChannelStatus`: 0x90→0x94 at ch5, 0xF8 and 0xF0 untouched. `remapMidiChannel` on a real `MidiBuffer` at ch1/ch16. |
| Vitest | `formatDb`: 0, 1, 0.5 (−6.0), 0.0001. Native: `settings` present iff `getAudioSettings` is registered. Dialog: lists rendered; a device change re-queries and resets rate/buffer; Apply sends the exact request and closes on ok; an error token shows `role=alert` and keeps the dialog open; Cancel/Escape never call apply; the level slider dispatches `setMasterLevel` and the readout updates. App: Settings absent without `settings`, present and opening with it. |
| Manual gate | WASAPI/ASIO device and rate/buffer switch while playing; port swap mid-note (no hung note, CC123 last on the old port); channel 5 on a monitor; None; relaunch restores; unplugged saved port → first port; corrupt `settings.xml`; plugin hides Settings and stays on ch1; export stays on ch1. |

## Threat Matrix

N/A: no routing, shell, subprocess, VCS/PR automation, executable-file classification or process-integration boundary. Device enumeration and file I/O are in-process JUCE APIs.

## Work Units and Size Forecast

| PR | Commit | Content | ~Lines |
|---|---|---|---|
| A | 1 | Pure `StandaloneSettings` + tests + jucer | 330 |
| A | 2 | Channel remap + sink reopen/channel + tests | 130 |
| A | 3 | `StandaloneHost`, `MainComponent` host/restore/save, factory/WebEditor registration | 215 |
| B | 4 | Bridge types, detection, mock + tests | 160 |
| B | 5 | `db`, `SettingsDialog`, TopBar/App, CSS + tests | 365 |

The total is about 1,200 authored lines. That is well over the 800 budget, so the proposal's 650–780 estimate is not realistic. **Split: PR A (C++, ~675) → PR B (React, ~525).** PR A is autonomous: the functions are registered but not called yet, the button stays disabled, and launch restore works from a hand-edited file.

## Migration / Rollout

No migration is required. The first launch has no file, so it uses today's defaults. `BERLIN_WEB_UI` stays OFF.

## Open Questions

- [ ] Optional `getAudioSettings` query argument and the `""` id for None need to be reconciled with the spec.
- [ ] ASIO: `createDevice` for a second driver while one is open may fail. The fallback is to return empty lists → `device-unavailable` on apply.
