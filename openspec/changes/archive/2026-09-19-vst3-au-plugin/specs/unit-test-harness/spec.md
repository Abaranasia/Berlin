# Delta for Unit Test Harness

## MODIFIED Requirements

### Requirement: Console Test Runner Project

The system MUST provide a sibling Projucer project `Tests/BerlinTests.jucer` with `projectType="consoleapp"` and the preprocessor definition `JUCE_UNIT_TESTS=1`, built independently from `Berlin.jucer`, sharing all harness-eligible source tiers — currently `Source/core/*`, `Source/generation/*`, `Source/playback/*`, `Source/midi/*` (excluding `MidiOutputSink.*`), the JUCE-free half of `Source/export/*`, the DSP math of `Source/synth/*`, and `Source/plugin/BerlinAudioProcessor.{h,cpp}` (excluding `BerlinAudioProcessorEditor.{h,cpp}`, which is GUI and stays manual-gate-only) — by relative-path file registration (no source duplication). The harness's module list MUST include `juce_dsp`, `juce_audio_formats`, `juce_audio_basics`, and `juce_audio_processors_headless` in addition to `juce_core`. A source tier or file that depends on any module beyond this set (e.g. `Source/midi/MidiOutputSink.*` or `BerlinAudioProcessorEditor.*`, requiring `juce_audio_devices`/`juce_gui_basics`) MUST NOT be registered in this project and stays manual-gate-only.

(Previously: did not include `BerlinAudioProcessor` or `juce_audio_processors_headless`; the plugin engine layer did not exist.)

#### Scenario: Test project regenerates and builds

- GIVEN `Tests/BerlinTests.jucer` with its registered source files
- WHEN `Projucer.exe --resave Tests/BerlinTests.jucer` is run and the generated project is built
- THEN the build succeeds and produces a console executable

#### Scenario: BerlinAudioProcessor is registered, its editor is not

- GIVEN `Source/plugin/BerlinAudioProcessor.{h,cpp}` and `Source/plugin/BerlinAudioProcessorEditor.{h,cpp}`
- WHEN `Tests/BerlinTests.jucer`'s file list is inspected
- THEN `BerlinAudioProcessor.{h,cpp}` is registered by relative path with `juce_audio_processors_headless` in the module list
- AND `BerlinAudioProcessorEditor.{h,cpp}` is absent, because it depends on GUI modules

## ADDED Requirements

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
