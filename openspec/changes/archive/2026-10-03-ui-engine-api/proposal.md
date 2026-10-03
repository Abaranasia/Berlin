# Proposal: Engine API for the UI

Slice 0 of 6, UI redesign (`docs/proposal/ui-redesign-plan.md`). Builds on `explore.md` (Engram #328).

## Intent

The planned WebView UI needs state and controls the processor does not expose: play/stop, playhead, toggle getters, live master level, and one validated command surface. This slice adds them in C++ only, so Slice 1 can wire a web view without touching the engine.

## Scope

### In Scope
- Transport: `setPlaying(bool)`/`isPlaying()`. An atomic request on `SequencePlayer` (mirrors `pendingBpm`) is adopted inside `process()`. An adopted stop emits any pending note-off through the normal event path. `isPlaying()` returns the **requested** state, so the UI is correct even when no audio callback runs.
- Passthroughs: `getPlayheadStep()`, `getLoopCount()`.
- Getters: `isSynthEnabled()`, `areEffectsEnabled()`, `isAutoEvolveEnabled()`, `getAutoEvolveRate()`.
- Master level: `setMasterLevel(float)`/`getMasterLevel()`, clamped to `[kMinOutputLevel, kMaxOutputLevel]` = `[0, 1]`. One setter updates `currentPatch.outputLevel` and a `SynthEngine` atomic. Smoothed gain at the terminal mix. Audio only; MIDI unaffected. `setPatch`, `loadPreset`, and `setStateInformation` now apply `outputLevel` live, which closes a gap with `preset-persistence`, whose spec already calls it live-adjustable.
- `UiBridge` (`Source/bridge/`): `dispatch(command, args)` returns `{ok, error[, snapshot]}`, and `snapshot()` returns patch, generation params, seed, BPM, transport, toggles, master level, auto-evolve, mutation count, steps, and preset names.
- Commands: `setPlaying`, `setBpm`, `setSeed`, `setPatch` (partial merge; recomputes synced delay time), `setGenerationParams` (partial merge, committed immediately), `regenerate {randomize}`, `mutate`, `setSynthEnabled`, `setEffectsEnabled`, `setMasterLevel`, `setAutoEvolveEnabled`, `setAutoEvolveRate`, `savePreset {name, overwrite}`, `loadPreset`, `exportMidi {path}`. Each command clamps its input. An unknown command or malformed args returns `ok=false` and leaves state unchanged.
- Register files in `Berlin.jucer`, `Plugin/BerlinPlugin.jucer`, and `Tests/BerlinTests.jucer`.

### Out of Scope
- WebView, any preset schema change.
- Any editor change, with one approved exception: the old editor's `currentPatchFromWidgets` keeps `owner.getPatch().outputLevel` instead of the default, so touching a knob no longer resets a loaded preset's level to 0.8.
- Native dialogs (file chooser, overwrite confirm).
- A typed command layer (explore Option 2).

## Capabilities

### New Capabilities
- `ui-bridge`: command dispatch, validation, result shape, snapshot contents.

### Modified Capabilities
- `playback-transport`: thread-safe play/stop request adopted on the audio thread; note-off on stop.
- `internal-synth-output`: live, smoothed master level at the terminal mix; applied on patch, preset, and state load.

## Approach

Explore Option 3: a name→handler dispatch table with inline validation. Copy the existing atomic patterns (`pendingBpm`, `enabled`). Strict TDD in `BerlinTests`. Estimate is about 550–750 changed lines including tests, within the 800 budget.

## Affected Areas

| Area | Impact |
|---|---|
| `Source/playback/SequencePlayer.h/.cpp` | Modified: play request, stop note-off |
| `Source/synth/SynthEngine.h/.cpp` | Modified: master atomic and smoothing, getters |
| `Source/plugin/BerlinAudioProcessor.h/.cpp` | Modified: new API, `outputLevel` propagation |
| `Source/bridge/UiBridge.h/.cpp` | New |
| `Tests/Source/UiBridgeTests.cpp` and existing suites | New/Modified |
| three `.jucer` files | Modified |

## Risks

| Risk | Likelihood | Mitigation |
|---|---|---|
| Default output is now 0.8× (about -1.9 dB) because `outputLevel` takes effect | High | Accepted and documented. Adds headroom. |
| Stop leaves a stuck note | Med | Note-off is emitted in `process()`; tested |
| Bridge commits generation params immediately, unlike the old editor's staged widgets | Low | Intentional; old editor unchanged |
| A `.jucer` registration is missed | Low | Shows up as a link error |

## Rollback Plan

Revert the slice commit. Everything is additive except the live `outputLevel`. After a revert, saved presets still load, because the schema is unchanged.

## Dependencies

- None. Every precedent (`pendingBpm`, `TempoSync`, `TempPresetDir`) already exists.

## Success Criteria

- [ ] Every command has a valid-input test and an invalid-input test. Unknown commands leave state unchanged.
- [ ] Stopping silences the synth with no stuck note; restarting resumes.
- [ ] Level changes are smoothed, MIDI is unaffected, and presets restore the level live.
- [ ] Old editor untouched except the single approved `currentPatchFromWidgets` line (keeps `owner.getPatch().outputLevel`); `BerlinTests.exe --category=Berlin` passes.

## Proposal question round

Auto mode, so these were resolved by assumption. Flag any to change:
1. Should the default level stay at 0.8 (quieter than today), or should the default change to 1.0? Changing it would need a schema-default decision.
2. Should `isPlaying()` report the requested state rather than the adopted state?
3. On restart, should playback resume from the current position or reset to step 1? The assumption is that it resumes.
