# UI Redesign Plan — React in JUCE WebBrowserComponent

Status: planned (2026-10-02). Replaces Slice 5 ("synth-optional UI") of the Berlin School
authenticity initiative and absorbs most of Slice 4 ("MIDI device/channel config").

Mockup: https://claude.ai/artifact/2btudrMwb5C2PBJgfwSp2P (Main, Synth open, Settings, Knob).

## Goals

1. Preset load/save at the top of the screen.
2. Internal synth (oscillator, filter, envelope, LFO, delay, reverb) in a collapsible panel.
3. The sequencer engine and its options are the main focus (live step view, generation, rhythm, pitch, evolution).
4. A standalone-only Audio & MIDI settings dialog, including master volume.
5. A layout that adapts to the window size; fixes the standalone clipping (`MainComponent` fixed at 800x680).

## Non-goals

- Host automation / `AudioProcessorParameter`s. Berlin has none today, so JUCE's
  `Web*ParameterAttachment` classes do not apply. Adding parameters later is a separate change.
- Host tempo sync (the plugin free-runs at its own BPM today).
- New engine features beyond what the UI needs (transport, playhead, volume, MIDI config).

## Key technical decisions

| Decision | Choice | Why |
|---|---|---|
| UI stack | React + TypeScript + Vite, under `ui/` | The user is experienced with it. CSS covers the layout and visuals cheaply. Testable with Vitest/Playwright. |
| Host | `juce::WebBrowserComponent`, `Backend::webview2`, `JUCE_USE_WIN_WEBVIEW2=1` | JUCE 9 provides native integration (`withNativeFunction`, `withEventListener`, `emitEventIfBrowserIsVisible`, `withResourceProvider`). The IE backend is the Windows default and is unusable. |
| Bridge | Plain native functions (UI to engine) + events (engine to UI); no parameter relays | No `RangedAudioParameter`s exist. A small typed command/snapshot protocol is simpler and testable. |
| Bridge logic | Pure C++ `UiBridge` (command dispatch + state snapshot as `juce::var`) separate from the WebView | Testable headlessly in `BerlinTests`. The WebView editor becomes a thin adapter (hexagonal port/adapter). |
| Assets | Release: built `ui/dist` embedded in the binary, served via `withResourceProvider`. Dev: load the Vite dev server URL for hot reload (compile-time flag) | No network in release builds. Fast iteration in development. |
| Embedding | Node script generates `Source/ui/generated/WebAssets.h/.cpp`, run as a pre-build step | Projucer `BinaryData` only regenerates on a Projucer re-save, not at build time. |
| Playhead | GUI `Timer` (~30 Hz) reads relaxed atomics, emits an event only on change | Never touches the audio thread. `SequencePlayer::getPlayheadStep()` is already atomic. |
| Dialogs | File chooser and overwrite prompt stay native (C++) | Already owned by the editor today; native file access stays outside the web view. |
| Migration | New `WebEditor` behind a compile flag; the old editor stays until parity, then is removed | Always-shippable app during the migration. |

## Slices

Each slice is its own SDD change (explore → archive). The dependency order is
0 → 1 → 2 → 3 → 5, and slice 4 can start once slice 2 is in.

### Slice 0 — Engine API for the UI (C++ only)

The UI needs state that the processor does not expose today. Pure C++, strict TDD, headless tests.

- `getPlayheadStep()` / `getLoopCount()` passthroughs on `BerlinAudioProcessor` (`player` is private today).
- Transport: `setPlaying(bool)` / `isPlaying()`. Playback always runs today (ctor calls `player.start()`, nothing stops it). Verify the thread model of `SequencePlayer::start/stop` before wiring it.
- Getters for `synthEnabled` / `effectsEnabled` (setter-only today).
- Live master output level: real-time-safe (atomic target + smoothing in the render path). `SynthPatch::outputLevel` exists but has no live setter.
- `UiBridge`: pure command dispatcher (name + args → processor call, with input validation) and `snapshot()` → `juce::var` (patch, generation params, seed, BPM, transport, toggles, sequence steps, preset names). Unit-tested for every command, including invalid input.
- Done when: all new API covered by `BerlinTests`; the old editor is untouched.
- Size: medium (~400–600 lines incl. tests).

### Slice 1 — WebView proof of concept (go/no-go gate)

Small end-to-end path to de-risk the stack before the full rewrite.

- Enable `JUCE_USE_WIN_WEBVIEW2=1` in `Berlin.jucer` and `Plugin/BerlinPlugin.jucer` (Projucer adds the WebView2 NuGet package); `Backend::webview2`; user data folder under `userApplicationDataDirectory/Berlin/WebView2` (plugins can run from read-only host folders).
- `ui/` Vite + React + TS scaffold; ship JUCE's `native/javascript/index.js` frontend library into the bundle.
- Asset embedding script + pre-build step; resource provider serving the embedded files.
- `WebEditor` (new `AudioProcessorEditor`) behind a compile flag: BPM display with −/+ (native function → `UiBridge`) and a 16-step playhead row (event).
- Check:
  - **Hosting:** standalone and one Windows DAW.
  - **Runtime behaviour:** two plugin instances at once, DPI scaling, keyboard focus, and editor close/reopen.
  - **Missing WebView2 runtime:** a native fallback message.
- Done when: all checks pass. Otherwise stop and revisit the decision (plain JUCE fallback).
- Size: medium (~300–500 lines, excluding generated assets and lockfiles).

### Slice 2 — Full bridge + functional parity

Every control the old editor has, working through the bridge. Plain, unstyled layout.

- Typed protocol in `ui/src/bridge/` (commands, snapshot, events), mirroring `UiBridge`.
- React state: a store fed by snapshot events (manual commands do not `sendChangeMessage()` today, so each command returns a fresh snapshot); `changeListenerCallback` (auto-evolve, state restore) pushes a snapshot event.
- Mock bridge for browser development and tests (`npm run dev` works without JUCE).
- Native file chooser + overwrite prompt invoked from UI commands (export MIDI, save preset).
- Tests: Vitest for store/protocol, C++ tests for any new dispatcher paths.
- Done when: every old-editor action works in the `WebEditor`.
- Size: large, likely over 800 lines → plan chained PRs (protocol + store, then controls).

### Slice 3 — Layout & visual style (the mockup)

- Top bar: wordmark, Play/Stop, BPM, preset picker + Load/Save, Export MIDI, settings (standalone only), Synth toggle.
- Main area: step view with playhead, Generation, Rhythm, Pitch, Evolution, delay-times card (click to copy).
- Collapsible synth panel: oscillator, filter & envelope, LFO, delay, reverb, Sound/FX switches.
- `Knob` component: pointer drag, mouse wheel, double-click to reset, keyboard arrows, ARIA slider semantics.
- Resizable editor with size limits; standalone `MainComponent` sized from the editor (fixes the clipping).
- Theme tokens (dark ground, amber `#F0A238`, IBM Plex Sans Condensed + Plex Mono, bundled locally, no font CDN).
- Tests: Playwright smoke + accessibility checks against the mock bridge.
- Size: large → chained PRs (shell + layout, then components).

### Slice 4 — Standalone Audio & MIDI settings

Absorbs the initiative's MIDI device/channel slice.

- C++:
  - **MIDI output:** make the channel configurable (it's `const` today); list output devices and select one (only `openFirstAvailableDevice()` exists).
  - **Audio device:** expose the `AudioDeviceManager` device, sample rate and buffer size.
  - **Persistence:** save the settings (app properties, `AudioDeviceManager::createStateXml`).
- Bridge: `isStandalone` capability flag in the init data; the plugin hides the settings button.
- React dialog per the mockup, including master volume (from Slice 0).
- Design decision to settle: device selection in React (consistent look, more bridge work) vs JUCE's native `AudioDeviceSelectorComponent` in a dialog window (less work, different look).
- Size: medium–large.

### Slice 5 — Remove the legacy editor

- Delete the old `BerlinAudioProcessorEditor` widget code and the compile flag; `WebEditor` becomes the editor.
- Update main specs (`plugin-host-integration`, `internal-synth-output`, etc.) and `AGENTS.md`/skills if UI conventions change.
- Size: small (mostly deletions).

## Cross-cutting risks

- **WebView2 runtime**: present on Windows 11, not guaranteed elsewhere → fallback message (Slice 1).
- **Plugin instances**: each opens a web view (memory); check in Slice 1.
- **DAW quirks**: keyboard focus and DPI scaling vary by host; test at least one DAW per slice that touches the editor.
- **Build complexity**: Node becomes a build dependency; the pre-build step must fail loudly if `ui/` build fails.
- **Thread safety**: native functions run on the message thread; the audio thread is reached only through existing atomics/handoffs. No new audio-thread work except the volume smoothing (Slice 0).
- **Security**: release builds serve only embedded assets (no remote URLs); every native function validates and clamps its input.
- **Persistence of UI-only state** (synth panel open/closed): decide in Slice 3 (editor-side property vs preset field).

## Testing strategy

| Layer | Tooling | Covers |
|---|---|---|
| Engine + bridge logic | `BerlinTests` (juce::UnitTest, headless) | Slice 0 API, `UiBridge` commands/snapshots, validation |
| UI logic | Vitest | Store, protocol, formatting (e.g. delay ms) |
| UI behaviour | Playwright against the mock bridge | Layout, collapsible panel, knobs, dialog, accessibility |
| Integration | Manual: standalone + one DAW | WebView hosting, playhead, presets, export, settings |
