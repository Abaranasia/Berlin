# Tasks: VST3/AU Plugin (roadmap Phase 11)

## Review Workload Forecast

| Field | Value |
|-------|-------|
| Estimated changed lines | ~1400-1800 (8 new files incl. ~40-widget editor + 3 new test files; 4 modified files incl. MainComponent reduction) |
| 400-line budget risk | High |
| Chained PRs recommended | No — user-accepted `size:exception` (obs #267) |
| Suggested split | Single PR, not chained |
| Delivery strategy | exception-ok |
| Chain strategy | size-exception |

Decision needed before apply: No
Chained PRs recommended: No
Chain strategy: size-exception
400-line budget risk: High

The 800-line internal guideline and the 400-line review budget are both exceeded by the editor extraction alone (~40 widgets). Per obs #267 the user explicitly chose a single PR with `size:exception` for this change, not chained/stacked delivery. Do not re-litigate this at apply time.

### Suggested Work Units (internal sequencing only — all land in ONE PR)

| Unit | Goal | PR | Focused test command | Runtime harness | Rollback boundary |
|------|------|-----|----------------------|-----------------|-------------------|
| 1 | D6 pure helpers (GenerationParams/SequenceBuilder/AutoEvolveSchedule) | PR 1 (exception) | `BerlinTests.exe SequenceBuilder AutoEvolveSchedule` | Console `juce_core`-only target | Revert `Source/generation/` new files |
| 2 | BerlinAudioProcessor engine + tests | PR 1 (exception) | `BerlinTests.exe BerlinAudioProcessor` | Headless, no device (`juce_audio_processors_headless`) | Revert `Source/plugin/BerlinAudioProcessor.*` + test file |
| 3 | Editor + MainComponent shell + `.jucer` export | PR 1 (exception) | N/A — GUI, manual gate only | pluginval + DAW load (manual) | Revert `Source/plugin/BerlinAudioProcessorEditor.*`, `MainComponent.*`, `Berlin.jucer` audioplug export |

## Phase 1: Pure Generation Helpers (D6) — RED first

- [x] 1.1 RED: `Tests/Source/SequenceBuilderTests.cpp` — `buildSeededSequence` determinism per `RhythmMode` (random/euclidean/probability), same seed -> same `Sequence`.
- [x] 1.2 GREEN: create `Source/generation/GenerationParams.h` — `RhythmMode` enum (lifted from `MainComponent`) + params struct, JUCE-free.
- [x] 1.3 GREEN: create `Source/generation/SequenceBuilder.{h,cpp}` — move `MainComponent::buildSeededSequence` (MainComponent.cpp:47) verbatim as `berlin::buildSeededSequence`.
- [x] 1.4 RED: `Tests/Source/AutoEvolveScheduleTests.cpp` — re-baselining arithmetic incl. busy-retry (`lastMutationLoopCount`/`dueAtLoopCount`).
- [x] 1.5 GREEN: create `Source/generation/AutoEvolveSchedule.{h,cpp}` — move re-baselining logic verbatim.
- [x] 1.6 Confirm both suites reach GREEN in the existing `juce_core`-only test target — no new module needed for this phase.

## Phase 2: Test Infrastructure Confirmation

- [x] 2.1 Add `juce_audio_processors_headless` module + `BERLIN_HEADLESS=1` define to `Tests/BerlinTests.jucer`.
- [x] 2.2 Resave via `Projucer.exe --resave`, confirm the console target still builds/links before writing any `BerlinAudioProcessor` test (RESOLVED: headless module ALONE links; do NOT add `juce_audio_processors` — it depends on `juce_gui_extra` and breaks the headless-only constraint).

## Phase 3: BerlinAudioProcessor — RED (write before extraction lands)

- [x] 3.1 `Tests/Source/BerlinAudioProcessorTests.cpp`: `regenerate` determinism + busy rejection leaves state unchanged.
- [x] 3.2 `mutate` advances `mutationCount` only on success.
- [x] 3.3 `getStateInformation`/`setStateInformation` round-trip patch+seed (plugin-state-recall).
- [x] 3.4 `setStateInformation` with garbage/truncated/empty bytes leaves state untouched, no crash (threat matrix).
- [x] 3.5 Preset save/load round-trip via a temp directory.
- [x] 3.6 `prepareToPlay` -> N x `processBlock` -> `releaseResources` at block sizes 1/7/512/4096, rates 44100/48000/96000 Hz.
- [x] 3.7 `processBlock` called before `prepareToPlay`: no crash, silent output (threat matrix).
- [x] 3.8 Pre-filled input `MidiBuffer` is cleared; host events never observed (threat matrix).
- [x] 3.9 MIDI lands in the passed `MidiBuffer` at expected sample offsets; note-off precedes note-on.
- [x] 3.10 `isBusesLayoutSupported` rejects mono/5.1/with-input layouts (threat matrix).
- [x] 3.11 Several hundred blocks with synth+FX enabled stay bounded/finite (feedback-stability rule).

## Phase 4: BerlinAudioProcessor — GREEN

- [x] 4.1 Create `Source/plugin/BerlinAudioProcessor.{h,cpp}` owning `SequencePlayer`, `Transport`, `SynthEngine`, `MidiEventTranslator`, `PresetManager`, `GenerationParams`, seed, mutation state, auto-evolve `Timer`.
- [x] 4.2 Implement `processBlock`: `ScopedNoDenormals` -> `buffer.clear()` -> `midiMessages.clear()` -> `player.process` -> `translator.translate` -> `synth.render(0, n)`. No allocation/lock/log.
- [x] 4.3 Implement `prepareToPlay`/`releaseResources`/`isBusesLayoutSupported` (stereo-out-only, no input bus)/`getTailLengthSeconds` (reads owned `SynthPatch` delay/feedback/reverb-roomSize fields directly, since `Source/synth/*` stays unchanged/verbatim and exposes no tail query).
- [x] 4.4 Implement `getStateInformation`/`setStateInformation` via `PresetManager::toValueTree`/`fromValueTree` verbatim (no schema change, `kSchemaVersion` stays 1).
- [x] 4.5 Implement commands: `regenerate`/`mutate` (bool), `save`/`loadPreset` (`PresetResult`), `presetExists`, `listPresetNames`, `exportMidiTo` (`MidiFileWriteResult`).
- [x] 4.6 Implement accessors: `setPatch`/`getPatch`, `setGenerationParams`/`getGenerationParams`, `setSeed`/`getSeed`, `setSynthEnabled`, `setEffectsEnabled`, `setAutoEvolveEnabled`, `setAutoEvolveRate`, `getMutationCount`, `getCurrentSequence`, `flushPendingNoteOff`.
- [x] 4.7 Derive from `ChangeBroadcaster`; broadcast after auto-evolve `mutate()` and after `setStateInformation`.
- [x] 4.8 Define `createEditor()`/`hasEditor()` behind `#if BERLIN_HEADLESS` (return `nullptr`/`false`); editor `#include` under the same guard.
- [x] 4.9 `acceptsMidi()` = false, `producesMidi()` = true, `isMidiEffect()` = false. **(corrected post-manual-gate, see D8: `acceptsMidi()` now returns `true` — required for the VST3 SDK's `Instrument`-category event-input-bus rule; `producesMidi()`/`isMidiEffect()` unchanged)**
- [x] 4.10 Run Phase 3 suite to GREEN; code-review confirm zero allocation/lock/log anywhere in `processBlock`'s path.

## Phase 5: BerlinAudioProcessorEditor (GUI — no headless coverage)

- [x] 5.1 Create `Source/plugin/BerlinAudioProcessorEditor.{h,cpp}`: ~40 widgets ported from `MainComponent`; register as `ChangeListener` in ctor, deregister in dtor.
- [x] 5.2 Wire widget callbacks to processor command methods; render status text from returned result enums (`describePresetFailure`, `describeWriteFailure`, `statusLabel`).
- [x] 5.3 Own the overwrite `NativeMessageBox` and export `FileChooser` here (not in the processor, per D2).
- [x] 5.4 Explicitly note: this file has no automated test coverage (GUI) — matches design's stated gap, do not invent untestable assertions. Confirmed by a successful `Berlin.exe` guiapp build (Phase 6/7 integration compile), not by a unit test.

## Phase 6: MainComponent Reduction

- [x] 6.1 Modify `Source/MainComponent.{h,cpp}`: reduce to an `AudioAppComponent` shell owning one `BerlinAudioProcessor` + one `BerlinAudioProcessorEditor` + `MidiOutputSink`.
- [x] 6.2 Implement `getNextAudioBlock`'s sub-buffer view (wraps write pointers, 2ch, no allocation) -> `processor.processBlock` -> `midiSink.dispatch`.
- [x] 6.3 Forward `prepareToPlay`/`releaseResources` to the processor; keep the `flushPendingNoteOff` seam for the existing OS note-off flush on close.
- [x] 6.4 Manual regression only (no automated harness — GUI+device): standalone audio/OS-MIDI/UI behavior unchanged. `Berlin.exe` (guiapp) builds successfully with the new shell; actual runtime/UI behavior parity still needs a human to run it (deferred to Phase 8's manual gate, item 8.3).

## Phase 7: Plugin `audioplug` Export

- [x] 7.1 **DEVIATION FROM DESIGN (documented, verified technically necessary)**: design.md D7 / this task's literal wording call for adding an `audioplug` EXPORT "beside existing `guiapp`" inside the SAME `Berlin.jucer`. Verified against Projucer's own source (`jucer_Project.cpp:323` binds `projectTypeValue` to the project root via a single-select `ChoicePropertyComponent` at line 1477) that a `.jucer` file's `projectType` is a single scalar — one file cannot be simultaneously `guiapp` and `audioplug`, and `pluginFormats=...,buildStandalone` would substitute JUCE's own generic Standalone-plugin wrapper for our hand-rolled `MainComponent`/`MidiOutputSink` shell, which the locked constraints explicitly forbid replacing. Resolution: created a new sibling project `Plugin/BerlinPlugin.jucer` (in its own `Plugin/` subdirectory — NOT the repo root; see below), `projectType="audioplug"`, registering the SAME `Source/plugin/*`, `Source/generation/*`, `Source/core/*`, `Source/playback/*`, `Source/midi/{MidiMessageBytes.h,MidiEventTranslator.*}` (excl. `MidiOutputSink.*`), `Source/export/*`, `Source/synth/*`, `Source/preset/*` files BY RELATIVE PATH (`../Source/...`) — zero duplication, following the exact precedent `Tests/BerlinTests.jucer` already established for sharing this same source tree across multiple `.jucer` projects. `Berlin.jucer` (`guiapp`) is unchanged in `projectType`; only its own `Source/plugin/*` `<FILE>` registrations were added in Phase 6. Added `Source/plugin/BerlinPluginMain.cpp` (new, plugin-target-only) providing the required `createPluginFilter()` factory function `juce_audio_plugin_client` expects.
  **Subtlety discovered and fixed**: a first attempt placed `BerlinPlugin.jucer` at the repo root (same directory as `Berlin.jucer`). Both `.jucer` files generate a `JuceLibraryCode/` folder relative to their OWN directory — two `.jucer` files in the SAME directory clobber each other's generated glue code on `--resave`. This silently DELETED `Berlin.jucer`'s `include_juce_audio_devices.cpp`/`include_juce_audio_utils.cpp` and broke the `guiapp` build (caught immediately by re-running the `Berlin.sln` build after the plugin resave — do not skip this regression check). Fixed by moving `BerlinPlugin.jucer` into its own `Plugin/` subdirectory (mirroring `Tests/`'s existing isolation), which gives it an isolated `Plugin/JuceLibraryCode/` and `Plugin/Builds/VisualStudio2026/`. Re-verified: `Berlin.jucer` re-resaved to restore its correct `JuceLibraryCode/`, then all three targets (`Berlin.exe` guiapp, `BerlinPlugin.vst3` plugin, `BerlinTests.exe` headless suite) rebuilt clean in the same session.
- [x] 7.2 `pluginFormats=buildVST3,buildAU`; `pluginCharacteristicsValue=pluginIsSynth,pluginProducesMidiOut` (verified real attribute name is `pluginCharacteristicsValue`, not `pluginCharacteristics`, in this JUCE/Projucer version); did NOT set `pluginWantsMidiIn`/`pluginIsMidiEffectPlugin`. **(corrected post-manual-gate, see D8: `pluginWantsMidiIn` now set — added to the `pluginCharacteristicsValue` comma-list as `pluginIsSynth,pluginProducesMidiOut,pluginWantsMidiIn`; NOTE it is a member of that same list, not a standalone `.jucer` attribute — a first attempt added it as `pluginWantsMidiIn="1"` and Projucer's `--resave` silently collapsed/destroyed the existing `pluginCharacteristicsValue` list, caught immediately by re-reading the resaved file. `pluginIsMidiEffect` still NOT set — Berlin remains an instrument, not an effect)**
- [x] 7.3 `pluginManufacturerCode=Abar`, `pluginCode=Brln`, `pluginAUMainType='aumu'`, `pluginVST3Category=Instrument,Synth`. Also added `defines="JUCE_VST3_CAN_REPLACE_VST2=0"` (new plugin, never released as VST2 — avoids `juce_audio_plugin_client_VST3.cpp`'s mandatory `#error` about VST2/VST3 parameter-automation ID conflicts; irrelevant here regardless since there are zero `AudioProcessorValueTreeState` parameters per the locked GUI-only-controls constraint).
- [x] 7.4 Bus layout: already satisfied by `BerlinAudioProcessor`'s constructor (Phase 4) — `BusesProperties().withOutput("Output", stereo, true)`, no input bus. No `.jucer`-level bus configuration needed (modern JUCE plugin projects define buses in code, not XML).
- [x] 7.5 (added) Resaved `Plugin/BerlinPlugin.jucer` via `Projucer.exe --resave`, built the generated `Berlin.sln` (VS2026 exporter produces `Berlin_SharedCode`, `Berlin_VST3`, `Berlin_VST3ManifestHelper` projects — AU is macOS-only and correctly absent from the Windows exporter) — confirmed a real `BerlinPlugin.vst3` bundle is produced at `Plugin/Builds/VisualStudio2026/x64/Debug/VST3/BerlinPlugin.vst3`. Re-ran `Berlin.exe` (guiapp) and the full headless `BerlinTests.exe` suite afterward — all three targets green, no regressions.

## Phase 8: Manual Verification Gate (not in `BerlinTests.exe`)

- [x] 8.1 pluginval, strictness level 5+, against the built VST3. **User-confirmed passed** (tool-side unverifiable by this agent — pluginval is an external GUI/CLI tool run by a human against the built `BerlinPlugin.vst3`; no automated re-check possible from this session).
- [x] 8.2 DAW load: audio through internal synth; MIDI output routes to another track; editor close/reopen keeps state and auto-evolve running; session save/reload returns patch+seed (record the D4 generation-param reset as expected, not a bug). **User-confirmed**: Cakewalk Sonar load+play confirmed; Ableton Live load+play confirmed (only after the D8 fix — Ableton's own log previously rejected the plugin with "plugin has instrument category, but no valid event input bus" before `pluginWantsMidiIn`/`acceptsMidi()=true` landed; this re-verify exists specifically to confirm that regression is closed); editor close/reopen state retention confirmed.
- [x] 8.3 Standalone parity: `guiapp` binary unchanged, incl. OS MIDI device output and the CC123 panic guard on close. **User-confirmed passed.**
- [ ] 8.4 AU: macOS-only. If no macOS machine is available, record explicitly as deferred/unverified — never claim it passed. **Explicitly unverified — no macOS machine available.** Documented as an acceptable, non-blocking gap per design.md's Manual verification gate section (item 4: "AU: macOS-only; explicitly deferred/unverified if no macOS machine is available"). Not a regression, not claimed as passed.
