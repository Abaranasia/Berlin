# Delta for Unit Test Harness

## MODIFIED Requirements

### Requirement: Console Test Runner Project

The system MUST provide a sibling Projucer project `Tests/BerlinTests.jucer` with `projectType="consoleapp"` and the preprocessor definition `JUCE_UNIT_TESTS=1`, built independently from `Berlin.jucer`, sharing all harness-eligible source tiers — currently `Source/core/*`, `Source/generation/*`, `Source/playback/*`, `Source/midi/*`, the JUCE-free half of `Source/export/*`, and the DSP math of `Source/synth/*` that depends only on `juce_core`, `juce_dsp`, `juce_audio_formats`, and/or `juce_audio_basics` — by relative-path file registration (no source duplication). The harness's module list MUST include `juce_dsp`, `juce_audio_formats`, and `juce_audio_basics` in addition to `juce_core` (`juce_dsp` depends on `juce_audio_formats`, which depends on `juce_audio_basics` — all three must be registered together for the project to link). A source tier or file that depends on any module beyond this set (e.g. `Source/midi/MidiOutputSink.*`, requiring `juce_audio_devices`) MUST NOT be registered in this project and stays manual-gate-only.
(Previously: the harness's module list was `juce_core`-only, and anything requiring more MUST NOT be registered.)

#### Scenario: Test project regenerates and builds
- GIVEN `Tests/BerlinTests.jucer` with its registered source files
- WHEN `Projucer.exe --resave Tests/BerlinTests.jucer` is run and the generated project is built
- THEN the build succeeds and produces a console executable

#### Scenario: Shared JUCE-free sources are not duplicated
- GIVEN the JUCE-free source tiers under `Source/`
- WHEN both `Berlin.jucer` and `Tests/BerlinTests.jucer` are inspected
- THEN each shared file is registered by relative path in both projects
- AND no copy of the file exists under `Tests/`

#### Scenario: A new tier splits into a unit-tested half and a manual-gate-only half
- GIVEN `Source/export/MidiExportTimeline.*` (JUCE-free) and `Source/export/MidiFileWriter.*` (performs file I/O)
- WHEN `Tests/BerlinTests.jucer`'s file list is inspected
- THEN `MidiExportTimeline.*` is registered by relative path
- AND `MidiFileWriter.*` is absent, because it performs file I/O regardless of which modules its declared dependencies now satisfy

#### Scenario: Synth DSP math is unit-tested while device-touching code is not
- GIVEN `Source/synth/*` DSP components that depend only on `juce_core`, `juce_dsp`, `juce_audio_formats`, and/or `juce_audio_basics`
- WHEN `Tests/BerlinTests.jucer`'s file list is inspected
- THEN those synth DSP files are registered by relative path
- AND the module list includes `juce_dsp`, `juce_audio_formats`, and `juce_audio_basics` in addition to `juce_core`

#### Scenario: Previously-excluded device/file-writing code stays excluded
- GIVEN `Source/export/MidiFileWriter.*` (file I/O) and `Source/midi/MidiOutputSink.*` (depends on `juce_audio_devices`)
- WHEN `Tests/BerlinTests.jucer`'s file list is inspected after the module list expands
- THEN `MidiFileWriter.*` and `MidiOutputSink.*` remain unregistered and manual-gate-only, because adding `juce_dsp`/`juce_audio_formats`/`juce_audio_basics` for synth DSP does not retroactively qualify file-I/O-performing or `juce_audio_devices`-dependent files for inclusion
