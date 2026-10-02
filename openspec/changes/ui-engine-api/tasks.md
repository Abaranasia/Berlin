# Tasks: Engine API for the UI (Slice 0/6)

## Review Workload Forecast

| Field | Value |
|---|---|
| Estimated changed lines | ~865 (production ~460, tests ~405) — design.md's own count |
| 400-line budget risk | High |
| Chained PRs recommended | No (user decision, 2026-10-02) |
| Suggested split | Single PR, `size:exception`; engine-first (PR1-shape) then bridge (PR2-shape) as internal review order only |
| Delivery strategy | single-pr / exception-ok |
| Chain strategy | size-exception |

Decision needed before apply: No
Chained PRs recommended: No
Chain strategy: size-exception
400-line budget risk: High

### Suggested Work Units (review order inside the one PR, not separate PRs)

| Unit | Goal | Likely PR | Focused test command | Runtime harness | Rollback boundary |
|---|---|---|---|---|---|
| 1 (engine) | `SequencePlayer` D1-D4, `SynthEngine` D5, `SynthPatch.h` comment, `BerlinAudioProcessor` D6/D7/D14 + getters, editor one-liner | Single PR, reviewed first | `BerlinTests.exe --name=SequencePlayerStop` then `--category=Berlin` | `Berlin.exe` manual smoke (Phase 10) | Revert engine commits only; additive atomics + one preserved comment line, no schema change |
| 2 (bridge) | `UiBridge` D8-D13, three `.jucer` registrations, `UiBridgeTests.cpp` | Single PR, reviewed second | `BerlinTests.exe --category=Berlin` | DAW plugin load if available (Phase 10.5) | Revert bridge commit; `UiBridge` is new/additive, no call sites outside tests yet |

## Phase 0: Baseline Builds (blocking, do first)

- [x] 0.1 Build `Builds/VisualStudio2026/Berlin.sln` (standalone, Debug x64) clean/rebuild; record exit code, errors, warnings as the regression baseline. **Result: 0 errors, 2 pre-existing warnings (Main.cpp C4100 x2), exit 0.**
- [x] 0.2 (**Result: CONFIRMED BROKEN — LNK2019 unresolved `berlin::delaySecondsFor`/`berlin::formatDelayRecommendations`, LNK1120 2 unresolved externals in Berlin_VST3.vcxproj.**) Build the plugin solution under `Plugin/Builds/VisualStudio2026/` (Debug x64); record the result **honestly, whatever it is** — it is suspected BROKEN today because `Plugin/BerlinPlugin.jucer` registers no `TempoSync.h/.cpp` (confirmed absent by inspection) while `Source/plugin/BerlinAudioProcessor.cpp`/editor call `delaySecondsFor`.
- [x] 0.3 Build `Tests/Builds/VisualStudio2026/BerlinTests.sln` (Debug x64) `/t:Rebuild`; record exit code. **Result: 0 errors, exit 0.**
- [x] 0.4 Run `Tests/Builds/VisualStudio2026/x64/Debug/ConsoleApp/BerlinTests.exe --category=Berlin`; record the exact baseline pass count (regression safety net for Phase 9). **Result: 320 beginTest blocks, exit 0, all passed.**
- [x] 0.5 Document the `.jucer` edit workflow for this slice (no code yet): any new/registered file requires `H:\Proyectos\Juce\Projucer\Projucer.exe --resave <path>\<Name>.jucer` (precedent: `playback-transport-clock` tasks.md 7.1), then rebuilding that `.jucer`'s regenerated VS solution. Keep each `.jucer` resaved from its own directory only — two `.jucer`s sharing one directory clobber each other's generated `JuceLibraryCode/` on resave (precedent: `vst3-au-plugin` tasks.md 7.1 subtlety); `Berlin.jucer`, `Plugin/BerlinPlugin.jucer`, and `Tests/BerlinTests.jucer` are already each in their own directory, so no relocation is needed here.

## Phase 1: SequencePlayer — Transition-Only Play/Stop Adoption (D1-D4)

- [x] 1.1 RED `Tests/Source/SequencePlayerStopTests.cpp`: with the existing `sampleRate=4,bpm=60,spb=1` fixture and an all-active sequence in 2-sample blocks, after step 5's note-on is emitted and `setPlaying(false)` is called 2 samples into step 5, the next `process()` emits exactly one note-off at offset 0 and the playhead stays frozen.
- [x] 1.2 RED same file: `isPlayRequested()` reflects the requested boolean immediately after `setPlaying(true)`, with zero `process()` calls made.
- [x] 1.3 RED same file: a direct `stop()` (no `setPlaying` request) still leaves `pendingNote` set; the following `process()` emits nothing (transition-only adopt, existing contract preserved).
- [x] 1.4 RED same file: restart resumes — after 1.1's stop, call `setPlaying(true)`; the next 4-sample `process()` emits exactly one event `{offset 2, step 6, note-on}` (no note-off), and no event carries step 5 or step 0.
- [x] 1.5 Confirm 1.1-1.4 fail to compile (`setPlaying`/`isPlayRequested` do not exist yet) — RED proven.
- [x] 1.6 GREEN `Source/playback/SequencePlayer.h`: add `std::atomic<bool> playRequested` (relaxed), ctor-initialized from `transport.isRunning()`; declare `void setPlaying (bool) noexcept;` and `bool isPlayRequested() const noexcept;`.
- [x] 1.7 GREEN `Source/playback/SequencePlayer.cpp`: `start()`/`stop()` also store `playRequested`; `setPlaying` stores it; in `process()`, after the `pendingBpm` adopt and before `countBoundaries`, adopt only on a transition — stop (`!playRequested && transport.isRunning()`): `transport.stop()`, push `{0, pendingStep, pendingNote, false}` if `pendingNote >= 0`, then `pendingNote = -1`; start (`playRequested && !transport.isRunning()`): `transport.start()`.
- [x] 1.8 Run `BerlinTests.exe --category=Berlin` — confirm 1.1-1.4 GREEN and no regression in existing `SequencePlayerTests`/`SequencePlayerHandoffTests`/`PlaybackTimingTests`.

## Phase 2: SynthEngine — Smoothed Master Level (D5) + Toggle Getters

- [x] 2.1 RED `Tests/Source/SynthEngineTests.cpp`: level 0.5 set before `prepare()` renders exactly 0.5× of a reference engine (two identically prepared engines, same events).
- [x] 2.2 RED same file: switching 1→0 ramps linearly (ratio ≈ `1 - i/2205` at 44.1 kHz, 50 ms smoothing), with exact zeros after sample 2205.
- [x] 2.3 RED same file: `setMasterLevel(2.5)` clamps `getMasterLevel()` to 1.0; `setMasterLevel(NaN)` clamps it to 0 (mirrors `clampParameter`'s documented NaN-to-`lo` behavior).
- [x] 2.4 RED same file: `isEnabled()` and `isEffectsEnabled()` getters reflect `setEnabled`/`setEffectsEnabled`.
- [x] 2.5 Confirm 2.1-2.4 fail to compile (`setMasterLevel`/`getMasterLevel`/`isEnabled`/`isEffectsEnabled` do not exist yet).
- [x] 2.6 GREEN `Source/synth/SynthEngine.h`: add `std::atomic<float> masterLevel{0.8f}` and `juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> masterGain`; declare `void setMasterLevel (float) noexcept;`, `float getMasterLevel() const noexcept;`, `bool isEnabled() const noexcept;`, `bool isEffectsEnabled() const noexcept;`.
- [x] 2.7 GREEN `Source/synth/SynthEngine.cpp`: `prepare()` calls `masterGain.reset(sampleRate, 0.05)` and `masterGain.setCurrentAndTargetValue(masterLevel load)`; `render()`, after the FX stage and before the terminal `addFrom` loop, calls `masterGain.setTargetValue(masterLevel load)` then `masterGain.applyGain(scratch, numSamples)`; the disabled early-return path snaps the smoother to target; `setMasterLevel` applies `clampParameter(v, kMinOutputLevel, kMaxOutputLevel)` before storing.
- [x] 2.8 Run `BerlinTests.exe --category=Berlin` — confirm 2.1-2.4 GREEN, no MIDI-path regression (`SynthEngineTests`/`SynthVoiceTests`/`DspLinkSmokeTests`).

## Phase 3: SynthPatch.h — Comment-Only Update

- [x] 3.1 `Source/synth/SynthPatch.h`: update the `kMinOutputLevel`/`kMaxOutputLevel` comment (currently "preset-persistence bound only ... no live setter exists") to state a live setter now exists (`SynthEngine::setMasterLevel`); no behavior change (±4 lines).

## Phase 4: BerlinAudioProcessor — New API, D6/D7/D14

- [x] 4.1 RED `Tests/Source/BerlinAudioProcessorTests.cpp`: headless `prepareToPlay(44100,512)` + `processBlock`; `setPlaying(false)` then one block gives a note-off in `midi` and no later note-on; `isPlaying()` returns the requested state before any `processBlock`.
- [x] 4.2 RED same file: `setMasterLevel(0)` keeps MIDI output identical to a run at the default level (audio-only effect).
- [x] 4.3 RED same file: `setPatch`, `loadPreset` (via `TempPresetDir`), and `setStateInformation` each apply `outputLevel` live (`synth.getMasterLevel()` reflects the new patch's value with no extra call).
- [x] 4.4 RED same file: synced delay is derived on `setPatch`/`setBpm`/`loadPreset`/state restore when `delaySynced` is true; Free mode (`delaySynced=false`) is untouched; an editor-style pre-derived patch round-trips as a no-op.
- [x] 4.5 RED same file (D14): `regenerate(false)` then, with no intervening `processBlock`, `regenerate(true)` returns `false` and leaves `getSeed()`, `getCurrentSequence()`, and `getMutationCount()` unchanged; a busy `mutate()` likewise leaves state unchanged.
- [x] 4.6 RED same file: new getters/setters exist and round-trip — `isPlaying`, `getPlayheadStep`, `getLoopCount`, `isSynthEnabled`, `areEffectsEnabled`, `isAutoEvolveEnabled`, `getAutoEvolveRate`, `getMasterLevel`/`setMasterLevel` (== `currentPatch.outputLevel`).
- [x] 4.7 Confirm 4.1-4.6 fail to compile (none of the new methods exist on `BerlinAudioProcessor` yet).
- [x] 4.8 GREEN `Source/plugin/BerlinAudioProcessor.h`: add `#include "core/TempoSync.h"`; declare `void setPlaying(bool); bool isPlaying() const; int getPlayheadStep() const; int getLoopCount() const; bool isSynthEnabled() const; bool areEffectsEnabled() const; void setMasterLevel(float); float getMasterLevel() const; bool isAutoEvolveEnabled() const; int getAutoEvolveRate() const;` plus a private `void applySyncedDelayTime();`.
- [x] 4.9 GREEN `Source/plugin/BerlinAudioProcessor.cpp`: implement the Phase 4.8 getters as thin forwarders to `player`/`synth`/existing members; `setPlaying` forwards to `player.setPlaying`; `setMasterLevel` clamps via `clampParameter`, writes `currentPatch.outputLevel`, calls `synth.setMasterLevel`; `pushPatchToSynth()` forwards `outputLevel`; `applySyncedDelayTime()` sets `currentPatch.delayTimeSeconds` from `delaySecondsFor(currentBpm, delayDivision)` when `delaySynced`, called from `setPatch()` before `pushPatchToSynth()` and from `setBpm()` followed by `synth.setDelayTimeSeconds`.
- [x] 4.10 GREEN same file (D14): at the top of `regenerate(bool drawNewSeed)`, return `false` immediately when `player.isPublishPending()`, **before** the `drawNewSeed && !lockSeed` seed draw; keep the existing `publishSequence` result check as a defensive second guard.
- [x] 4.11 Run `BerlinTests.exe --category=Berlin` — confirm 4.1-4.6 GREEN, no regression in existing `BerlinAudioProcessorTests`/`PresetSerializationTests`/`PresetManagerFileTests`.

## Phase 5: PresetManager — Shared Enum-Name Forwarders (D8)

- [x] 5.1 GREEN `Source/preset/PresetManager.h`: add 8 public static forwarders — `waveformName`/`parseWaveform`, `lfoDestinationName`/`parseLfoDestination`, `scaleTypeName`/`parseScaleType`, `divisionName`/`parseDivision` — delegating to the existing anonymous-namespace helpers (no table move, ~20 lines). Covered by Phase 6's bridge enum round-trip tests (no separate RED needed — these are pure delegation with zero new logic).
- [x] 5.2 GREEN `Source/preset/PresetManager.cpp`: implement the 8 forwarders as one-line calls into the existing helpers; confirm no accidental recursion against the anonymous `waveformToName`-style names.

## Phase 6: UiBridge — Command Dispatch (D9-D13)

- [x] 6.1 RED `Tests/Source/UiBridgeTests.cpp` (new, own `TempPresetDir` copy): generic invalid matrix for every applicable command — missing required arg (`"missing arg: X"`), wrong type (`"invalid type: X"`), NaN/Infinity (`"non-finite: X"`), unknown field in `setPatch`/`setGenerationParams` (`"unknown field: X"`), bad enum name (`"invalid enum: X"`) — each asserting the exact token and that a subsequent snapshot is unchanged.
- [x] 6.2 RED same file: `dispatch("doesNotExist", {})` returns `{ok:false, error:"unknown command: doesNotExist"}` with no `snapshot` key; bpm (or any probed field) is unchanged afterward.
- [x] 6.3 RED same file: a no-state-change helper compares `JSON::toString(snapshot())` before/after every rejected call for equality (used across 6.1-6.2 and 6.6-6.8).
- [x] 6.4 RED same file: valid-input round trip for each of the 15 commands — `setPlaying`, `setBpm` (clamped to `[kMinBpm,kMaxBpm]`), `setSeed` (19-digit decimal string round-trips, `strtoll`+`ERANGE`), `setPatch` (partial merge, omitted fields preserved, recomputes synced delay time per D7), `setGenerationParams` (partial merge, commits immediately; pulses `[0,16]`, rotation `[0,15]`, stepProbability `[0,1]`, root `[0,11]`, range `[0,127]` + `normalizePitchRange`), `regenerate{randomize}`, `mutate{}`, `setSynthEnabled`, `setEffectsEnabled`, `setMasterLevel` (clamped, not rejected, for an out-of-range finite value), `setAutoEvolveEnabled`, `setAutoEvolveRate` (`[1,16]`), `savePreset{name,overwrite}`, `loadPreset{name}`, `exportMidi{path}`.
- [x] 6.5 RED same file: busy regenerate — after a pending `regenerate{randomize:false}` with no intervening `processBlock`, `regenerate{randomize:false}` returns `{ok:false,error:"busy"}` with the sequence unchanged; busy randomize additionally asserts the seed and full snapshot JSON (seed included) are byte-identical before/after.
- [x] 6.6 RED same file: `lockSeed` — with `generationParams.lockSeed=true`, `regenerate{randomize:true}` keeps the seed; with `false` (on a fresh processor or after draining the pending publish via `prepareToPlay`+`processBlock`) it changes.
- [x] 6.7 RED same file: `savePreset{overwrite:false}` on an existing name returns `"exists"` and writes nothing; `loadPreset{name:"Ghost"}` returns the `PresetResult` name `"fileNotFound"`, state unchanged.
- [x] 6.8 RED same file (D13): `exportMidi{path:"output.mid"}` (relative) returns `{ok:false,error:"path not absolute"}`, no debug assertion fires, no file is created, and no `juce::File` is constructed from the raw string.
- [x] 6.9 RED same file: snapshot shape — contains all documented top-level keys, `patch` has 21 fields (enums as the preset XML names), `steps` has exactly 16 `{note,active}` entries, `seed` is a JSON string.
- [x] 6.10 Confirm all of 6.1-6.9 fail to compile (`UiBridge` does not exist yet) — RED proven.
- [x] 6.11 GREEN `Source/bridge/UiBridge.h` (new, forward-declares `BerlinAudioProcessor`, headless-safe): `explicit UiBridge(BerlinAudioProcessor&) noexcept; juce::var dispatch(const juce::String&, const juce::var&); juce::var snapshot() const;`.
- [x] 6.12 GREEN `Source/bridge/UiBridge.cpp`: field-descriptor tables (pointer-to-member + bounds) driving `setPatch`/`setGenerationParams` decode and `snapshot()` encode so they cannot drift (D9); a `constexpr` 15-entry dispatch array `{name, run}` with `static_assert(size==15)`; arg-rule helpers (D10: object required, every documented arg required, number via `isInt`/`isInt64`/`isDouble` + finite check, bool via `isBool` only, int via `lround` after clamp, seed via `strtoll`+end-pointer+`ERANGE`); D11 error-token constants; D12 clamp bounds per field; D13 `juce::File::isAbsolutePath` check before any `juce::File` construction in `exportMidi`.
- [x] 6.13 Run `BerlinTests.exe --category=Berlin` — confirm 6.1-6.9 GREEN.

## Phase 7: Old Editor — One-Line Approved Exception

- [x] 7.1 `Source/plugin/BerlinAudioProcessorEditor.cpp`: in `currentPatchFromWidgets`, replace the default `outputLevel` assignment with `owner.getPatch().outputLevel` (the single approved exception — prevents touching a knob from resetting a loaded preset's level to 0.8). No other editor line changes.

## Phase 8: `.jucer` Registration (blocking before Phase 9)

- [x] 8.1 `Berlin.jucer`: add a new `bridge` group with `Source/bridge/UiBridge.h`/`.cpp`.
- [x] 8.2 `Plugin/BerlinPlugin.jucer`: add `Source/bridge/UiBridge.h`/`.cpp` **and** `Source/core/TempoSync.h`/`.cpp` (currently missing — root cause of the suspected Phase 0.2 break).
- [x] 8.3 `Tests/BerlinTests.jucer`: add `Tests/Source/UiBridgeTests.cpp`, plus its own `UiBridge.h`/`.cpp` `<FILE>` entries under fresh ids (mirrors the existing `tlBapC`-style per-jucer duplication for shared `Source/` files).
- [x] 8.4 Resave all three via `H:\Proyectos\Juce\Projucer\Projucer.exe --resave <path>` (one call per `.jucer`); confirm each regenerated VS2026 solution opens/parses with no missing-file errors.

## Phase 9: Full Regression Gate

- [x] 9.1 Rebuild `Builds/VisualStudio2026/Berlin.sln` (Debug x64) — 0 errors; compare warnings against the Phase 0.1 baseline.
- [x] 9.2 Rebuild the plugin solution under `Plugin/Builds/VisualStudio2026/` (Debug x64) — now expected to succeed (TempoSync + UiBridge registered); if still broken, record the exact error and treat as a blocker.
- [x] 9.3 Rebuild `Tests/Builds/VisualStudio2026/BerlinTests.sln` (Debug x64) `/t:Rebuild` — 0 errors.
- [x] 9.4 Run `Tests/Builds/VisualStudio2026/x64/Debug/ConsoleApp/BerlinTests.exe --category=Berlin` — full suite green; compare the pass count against the Phase 0.4 baseline plus every new test added in Phases 1-6.

## Phase 10: Manual Verification (human-only — flag clearly, not automatable)

- [ ] 10.1 **MANUAL**: Run the standalone app (`Berlin.exe`) — confirm it launches and plays as before (no engine API change is reachable from the old editor except the Phase 7 exception).
- [ ] 10.2 **MANUAL**: Confirm Synth on/off and FX on/off toggles still behave as before in the standalone app (no regression from the new `isSynthEnabled`/`areEffectsEnabled` getters, which are additive reads).
- [ ] 10.3 **MANUAL**: Confirm the old editor still works end-to-end (knobs, presets, tempo sync) with only the Phase 7 one-line change applied.
- [ ] 10.4 **MANUAL**: Listen for a click or dropout when the master level changes rapidly (stress beyond the 2205-sample unit test) — confirm the 50 ms smoother sounds clean in practice.
- [ ] 10.5 **MANUAL, if a DAW is available**: Load `BerlinPlugin.vst3` in a host — confirm it scans, loads, and plays (first real exercise of the now-fixed Plugin build from Phase 9.2).

Note: Threat matrix is N/A for this change (no routing/shell/subprocess/VCS/process-integration boundary; `exportMidi` writes only to a user-chosen absolute path, rejected pre-`juce::File` when relative per D13) — no RED tasks owed beyond Phases 1-6's cases already listed.

## Delivery Notes (2026-10-03)

- Delivered as two stacked PRs (user decision; actual size ~2000 authored lines vs ~865 estimated):
  - PR1 engine: `feat/ui-engine-api` @ `1db0a70` (base `feat/UI-design`) — Phases 1-4, 7 and the plugin TempoSync registration. Review lineage `review-38014a24a6e2d63d` approved; 337/337 tests.
  - PR2 bridge: `feat/ui-engine-api-pr2` (base PR1) — Phases 5, 6, 8 (UiBridge registrations) and these SDD docs. 350/350 tests.
- PR1 review follow-ups applied before PR1 commit: `loadPreset()`/`setStateInformation()` now call `setBpm()` before `setPatch()` so the synced delay time is computed once at the restored bpm; the 50 ms ramp is the named constant `kMasterLevelRampSeconds` (SynthEngine.h), shared with the tests. One extra regression test added (setStateInformation final-bpm delay derivation).
- First PR1 review lineage `review-783669254673168c` is left stale: finalize failed on a duplicate finding id across lenses (tooling incident, no code finding involved).
