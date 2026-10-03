# Unit Test Harness Specification

## Purpose

A runnable, headless console test target for Berlin, modeled on `JUCE/extras/UnitTestRunner`, that discovers and runs all `juce::UnitTest` suites and reports success/failure via process exit code — enabling CTest-less CI/automation and strict TDD red/green cycles for the domain and generation code.

## Requirements

### Requirement: Console Test Runner Project

The system MUST provide a sibling Projucer project `Tests/BerlinTests.jucer` with `projectType="consoleapp"` and the preprocessor definition `JUCE_UNIT_TESTS=1`, built independently from `Berlin.jucer`, sharing all harness-eligible source tiers — currently `Source/core/*`, `Source/generation/*`, `Source/playback/*`, `Source/midi/*` (excluding `MidiOutputSink.*`), the JUCE-free half of `Source/export/*`, the DSP math of `Source/synth/*`, and `Source/plugin/BerlinAudioProcessor.{h,cpp}` (excluding `BerlinAudioProcessorEditor.{h,cpp}`, which is GUI and stays manual-gate-only) — and additionally the gui-free `Source/ui/` helpers (asset lookup, MIME, path-traversal rejection, user-data-folder path, native-args adapter), using only `juce_core`, and `Source/bridge/*` (`UiBridge`), all by relative-path file registration (no source duplication). The harness's module list MUST include `juce_dsp`, `juce_audio_formats`, `juce_audio_basics`, and `juce_audio_processors_headless` in addition to `juce_core`. `WebEditor.{h,cpp}` and `Source/ui/generated/EmbeddedAssets.*` MUST NOT be registered; they stay manual-gate-only. A source tier or file that depends on any module beyond this set (e.g. `Source/midi/MidiOutputSink.*` or `BerlinAudioProcessorEditor.*`, requiring `juce_audio_devices`/`juce_gui_basics`) MUST NOT be registered in this project and stays manual-gate-only.

(Previously: did not include `BerlinAudioProcessor`, `juce_audio_processors_headless`, or `Source/ui/` helpers; the plugin engine layer and GUI-free helpers did not exist. `Tests/BerlinTests.jucer` already registers `../Source/bridge/UiBridge.{h,cpp}`; this delta only corrects the requirement text to list `Source/bridge/*`, and no `.jucer` change is needed for the bridge. Only the new gui-free `Source/ui/` helpers are newly registered.)

#### Scenario: Test project regenerates and builds

- GIVEN `Tests/BerlinTests.jucer` with its registered source files
- WHEN `Projucer.exe --resave Tests/BerlinTests.jucer` is run and the generated project is built
- THEN the build succeeds and produces a console executable

#### Scenario: BerlinAudioProcessor is registered, its editor is not

- GIVEN `Source/plugin/BerlinAudioProcessor.{h,cpp}` and `Source/plugin/BerlinAudioProcessorEditor.{h,cpp}`
- WHEN `Tests/BerlinTests.jucer`'s file list is inspected
- THEN `BerlinAudioProcessor.{h,cpp}` is registered by relative path with `juce_audio_processors_headless` in the module list
- AND `BerlinAudioProcessorEditor.{h,cpp}` is absent, because it depends on GUI modules

#### Scenario: Bridge and helpers registered

- GIVEN `Tests/BerlinTests.jucer`
- WHEN its file list is inspected
- THEN `Source/bridge/*` (already present) and the new gui-free `Source/ui/` helpers are registered by relative path

#### Scenario: GUI and generated files excluded

- GIVEN `WebEditor.{h,cpp}` and `EmbeddedAssets.{h,cpp}`
- WHEN the file list is inspected
- THEN none are present, and the project builds with no `juce_gui_*` module and no pnpm

### Requirement: UnitTest Suite Discovery

The system MUST discover and run all `juce::UnitTest`-derived test suites in the linked binary without requiring manual registration of each suite in a runner entry point.

#### Scenario: New test suite is picked up automatically

- GIVEN a new class deriving from `juce::UnitTest` is added to `Tests/Source/`
- WHEN the test binary is rebuilt and run with no filter arguments
- THEN the new suite's tests execute as part of the full run

#### Scenario: Category/name filtering

- GIVEN multiple registered `juce::UnitTest` suites across different categories
- WHEN the runner is invoked with a `--category` or `--name` filter matching a subset
- THEN only the matching suite(s) execute

### Requirement: Headless BerlinAudioProcessor Engine Coverage

The system MUST test `BerlinAudioProcessor`'s engine-orchestration logic headlessly, without an audio device: driving `prepareToPlay`/`processBlock` over allocated buffer pairs across multiple block sizes and sample rates, and asserting (a) MIDI events land in the host `MidiBuffer` at their expected sample offsets, (b) `regenerate`/`mutate` produce deterministic results for a given seed, and (c) `getStateInformation`/`setStateInformation` round-trip patch and seed correctly. These tests MUST be written before the `MainComponent` extraction lands (Strict TDD).

#### Scenario: processBlock output is asserted across block sizes

- GIVEN a constructed `BerlinAudioProcessor` prepared at several different block sizes and sample rates
- WHEN `processBlock` is driven over allocated buffers for each configuration
- THEN MIDI events appear in the host buffer at the expected sample offsets for every configuration tested

#### Scenario: regenerate/mutate are deterministic for a given seed

- GIVEN a fixed seed
- WHEN `regenerate` then `mutate` are invoked in a test
- THEN repeating the same sequence of calls with the same seed produces identical results

#### Scenario: State round-trip is verified headlessly

- GIVEN a processor with a known patch and seed
- WHEN `getStateInformation` output is fed into `setStateInformation` on a second instance
- THEN the second instance's patch and seed match the first, without requiring an audio device

### Requirement: Exit Code Contract

The system MUST exit with code 0 when all executed tests pass, and MUST exit with a non-zero code (1) when any executed test fails, enabling CTest-less pass/fail automation.

#### Scenario: All tests pass

- GIVEN a test run where every executed `juce::UnitTest` assertion succeeds
- WHEN the process completes
- THEN the process exit code is 0

#### Scenario: A test fails

- GIVEN a test run where at least one executed `juce::UnitTest` assertion fails
- WHEN the process completes
- THEN the process exit code is 1

### Requirement: Deterministic Seed Argument

The system MUST accept a `--seed` argument that, when provided, is used to seed any randomized aspects of the test run itself (independent of any seed used inside `DeterministicRandom`-based domain tests), so a failing run can be reproduced.

#### Scenario: Re-running with the same runner seed reproduces the run

- GIVEN a test run invoked with `--seed <value>` that reports a failure
- WHEN the runner is invoked again with the same `--seed <value>`
- THEN the same tests execute in the same order with the same result

### Requirement: Headless UI Helper Coverage

The harness MUST include tests, written before the helpers (Strict TDD), for asset lookup (`/` to `index.html`, unknown returns `nullopt`, empty table `{nullptr, 0}` returns `nullopt`, traversal rejected), MIME mapping, user-data-folder path, and the native-args adapter (valid dispatch, missing/invalid args produce an error result without crashing).

#### Scenario: Helper suites run headlessly

- GIVEN the test binary
- WHEN run with no filter
- THEN the UI helper suites execute and pass without a window or WebView2

#### Scenario: Adapter error paths covered

- GIVEN empty, non-string-cmd, and malformed-args inputs
- WHEN the adapter tests run
- THEN each yields an error result and no failure or crash
