# Proposal: Settings Dialog (UI Redesign Slice 4/6)

## Intent

The standalone app cannot pick an audio device, a MIDI output port, or a MIDI channel, and it forgets everything at relaunch. It always opens the first MIDI port on channel 1. The TopBar "Settings" button is a disabled placeholder. This slice adds a working Settings dialog (mockup `Settings.dc.html`) for the standalone app only.

## Scope

### In Scope
- React Settings dialog: audio device type/device, sample rate, buffer size; MIDI output port (including `None`); MIDI channel 1-16; output level (the existing `masterLevel`) with a dB readout. Cancel/Apply.
- Standalone-only native functions (`getAudioSettings`, `applyAudioSettings`), registered only when `WebEditor` gets a non-null `StandaloneHost*`.
- MIDI port reopen path in `MidiOutputSink`, plus a standalone live channel for MIDI output.
- One versioned settings file under `userApplicationDataDirectory`, restored at launch.

### Out of Scope
- A new gain stage or app-level volume. The plugin's device/channel behavior and MIDI export (both stay on channel 1).
- Audio input configuration, MIDI input, hot-plug auto-refresh (the dialog re-enumerates when it opens). Legacy editor removal (Slice 5).

## Capabilities

### New Capabilities
- `standalone-settings`: dialog contents, capability detection, transactional apply, error tokens, persistence and launch restore with fallback.

### Modified Capabilities
- `web-ui-layout`: "Settings Placeholder" becomes an enabled Settings button in standalone and a hidden one in the plugin.
- `midi-device-selection`: "Auto-Open First Available Device" becomes "restore saved port, else first available, or None". "Fixed Output Channel Constant" becomes a standalone channel that the user picks and that persists. Reopening a port keeps the ordered close sequence and message-thread confinement.
- `ui-bridge`: adds the standalone-only functions and capability-based detection.

## Decisions

| # | Decision |
|---|---|
| D1 | Output level reuses the preset-persisted `masterLevel`. The TopBar slider stays, and the dialog shows the same value. (user) |
| D2 | Custom React dialog fed by devices enumerated in C++. JS treats the app as standalone when `getAudioSettings` exists. (user) |
| D3 | The channel is a standalone-only live setting. The const `MidiEventTranslator`, the plugin and MIDI export stay on channel 1. |
| D4 | Apply is transactional: validate everything first, then apply it all at once, or reject with an error token and change nothing. |
| D5 | One versioned file stores the audio state (`createStateXml`), the MIDI port (identifier + name, or None) and the channel. A missing device or port falls back to defaults. |
| D6 | The Settings button is shown only in standalone. |

## Approach

`MainComponent` implements a small `StandaloneHost` interface and passes it to `createBerlinEditor`. The plugin passes `nullptr`. A port swap is bracketed by stopping and restarting audio, so the sink lifecycle contract holds. The channel is applied on the sink side through an atomic.

## Affected Areas

| Area | Impact |
|---|---|
| `Source/ui/WebEditor.*`, `Source/MainComponent.*` | Host interface, function registration |
| `Source/midi/MidiOutputSink.*` | Reopen, channel |
| `Source/` (new settings store) | Persistence |
| `ui/src/components/Settings*`, `TopBar`, `bridge/native.ts` + tests | Dialog, detection |

## Delivery forecast

About 650-780 lines (C++ ~280, React ~220, tests ~200, specs ~80). That is under 800 but tight, because reusing `masterLevel` removed ~100. Fallback split: PR A has the C++ host, sink and persistence. PR B has the dialog.

## Risks

| Risk | Likelihood | Mitigation |
|---|---|---|
| Port swap breaks the CC123-last ordering | Med | Close/reopen with audio stopped, on the message thread only |
| Async callbacks after `~WebEditor` | Med | Reuse `shuttingDown` + `SafePointer` |
| Saved device missing or rate unsupported | Med | Validate, then fall back to defaults with an error token |
| JUCE glue can't be headless-tested | Med | Pure validation/serialization in `BerlinTests`, plus a manual gate |

## Rollback Plan

Revert the slice PR(s). The settings file is ignored by older builds, and `BERLIN_WEB_UI` stays OFF by default.

## Success Criteria

- [ ] Standalone: the chosen device, port and channel apply, and they survive a relaunch. A missing device or port falls back without a crash.
- [ ] An invalid Apply changes nothing and shows an error.
- [ ] The plugin hides Settings, and its MIDI and export stay on channel 1.
- [ ] Vitest and `BerlinTests` pass.

## Proposal question round

Auto mode. Assumptions for review: the dialog has no hot-plug refresh; a port of `None` is persisted as an explicit choice.
