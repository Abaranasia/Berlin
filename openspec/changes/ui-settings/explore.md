# Exploration: ui-settings (UI Redesign Slice 4/6)

Mirror of Engram `sdd/ui-settings/explore`.

## Current State

- **Audio device**: `MainComponent` calls `setAudioChannels(2, 2)` with no saved state (2 unused inputs). Nothing is persisted (no `PropertiesFile`/`ApplicationProperties`/`createStateXml` in `Source/`). No legacy device UI.
- **MIDI output**: `MidiOutputSink::openFirstAvailableDevice()` once in the `MainComponent` ctor. No port list, no `None`, no reopen path. Lifecycle contract: open/close only with audio provably stopped.
- **MIDI channel**: `kMidiChannel = 1` (`Source/midi/MidiChannel.h`) feeds the processor-owned const `MidiEventTranslator` (audio thread, shared with the plugin), `MidiFileWriter` (export), and `MidiOutputSink::channel` (CC123 panic only).
- **Volume**: master level already exists end to end (`setMasterLevel`, `Snapshot.masterLevel`, `Transport.tsx` "Master level" slider in TopBar). It is `patch.outputLevel`, preset-persisted, and applies in the plugin too.
- **Bridge**: `WebEditor` registers `dispatch`, `getSnapshot`, `chooseExportFile`, `confirm`; events `playhead`/`snapshot` at 30 Hz. JS detects the host via `__juce__functions.includes('dispatch')`. No `isStandalone` signal. `WebEditor` is built by `createBerlinEditor(processor)` for both standalone and plugin and has no access to `AudioDeviceManager`.
- **Tests**: Vitest (`ui/src/testing/harness.tsx`, `bridge/native.test.ts` with real interop + mocked host). `Tests/BerlinTests.jucer` is headless juce_core-only; JUCE-aware glue is manual-gate only.

## Approaches

1. **Custom React dialog fed by C++-enumerated devices (recommended)**: standalone-only native functions (`getAudioSettings`, `applyAudioSettings`) registered only when `WebEditor` gets a non-null `StandaloneHost*`; JS derives `isStandalone` by capability detection. Matches mockup, testable. ~700-900 lines.
2. **Embed JUCE `AudioDeviceSelectorComponent`** in a native window: less code (~250-350), but off-style and no MIDI port/channel UI as designed.
3. **Hybrid**: rejected (split UX, same plumbing).

## Decisions for the proposal

- Output level: reuse preset-persisted `masterLevel` vs new app-level gain.
- MIDI channel scope: standalone-only live channel (plugin and export stay on 1) vs global.
- Apply semantics: transactional validate-then-apply, atomic reject.
- Persistence: one versioned ValueTree/XML in `userApplicationDataDirectory` with `createStateXml`, MIDI port id/name, channel, schema version.

## Risks

- MIDI port swap vs the sink lifecycle contract (bracket with `closeAudioDevice`/`restartLastAudioDevice`, keep CC123-last ordering).
- Audio-thread safety for channel remap or any new gain stage (pre-sized buffers, atomics, smoothing).
- Async native callbacks after `~WebEditor` (reuse `shuttingDown` + `SafePointer`).
- Device edge cases: no devices, unplugged, saved device missing, unsupported rate/buffer.
- Spec deltas: `web-ui-layout` "Settings Placeholder"; `midi-device-selection` "No Device-Selection UI" and "Fixed channel, not persisted".
- Budget: 700-900 lines vs 800; may need 2 stacked PRs.
