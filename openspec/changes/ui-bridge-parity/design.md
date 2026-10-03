# Design: Full Bridge + Functional Parity (UI Redesign Slice 2/6)

## Technical Approach

The host stays thin. `WebEditor` gains a `snapshot` event, which a gui-free `PendingEventGate` gates, and two native dialog functions. `PresetManager` caches per-file validity, and `loadPreset` reports `busy`. `UiBridge` stays pure and its command set doesn't change. The UI is rebuilt on a hand-written protocol, a `Bridge` interface (with native and DEV-only mock implementations) and a dependency-free external store. Plain controls read from the store through selectors. TS orchestrates the dialog flows.

## Architecture Decisions

| # | PR | Choice | Rejected | Rationale |
|---|---|---|---|---|
| D1 | 1 | `WebEditor : juce::ChangeListener`. The constructor calls `addChangeListener`. The destructor calls `removeChangeListener` first, sets `shuttingDown = true`, then runs `stopTimer`, `exportChooser.reset()`, `confirmBox = {}` and `browser.reset()`, in that order. `changeListenerCallback` only calls `gate.markPending()`. In `timerCallback`: `if (gate.shouldEmit(browser && browser->isVisible())) { emitEventIfBrowserIsVisible("snapshot", bridge.snapshot()); gate.commit(); }`. | Emitting from the callback | One emit path; a hidden browser keeps the event pending. |
| D2 | 1 | One `dialogOpen` flag covers both dialogs. `chooseExportFile`: busy resolves to `{cancelled:true,path:"",error:"busy"}`; otherwise it calls `launchAsync(save\|canSelectFiles\|warnAboutOverwriting)` on `std::unique_ptr<juce::FileChooser>`, with `defaultExportFile(userDocumentsDirectory)` = `Documents/Berlin/berlin-export.mid`. `confirm(title,message)`: busy resolves to `false`; otherwise it calls `NativeMessageBox::showScopedAsync(makeOptionsOkCancel(...))` held in a `juce::ScopedMessageBox`. Callbacks capture `SafePointer<WebEditor>` and return without calling `done` when `safe == nullptr \|\| safe->shuttingDown`. | Dialogs in `UiBridge`; SafePointer alone | The SafePointer is only cleared in `~Component`, which runs after the `~WebEditor` body. A synchronous callback during `reset()` would still see it non-null, hence `shuttingDown`. |
| D3 | 1 | Add `PresetResult::busy`. `BerlinAudioProcessor::loadPreset` returns `busy` before `presetManager.load` when `player.isPublishPending()`. It also returns `busy` if `regenerate(false)` (`BerlinAudioProcessor.cpp:260`, result ignored today) returns false. `presetResultToken` maps it to `"busy"`; legacy `describePresetFailure` maps it to `Busy, try again` (the legacy editor's existing busy wording for regenerate/mutate). | Checking only the `regenerate` result | Mirrors the D14 rule (busy is checked at the root, state untouched). Pending can only clear between the check and `regenerate` (only the message thread publishes), so the check is effectively atomic. `setStateInformation:169` also ignores the result: out of scope. |
| D4 | 1 | `PresetManager` keeps a `mutable std::map<juce::String fullPath, Entry{int64 size; juce::Time mtime; std::optional<juce::String> name}>` under a `std::mutex`. Each call lists `*.xml`, reuses entries whose size and mtime match, parses the rest (incrementing `parseCount_`), and replaces the map with the entries it saw, so removed files are pruned. `std::size_t parseCount() const` is the test seam. `save()` needs no invalidation. | Cache in the processor; name cache invalidated on save/load | User-approved D5. Callers today are `UiBridge::snapshot` and the legacy `refreshPresetList`, both on the message thread. The mutex makes the const-method mutation safe if a future caller runs elsewhere. |
| D5 | 1 | Delete `owner`. Add a private `BerlinAudioProcessor& engine() noexcept { return static_cast<BerlinAudioProcessor&>(processor); }`. | Keeping the duplicate | Single reference. |
| D6 | 2 | `ui/src/bridge/protocol.ts` (hand-written): a `Command` union of the 15 `{name,args}` variants (`setPatch` / `setGenerationParams` take `Partial<...>`), `Snapshot`, `DispatchResult`, the event payloads (`PlayheadEvent {step, playing}`, `snapshot` = `Snapshot`), `ChooseExportResult {cancelled, path, error?: "busy"}`, an `ErrorToken` union, enum literal unions (`Waveform`, `LfoDestination`, `DelayDivision`, `RhythmMode`, `ScaleType`; encodings per the `ui-bridge` Encoding Conventions), plus `limits.ts`. Field names come from `openspec/specs/ui-bridge/spec.md`. | Codegen; runtime `getSchema` | Explore §7. |
| D7 | 2 | `Bridge` = `{dispatch, getSnapshot, onPlayhead, onSnapshot, chooseExportFile, confirm}`. `resolveBridge({hasHost, loadMock})` returns `nativeBridge` when a host exists, `await loadMock()` when `loadMock` is defined, and `nativeBridge` (which degrades) otherwise. `main.tsx` passes `loadMock: import.meta.env.DEV ? () => import('./bridge/mock').then(m => m.mockBridge) : undefined`, so the dynamic import is lexically guarded by `import.meta.env.DEV` (a runtime `dev` boolean parameter would NOT let Rollup drop the chunk). `embed-lib.mjs` `assertNoMockChunk(files)` fails the embed if any asset file name matches `/mock/`. | A static import; a runtime `dev` flag inside `resolveBridge` | The build-time `false` drops the chunk; the guard proves it. |
| D8 | 2 | `createStore(bridge)` with slices `engine` (`Snapshot\|null`), `overlay` (optimistic values per key), `playhead`, `status` and `draft`. `send(key, cmd, optimistic)`: at most one request in flight per key, the latest pending replaces the previous one. A global `seq` is attached to each request, and a response snapshot applies only if `seq > lastAppliedSeq` (then `lastAppliedSeq = seq`); a stale response still settles its key. `snapshot` events carry no `seq` (D3 of the proposal): they always update `engine`, but overlays win until their key settles. A key settles when its in-flight request completes and nothing is pending for it; the overlay is then dropped, so the display shows `engine`. On `ok:false` with nothing pending for the key, the store drops the overlay (reverting to the last confirmed `engine` value) and sets status from the token; a newer pending value is still sent and keeps its overlay. Every promise is `.catch`ed into status. Keys: `bpm`, `patch.<field>`, `gen.<field>`, `gen.range` (lo and hi sent together), and one key per toggle. | `useReducer`+Context; Zustand | Explore approach B. |
| D9 | 2 | Drift guard `ui/scripts/limits-drift.test.mjs`: Node `fs` reads `new URL('../../Source/synth/SynthPatch.h', import.meta.url)`, regex-parses the `kMin*/kMax*` values (including bpm) and compares them with `src/bridge/limits.ts`. **Kept, with no build plumbing:** `scripts/**/*.test.mjs` is already in Vitest `include`, and placing it outside `src` avoids `tsc` needing `@types/node`. | Dropping it | Zero cost. |
| D10 | 2 | Add `"dev": "vite"`, `server: {port: 5173, strictPort: true}`, and `src/vite-env.d.ts` (`vite/client`). | — | Matches `BERLIN_WEB_UI_DEV_URL`. |
| D11 | 3/4 | The store comes from context; `App` gets `store` as a prop. Primitives go in `components/controls/` (`Slider` with an optional `skew`, `Select`, `Toggle`, `Button`). Panels: PR3 is `StatusLine`, `Transport` (Play/Stop, BPM, level), `EngineToggles`, `GenerationPanel` (seed, Lock, Generate/Randomize/Mutate), `RhythmPanel`, `PitchPanel`, `EvolutionPanel`, `ExportButton`. PR4 is `SynthPanel`, `DelayPanel`, `ReverbPanel`, `PresetPanel`. | Prop drilling | Selector re-renders. |
| D12 | 2-4 | Pure TS helpers: `status.ts` (`describe(command, token)` per the Status Messages table below, PR2), `seed.ts` (`/^-?\d+$/`, PR3), `skew.ts` (JUCE midpoint skew, PR4), `delayRecs.ts` (60000/bpm × [2, 1, .75, .5, 1/3, .25], rounded, PR4). Delay Sync memory lives in `draft.lastManualDelay`. | Host-computed | No new commands. |
| D13 | 2 | DOM tests use `react-dom/client` + `act` with a fake `Bridge`. Rewrite the Slice 1 `vi.mock('./bridge/native')`. | `@testing-library` | No new dependency. |

## Data Flow

    processor.sendChangeMessage ─▶ changeListenerCallback ─▶ gate.markPending
    Timer 30Hz ─▶ gate.shouldEmit(visible) ─▶ emit("snapshot", bridge.snapshot()) ─▶ gate.commit
    control ─▶ store.send(key) ─overlay─▶ bridge.dispatch ─▶ response(seq) ─fresh?─▶ engine
    Export:  chooseExportFile ─!cancelled─▶ dispatch exportMidi{path}   (error:"busy" ─▶ status busy, no dispatch)
    Save:    savePreset{overwrite:false} ─"exists"─▶ confirm ─true─▶ savePreset{overwrite:true}

## File Changes

| File | Action |
|---|---|
| `Source/ui/WebEditor.{h,cpp}` | Modify: D1, D2, D5 |
| `Source/ui/WebEditorSupport.{h,cpp}` | Modify: `PendingEventGate`, `defaultExportFile`, `exportChooserResult(File)`, `busyChooserResult()` |
| `Source/preset/{Preset.h,PresetManager.h,PresetManager.cpp}` | Modify: `busy`, cache |
| `Source/plugin/BerlinAudioProcessor.cpp`, `BerlinAudioProcessorEditor.cpp`, `Source/bridge/UiBridge.cpp` | Modify: D3 |
| `Tests/Source/{WebEditorSupportTests,PresetManagerFileTests,BerlinAudioProcessorTests,UiBridgeTests}.cpp` | Modify |
| `ui/src/bridge/{protocol,limits,bridge,native,mock,index}.ts`, `ui/src/store/{store,context,status}.ts` | Create/Rewrite (PR2) |
| `ui/src/lib/{seed,skew,delayRecs}.ts`, `ui/src/components/**` | Create (PR3/PR4) |
| `ui/src/{App,main}.tsx`, `ui/package.json`, `ui/vite.config.ts`, `ui/scripts/{embed-lib.mjs,limits-drift.test.mjs}` | Modify/Create |

## Interfaces / Contracts

```cpp
class PendingEventGate { public: void markPending() noexcept; bool shouldEmit (bool visible) const noexcept; void commit() noexcept; private: bool pending = false; };
juce::File defaultExportFile (const juce::File& documentsDir);   // documentsDir/Berlin/berlin-export.mid
juce::var  exportChooserResult (const juce::File& result);       // File() -> {cancelled:true,path:""}; else {cancelled:false,path:<full path>}
juce::var  busyChooserResult();                                  // {cancelled:true,path:"",error:"busy"}
```

## Status Messages

`status.ts` `describe(command, token)`. Preset and export wording is copied verbatim from `BerlinAudioProcessorEditor.cpp` `describePresetFailure` / `describeWriteFailure`; `busy` reuses the legacy regenerate/mutate wording. Error rows render in the error style.

| Command | Token / outcome | Message |
|---|---|---|
| any | `busy` | `Busy, try again` |
| `savePreset` | `exists` | `A preset with that name already exists.` (only reached if the overwrite flow is bypassed) |
| `savePreset`, `loadPreset` | `nameInvalid` | `Preset name is invalid.` |
| `savePreset`, `loadPreset` | `directoryUnavailable` | `Preset folder unavailable.` |
| `savePreset`, `loadPreset` | `writeFailed` | `Could not write the preset file.` |
| `savePreset`, `loadPreset` | `fileNotFound` | `Preset not found.` |
| `savePreset`, `loadPreset` | `parseFailed` | `Preset file is invalid or corrupted.` |
| `savePreset`, `loadPreset` | `unsupportedVersion` | `Preset was saved by a newer version of Berlin.` |
| `exportMidi` | `invalidTimeline` | `Export failed: the timeline was invalid.` |
| `exportMidi` | `pathUnavailable` | `Export failed: destination folder unavailable.` |
| `exportMidi` | `writeFailed` | `Export failed: could not write the file.` |
| `exportMidi` | `path not absolute` | `Export failed: the path is not absolute.` |
| any | thrown / rejected promise | `Error: <message>` |
| any | other token (`missing arg: …`, `invalid enum: …`, …) | the raw token |
| seed field | invalid input | `Seed must be a whole number.` |
| `regenerate` / `mutate` ok | success | `Generated.` / `Randomized.` / `Mutated.` |
| `savePreset` / `loadPreset` ok | success | `Saved "<name>".` / `Loaded "<name>".` |
| `exportMidi` ok | success | `Exported to <file name>` |

## Testing Strategy

| Layer | Cases (RED first) |
|---|---|
| BerlinTests | Gate: initially not emitting; pending+hidden → no; pending+visible → yes; no commit → still yes; commit → no. Chooser results and default path. Cache: equals a fresh-manager scan; second call leaves `parseCount` unchanged; rewriting (size, or `setLastModificationTime`) re-parses and delists invalid; add/remove are reflected. `loadPreset` while pending → `busy`, state unchanged; bridge → `ok:false,"busy"`, no snapshot. |
| Vitest | Protocol fixtures; mock envelopes and clamps; store coalescing (3 rapid sends → 2 dispatches, last value), stale response dropped, event does not clobber overlay, `ok:false` reverts and sets status, rejection does not escape; `resolveBridge` matrix (`hasHost` × `loadMock`); `assertNoMockChunk`; drift guard; `status.ts` table including command-dependent `writeFailed`; skew round-trip at the midpoints; recommendations at 120 bpm; seed; export flow (chosen, cancelled, busy) and save flow; DOM smoke tests per panel. |
| Manual | D8 checklist in standalone and Sonar, including closing the editor mid-dialog; listener-removed-first and destructor order verified by code review. |

## Threat Matrix

N/A: no routing, shell, subprocess, VCS/PR automation or executable-file classification boundary. The export path comes from the native chooser, and `exportMidi` already requires an absolute path.

## Size Forecast

PR1 ~420 (production ~230, tests ~190); PR2 ~760; PR3 ~760; PR4 ~740.

## Migration / Rollout

No migration is required. `BERLIN_WEB_UI` stays off by default. `PresetResult::busy` is additive.

## Open Questions

- [x] Does the Win32 `FileChooser` or `ScopedMessageBox` teardown invoke the callback synchronously? Resolved in manual check 23.17 (Sonar, debugger): no, both callbacks fire asynchronously after `~WebEditor`, so the `SafePointer` null check is the active guard and `shuttingDown` stays as defense in depth.
