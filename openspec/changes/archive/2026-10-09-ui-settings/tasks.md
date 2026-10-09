# Tasks: Settings Dialog (UI Redesign Slice 4/6)

Strict TDD is active: every behavior task is RED (write the failing test, run it, confirm it fails for the right reason), then GREEN (minimal code), then run the suite. Package manager is pnpm only (`pnpm --dir ui ...`); npm is broken, never use it. MSBuild: `C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe`. Projucer: `H:\Proyectos\Juce\Projucer\Projucer.exe --resave <jucer>`. C++ tests: `Tests/Builds/VisualStudio2026/x64/Debug/ConsoleApp/BerlinTests.exe --category=Berlin`. Skills to load when applying: `juce-app-dev` (message-thread-only device lifecycle, audio thread no alloc/locks, one versioned state tree), `juce-app-testing`, `work-unit-commits` (paths in `.atl/skill-registry.md`).

Authoritative inputs: `proposal.md`, `design.md` (D1-D11), `specs/*/spec.md`, Engram `sdd/ui-settings/reconcile` (id 402). Engram spec id 400 is stale. Note: BerlinTests links juce_audio_basics, juce_data_structures, juce_events and juce_audio_processors_headless (not juce_core only); `StandaloneSettings` may use `juce::XmlElement`, `juce::var` and `juce::AudioDeviceManager`-free types only (no `juce_audio_devices`).

Verification legend (from the specs): [V] Vitest/jsdom, [T] BerlinTests, [M] manual.

## Review Workload Forecast

| Field | Value |
|---|---|
| Estimated authored lines | ~1,200 (code + tests; excludes delta specs, lockfile, generated `dist`/embedded assets) |
| Review budget | 800 authored lines per PR |
| Delivery strategy | single-pr-default |
| Chain strategy | not yet chosen (orchestrator decides) |
| 400-line budget risk | High (PR A ~675 and PR B ~525 both exceed 400) |
| 800-line budget risk | High as a single PR (~1,200, about 50% over; the proposal's 650-780 estimate omitted glue and tests); Low per PR if split (A ~675, B ~525) |
| Chained PRs recommended | Yes (split A -> B) |
| Decision needed before apply | Yes (choose the A/B split, or a single PR with `size:exception`) |

Split boundary (usable if the orchestrator chooses a split):

| PR | Commits | Content | Est. lines | Rollback |
|---|---|---|---|---|
| PR A | 1-3 | C++: pure `StandaloneSettings` + tests + jucer; channel remap + sink reopen/channel + tests; `StandaloneHost`, `MainComponent` host/restore/save, factory/WebEditor registration | ~675 (330 + 130 + 215) | Revert A: standalone returns to first-port/ch1 with no persistence; commit 1 and 2 are revertable alone |
| ===== SPLIT BOUNDARY ===== | | PR B targets PR A's branch if chained | | |
| PR B | 4-5 | Bridge types/detection/mock + tests; `db`, `SettingsDialog`, TopBar/App, CSS + tests | ~525 (160 + 365) | Revert B: Settings button returns to the disabled placeholder; C++ functions stay registered but unused |

Note: PR A is autonomous. The native functions are registered but nothing calls them yet, the button stays the disabled placeholder, and launch restore works from a hand-edited `settings.xml`. No intermediate state is broken.

## Phase 0: Baseline

- [x] 0.1 `git status` and `git branch --show-current`: confirm branch `feat/ui-settings`; verify git state yourself (`?? openspec/changes/ui-settings/` is expected).
- [x] 0.2 `pnpm --dir ui test`; record the Vitest count (baseline: 462 tests expected after Slice 3, confirm).
- [x] 0.3 Build `Builds/VisualStudio2026/Berlin.sln` and `Tests/Builds/VisualStudio2026/BerlinTests.sln` (Debug x64, flag off); run `BerlinTests.exe --category=Berlin`; record the pass count (baseline 379 expected, confirm).

## Phase 1: Pure settings unit (commit 1, ~330 lines, PR A, strict TDD)

Files: `Source/settings/StandaloneSettings.{h,cpp}`, `Tests/Source/StandaloneSettingsTests.cpp`, `Berlin.jucer`, `Tests/BerlinTests.jucer`. Specs: standalone-settings "Transactional Apply", "Persistence File", "Launch Restore And Fallback"; ui-bridge "Standalone-Only Settings Functions" (payload shape). Design D3, D7, D8, D9.

- [x] 1.1 Add `StandaloneSettings.{h,cpp}` (empty declarations/stubs only) and `StandaloneSettingsTests.cpp` to both `.jucer` files; run Projucer `--resave` for `Berlin.jucer` and `Tests/BerlinTests.jucer`; confirm both solutions still compile. Check the new files are in `Tests/BerlinTests.jucer` with the existing test group conventions.
- [x] 1.2 RED `parseApplyRequest`: valid request; missing field; wrong type; channel 0 and 17; rate <= 0 each yield `invalid-request`. Confirm fails (missing implementation). Satisfies: Channel out of range [T]; Each token is reported [T] (bad shape).
- [x] 1.3 GREEN `parseApplyRequest` (returns request struct or `invalid-request`).
- [x] 1.4 RED `validateApplyRequest(request, DeviceOptions)`: one test per token (`device-unavailable`, `sample-rate-unsupported`, `buffer-size-unsupported`, `midi-port-unavailable`) and one proving the check order device, rate, buffer, port (a request violating several returns the first); `""` port id (None) is always valid; valid request returns `""`. `busy` is owned by the host guard (1.2 of Phase 3), not this function. Satisfies: Rejected apply changes nothing [T]; Each token is reported [T].
- [x] 1.5 GREEN `validateApplyRequest` and the `DeviceOptions` plain-array struct.
- [x] 1.6 RED XML: `settingsToXml`/`settingsFromXml` round trip (audio state element, port id + name, channel 5, root `BerlinSettings` `version="1"`); None serializes `none="1"` with no `id`; missing/empty, non-XML, wrong root, `version="2"`, channel 99 all yield defaults (channel 1, no audio state, default port choice), no exception. Satisfies: Round trip [T]; None serialized explicitly [T]; Missing file [T]; Corrupt or unknown version [T]; Saved channel invalid [T].
- [x] 1.7 GREEN `settingsToXml`/`settingsFromXml` (D7).
- [x] 1.8 RED `resolveMidiPort(saved, ports)`, all 5 branches: saved None stays None with ports available; id found; id missing but name found returns that id; neither found returns first port; zero ports returns None. Satisfies: Saved port present [T]; Saved port missing [T]; Saved None stays None [T].
- [x] 1.9 GREEN `resolveMidiPort` (D8).
- [x] 1.10 RED `audioSettingsToVar` + `okResult`/`errorResult`: field names match `AudioSettings` in design (`deviceTypes, deviceType, devices, device, sampleRates, sampleRate, bufferSizes, bufferSize, midiPorts, midiPortId, midiChannel`); `midiPorts[0]` is `{id:"",name:"None"}`; no volume field; `okResult` is `{ok:true}` with no `snapshot`; `errorResult(token)` is `{ok:false,error:token}`. Satisfies: Get returns lists and no volume (native shape); Apply success shape.
- [x] 1.11 GREEN `audioSettingsToVar`, `okResult`, `errorResult`.
- [x] 1.12 RED then GREEN: write-failure tolerance of the file write helper (if the pure unit exposes one; otherwise covered in task 3.5 and 9.x). Satisfies: Write failure tolerated [T] (non-writable directory returns false, no throw).
- [x] 1.13 Run `BerlinTests.exe --category=Berlin`; all green, count = baseline + new. Commit 1: `feat: add pure standalone settings parsing, validation and persistence`.

## Phase 2: Channel remap and sink reopen (commit 2, ~130 lines, PR A, strict TDD)

Files: `Source/midi/MidiMessageBytes.h`, `Source/midi/MidiChannelRemap.h`, `Source/midi/MidiOutputSink.{h,cpp}`, `Tests/Source/MidiChannelRemapTests.cpp`, `Tests/BerlinTests.jucer`. Specs: midi-device-selection "Standalone Output Channel", "Ordered Close Sequence", "Device Lifecycle Confined to the Message Thread". Design D5, D6.

- [x] 2.1 RED `remapChannelStatus` (constexpr): 0x90 at ch5 -> 0x94; 0x80-0xEF all remapped; 0xF8 and 0xF0 untouched; ch1 is identity. Satisfies: Events remapped to chosen channel [T]; Default channel [T]. Confirm fails.
- [x] 2.2 GREEN `remapChannelStatus` in `MidiMessageBytes.h`.
- [x] 2.3 RED `remapMidiChannel(juce::MidiBuffer&, int)` on a real `MidiBuffer`: note-on at ch1 -> ch5 and ch16; ch1 target is a no-op; SysEx and realtime bytes pass through; no extra allocation path (rewrite in place). Register `MidiChannelRemapTests.cpp` in `Tests/BerlinTests.jucer`. Confirm fails.
- [x] 2.4 GREEN `MidiChannelRemap.h` (header-only, writes through the iterator data pointer, documented `const_cast`, D6).
- [x] 2.5 `MidiOutputSink`: add `std::atomic<int> channel` (relaxed, default 1) and `setChannel(ch)` (sends CC123 on the old channel after `clearAllPendingMessages`, then stores); `dispatch(MidiBuffer&)` and `sendImmediately` call `remapMidiChannel` when channel != 1; CC123 on close uses the current channel; `openDevice(id)` closes the old device via the ordered sequence first and treats `""` as None; remove `openFirstAvailableDevice` (one caller, replaced in Phase 3). If a sink seam exists without hardware, add a test for panic-uses-current-channel [T]; otherwise it is covered by the remap tests plus [M] 9.5. Satisfies: Panic uses current channel [T]; Port switch closes old before opening new; Switch to None closes.
- [x] 2.6 Confirm the audio-thread path contains only a null/closed check and an atomic read (no lock, no allocation) by review. Satisfies: Audio callback never opens or closes a device.
- [x] 2.7 Confirm `MidiEventTranslator` and MIDI export still emit channel 1 (existing tests stay green). Satisfies: Export and plugin unaffected [T].
- [x] 2.8 Build standalone and tests; `BerlinTests.exe --category=Berlin` green. Commit 2: `feat: add standalone MIDI output channel remap and sink port reopen`.

## Phase 3: Host, MainComponent, factory, WebEditor (commit 3, ~215 lines, PR A, no headless seam, manual verification in Phase 9)

Files: `Source/ui/StandaloneHost.h`, `Source/ui/EditorFactory.{h,cpp}`, `Source/ui/WebEditor.{h,cpp}`, `Source/MainComponent.{h,cpp}`. Specs: ui-bridge "Standalone-Only Settings Functions"; standalone-settings "Transactional Apply", "Persistence File", "Launch Restore And Fallback"; midi-device-selection "Auto-Open First Available Device At Startup". Design D1, D2, D4, D8, D9. JUCE glue has no headless seam (juce_audio_devices); logic that can be pure was moved into Phase 1.

- [x] 3.1 Create `StandaloneHost.h` (abstract `getAudioSettings(const var&)` / `applyAudioSettings(const var&)` returning `juce::var`, D1). `createBerlinEditor(processor, StandaloneHost* host = nullptr)` signature in `EditorFactory.{h,cpp}`; legacy editor ignores it (flag off).
- [x] 3.2 `WebEditor.{h,cpp}`: store `StandaloneHost*`; register `getAudioSettings` and `applyAudioSettings` native functions only when non-null; both run on the message thread, return `busy` when `shuttingDown`, and never call back into JS after destruction (reuse the existing `shuttingDown` + `SafePointer` pattern). Satisfies: Registered only with a host; Plugin registers no settings functions [M]; Callback after teardown [M].
- [x] 3.3 `MainComponent.h/.cpp` member order: `processor, midiSink, settings state, editor, midiBlock` so the editor is destroyed before anything the host uses (D2); implement `StandaloneHost`; pass `this` to `createBerlinEditor`.
- [x] 3.4 `MainComponent::getAudioSettings(query)`: `scanForDevices()` on each call, gather `DeviceOptions` (for the query device use a temporary `createDevice` that is never opened; on failure return empty lists), MIDI ports with `{id:"",name:"None"}` first, then `audioSettingsToVar` (D9). No volume field.
- [x] 3.5 `MainComponent::applyAudioSettings(request)` per D4: guard `applying` (return `busy`); `parseApplyRequest`; gather options; `validateApplyRequest`; `closeAudioDevice()`; MIDI swap (`openDevice`, `setChannel`); audio `setCurrentAudioDeviceType` + `setAudioDeviceSetup(setup,true)` if changed else `restartLastAudioDevice()`; on post-validation open failure, best-effort rollback of port, channel, type and setup, do not write the file, return `device-unavailable` or `midi-port-unavailable`; on success `settingsToXml` and `XmlElement::writeTo(File)` to `userApplicationDataDirectory/Berlin/settings.xml`; a failed write still returns `{ok:true}` (D7).
- [x] 3.6 Launch restore (D8): load file (missing/corrupt/unknown version -> defaults), `setChannel(saved)`, `openDevice(resolveMidiPort(saved, ports))`, `setAudioChannels(2, 2, savedAudio.get())` with `selectDefaultDeviceOnFailure=true`; replaces the `openFirstAvailableDevice` call. Satisfies: Saved port available/missing/None at startup; No device is available at startup.
- [x] 3.7 Build standalone and plugin (`Plugin/Builds/VisualStudio2026/Berlin.sln`), flag ON and flag OFF, Debug x64; zero errors; `BerlinTests.exe --category=Berlin` green. Behavior check deferred to Phase 9. Commit 3: `feat: add standalone host with settings apply and launch restore`.

SPLIT BOUNDARY: PR A = commits 1-3 (Phases 1-3). PR B = commits 4-5 (Phases 4-5).

## Phase 4: Bridge types, detection, mock (commit 4, ~160 lines, PR B, strict TDD)

Files: `ui/src/bridge/{protocol,bridge,native,mock}.ts`, `ui/src/bridge/native.test.ts`. Specs: ui-bridge "Standalone-Only Settings Functions", "Capability-Based Standalone Detection"; standalone-settings "Standalone Capability Detection". Design D10.

- [x] 4.1 RED `native.test.ts`: `bridge.settings` present iff `__juce__functions` includes `getAudioSettings`; absent when the list has only `dispatch`; absent and no throw when `__juce__functions` is undefined; `get(query?)` forwards the optional `{deviceType, device}` and returns the payload unchanged (no volume field); `apply(req)` returns `{ok:true}` with no `snapshot` key and exposes `{ok:false,error}` tokens unchanged; the dispatch command list is still exactly the 15 existing commands. Satisfies: Standalone detected [V]; Plugin not standalone [V]; Detection true/false/No host [V]; Get returns lists and no volume [V]; Apply success/failure shape [V]; Dispatch surface unchanged [V]. Confirm fails.
- [x] 4.2 GREEN `protocol.ts` types (`SettingsError`, `AudioSettings`, `ApplyAudioSettingsRequest`, `ApplyAudioSettingsResult`, `SettingsBridge`), `bridge.ts` optional `settings?: SettingsBridge`, `native.ts` capability detection (D10).
- [x] 4.3 `mock.ts`: `createMockBridge({standalone})` provides a mock `settings` (lists with None first, configurable apply result); the mock chunk must stay out of the production build (existing `assertNoMockChunk`).
- [x] 4.4 `pnpm --dir ui test`, `typecheck`, `build` (no mock chunk). Commit 4: `feat: add standalone settings bridge types, detection and mock`.

## Phase 5: db, SettingsDialog, TopBar/App, CSS (commit 5, ~365 lines, PR B, strict TDD)

Files: `ui/src/lib/db.ts` + `db.test.ts`, `ui/src/components/SettingsDialog.tsx` + `SettingsDialog.test.tsx`, `ui/src/components/TopBar.tsx`, `ui/src/App.tsx` + `App.test.tsx`, `ui/src/main.tsx`, `ui/src/styles/components.css`. Specs: standalone-settings "Dialog Contents And Lifecycle", "Output Level Reuses Master Level", "Transactional Apply"; web-ui-layout "Settings Button". Design D10, D11.

- [x] 5.1 RED `db.test.ts`: `formatDb(0)` = `−∞ dB` (U+2212), `formatDb(1)` = `0.0 dB`, `formatDb(0.5)` = `-6.0 dB`, `formatDb(0.0001)` = `-80.0 dB`; negative level also `−∞ dB`. Satisfies: dB readout [V]; Zero level [V]. Confirm fails. Note: the spec writes `-6.0 dB` with an ASCII minus and `−∞ dB` with U+2212; match the spec text exactly in the assertion.
- [x] 5.2 GREEN `db.ts`.
- [x] 5.3 RED `SettingsDialog.test.tsx`: renders `role="dialog" aria-modal` with device type, device, rate, buffer, port (None option present), channel (16 entries), level slider with readout; fields populated from `get()`; changing the device re-queries `get({deviceType,device})` and resets rate/buffer to that device's options; Apply sends the exact request and closes on `{ok:true}`; `{ok:false,error}` keeps the dialog open, shows a `role="alert"` and keeps the edits; Cancel, Close and Escape never call `apply`; a failing `get` shows an error status and no form; reopening calls `get` again and shows new lists; the level slider dispatches `setMasterLevel` through `store.send('masterLevel', ...)`, never calls `apply`, and is not reverted by Cancel; level shared with the TopBar slider. Satisfies: Fields populated [V]; None port selectable [V]; Re-listed on every open [V]; Cancel discards [V]; Dependent options follow device [V]; Level change uses dispatch [V]; Shared with TopBar [V]; Valid apply [V]; Error shown, edits kept [V]. Confirm fails.
- [x] 5.4 GREEN `SettingsDialog.tsx` (D10, D11): `onClose` prop; focus handling; Escape handler.
- [x] 5.5 RED `App.test.tsx`: Settings button absent when `bridge.settings` is missing (no element with aria-label "Settings"); present, enabled and opening the dialog with no `dispatch` call when present; Escape closes the dialog and returns focus to the Settings button; update the Slice 3 "Settings disabled placeholder" assertion accordingly (keep the plugin variant). Satisfies: Enabled in standalone [V]; Hidden in plugin [V]; Dialog keyboard access [V]. Confirm fails.
- [x] 5.6 GREEN `TopBar.tsx` (render button only when `onOpenSettings` is provided), `App.tsx` (`onOpenSettings` only when `store.bridge.settings` exists; dialog state), `main.tsx` if wiring is needed.
- [x] 5.7 Dialog and Settings-button CSS in `components.css` using existing tokens only (no raw hex, `styles.test.ts` still passes).
- [x] 5.8 Parity check: Slice 1-3 tests pass unchanged except the intentional placeholder assertion edit in 5.5.
- [x] 5.9 `pnpm --dir ui test`, `typecheck`, `build` (no mock chunk). Commit 5: `feat: add settings dialog with db readout and enabled settings button`.

## Phase 6: Build and test commands (run before each commit and again at the end)

- [x] 6.1 `pnpm --dir ui test` (all Vitest green; compare with the Phase 0.2 baseline plus the new tests).
- [x] 6.2 `pnpm --dir ui typecheck` (tsc, no errors).
- [x] 6.3 `pnpm --dir ui build` with no mock chunk (existing `assertNoMockChunk`).
- [x] 6.4 `BerlinTests.exe --category=Berlin` (baseline plus the new tests).
- [x] 6.5 Flag-ON build, `/t:Rebuild`, Debug x64: standalone (`Builds/VisualStudio2026/Berlin.sln`) and plugin (`Plugin/Builds/VisualStudio2026/Berlin.sln`); `BERLIN_WEB_UI` defined; zero errors. (user 2026-10-09)
- [x] 6.6 Flag-OFF build: same two solutions with `BERLIN_WEB_UI` undefined; zero errors (legacy editor untouched by this slice). (user 2026-10-09)

## Phase 7: Open-question handling

- [x] 7.1 Open question: optional query argument and `""` id for None. Resolved in specs (reconcile id 402); no code task, record in apply-progress.
- [x] 7.2 Open question: ASIO `createDevice` for a second driver while one is open may fail. Implementation follows D9 fallback (empty lists, `device-unavailable` on apply); confirm and record the observed behavior during manual 8.1. (user 2026-10-09; ASIO second-driver observation not recorded separately, user reported all manual checks pass)
- [x] 7.3 Record in apply-progress: a failed settings write is silent (`{ok:true}`), no hot-plug refresh while the dialog is open, level applies live and is not reverted by Cancel.

## Phase 8: Manual verification (all [M] scenarios; record result and date per line)

Run on a flag-ON standalone build (real WASAPI and ASIO devices, at least two MIDI ports, a MIDI monitor) and in Cakewalk Sonar (flag-ON plugin) where noted.

- [x] 8.1 Hardware apply (standalone-settings): switch audio device type WASAPI <-> ASIO and device while playing; audio continues without a crash; note the ASIO second-driver behavior (7.2). (user 2026-10-09)
- [x] 8.2 Rate and buffer change: apply a different sample rate and buffer size while playing; audio continues on the new setup. (user 2026-10-09)
- [x] 8.3 Port switch, None and hot-unplug: apply a different MIDI port mid-note (no hung note; All Notes Off arrives last on the old port; nothing from A reaches B); apply None (no device open, no hung note); unplug the active port then apply/relaunch and confirm no crash. Satisfies: Reopen bracketed by audio stop [M]; Failed reopen degrades silently. (user 2026-10-09)
- [x] 8.4 Channel change: set channel 5 (monitor shows ch5 for all channel-voice messages); change channel mid-note and confirm no hanging notes (CC123 on the old channel before the switch). (user 2026-10-09)
- [x] 8.5 Restart persistence (Relaunch survives [M]): apply device, port and channel, close and relaunch; the same setup is active; a saved None stays None. (user 2026-10-09)
- [x] 8.6 Missing device at relaunch: unplug the saved audio device or port, relaunch; fallback setup, no crash (Missing device at relaunch [M]). (user 2026-10-09)
- [x] 8.7 Corrupt `settings.xml` fallback: replace the file with garbage, with another root and with `version="2"`; each launches with defaults (first port, ch1, default audio) and no crash; `ok:true` still returned when the settings directory is read-only. (user 2026-10-09)
- [x] 8.8 Plugin in Sonar has no Settings button (Plugin registers no settings functions [M]; Hidden in plugin): the flag-ON plugin editor shows no Settings button; its MIDI output stays on channel 1; exported MIDI is on channel 1 (Export and plugin unaffected). (user 2026-10-09)
- [x] 8.9 Callback after teardown [M]: close the standalone window while an apply is in flight; no crash, no JS callback. (user 2026-10-09)
- [x] 8.10 Flag-off build unaffected: flag-OFF standalone launches the legacy editor at 800x952, non-resizable; audio and OS MIDI behave as before (first-port open when no settings file, now restored from `settings.xml` when present); flag-OFF plugin editor unchanged. (user 2026-10-09)
- [x] 8.11 Dialog UX: Escape/Close/Cancel discard edits, Apply error keeps the dialog open with the alert, level slider matches the TopBar slider and shows the dB readout live. (user 2026-10-09)
- [x] 8.12 Record all results and the test counts in the apply-progress; hand off to `sdd-verify`. (user 2026-10-09)

## Spec Scenario Coverage Map

| Spec scenario | Task(s) |
|---|---|
| standalone-settings: Standalone detected [V] | 4.1, 4.2, 5.5, 5.6 |
| standalone-settings: Plugin not standalone [V] | 4.1, 4.2, 5.5 |
| standalone-settings: Plugin registers no settings functions [M] | 3.2, 8.8 |
| standalone-settings: Fields populated from current state [V] | 5.3, 5.4 |
| standalone-settings: None port is selectable [V] | 5.3, 5.4 |
| standalone-settings: Re-listed on every open [V] | 5.3, 5.4 |
| standalone-settings: Cancel discards [V] | 5.3, 5.4 |
| standalone-settings: Dependent options follow device [V] | 5.3, 5.4 |
| standalone-settings: dB readout [V] | 5.1, 5.2, 5.3 |
| standalone-settings: Zero level [V] | 5.1, 5.2 |
| standalone-settings: Level change uses dispatch [V] | 5.3, 5.4 |
| standalone-settings: Shared with TopBar [V] | 5.3, 5.4 |
| standalone-settings: Valid apply [V] | 5.3, 5.4 |
| standalone-settings: Rejected apply changes nothing [T] | 1.4, 1.5, 3.5 |
| standalone-settings: Each token is reported [T] | 1.2, 1.4, 3.5 (`busy` in the host guard, 8.x) |
| standalone-settings: Channel out of range [T] | 1.2, 1.3 |
| standalone-settings: Error shown, edits kept [V] | 5.3, 5.4 |
| standalone-settings: Hardware apply [M] | 8.1, 8.2, 8.3, 8.4 |
| standalone-settings: Round trip [T] | 1.6, 1.7 |
| standalone-settings: None serialized explicitly [T] | 1.6, 1.7 |
| standalone-settings: Write failure tolerated [T] | 1.12, 3.5, 8.7 |
| standalone-settings: Missing file [T] | 1.6, 1.7 |
| standalone-settings: Corrupt or unknown version [T] | 1.6, 1.7, 8.7 |
| standalone-settings: Saved port present [T] | 1.8, 1.9 |
| standalone-settings: Saved port missing [T] | 1.8, 1.9 |
| standalone-settings: Saved None stays None [T] | 1.8, 1.9 |
| standalone-settings: Saved channel invalid [T] | 1.6, 1.7 |
| standalone-settings: Relaunch survives [M] | 3.6, 8.5 |
| standalone-settings: Missing device at relaunch [M] | 3.6, 8.6 |
| ui-bridge: Registered only with a host [T] | 3.2, 3.7 (build + review; no headless seam, see risks), 8.8 |
| ui-bridge: Get returns lists and no volume [V] | 1.10, 1.11, 4.1 |
| ui-bridge: Apply success shape [V] | 1.10, 4.1 |
| ui-bridge: Apply failure shape [V] | 1.10, 4.1 |
| ui-bridge: Dispatch surface unchanged [V] | 4.1 |
| ui-bridge: Callback after teardown [M] | 3.2, 8.9 |
| ui-bridge: Detection true / false / No host [V] | 4.1, 4.2 |
| web-ui-layout: Enabled in standalone [V] | 5.5, 5.6 |
| web-ui-layout: Hidden in plugin [V] | 5.5, 5.6, 8.8 |
| web-ui-layout: Dialog keyboard access [V] | 5.3, 5.5, 5.6 |
| midi-device-selection: Saved port available / missing / None / no device at startup | 1.8, 3.6, 8.5, 8.6 |
| midi-device-selection: Events remapped to chosen channel [T] | 2.1-2.4 |
| midi-device-selection: Default channel [T] | 2.1, 2.5 |
| midi-device-selection: Panic uses current channel [T] | 2.5, 8.4 |
| midi-device-selection: Export and plugin unaffected [T] | 2.7, 8.8 |
| midi-device-selection: Panic guard last on the wire; Repeated close no-ops | 2.5 (existing sink tests stay green) |
| midi-device-selection: Port switch closes old before opening new; Switch to None closes | 2.5, 3.5, 8.3 |
| midi-device-selection: Audio callback never opens or closes a device | 2.6 |
| midi-device-selection: Reopen bracketed by audio stop [M] | 3.5, 8.3 |
| midi-device-selection: Failed reopen degrades silently | 2.5, 3.5, 8.3 |
