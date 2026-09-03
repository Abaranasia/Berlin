# Delta for Unit Test Harness

## MODIFIED Requirements

### Requirement: Console Test Runner Project

The system MUST provide a sibling Projucer project `Tests/BerlinTests.jucer` with `projectType="consoleapp"` and the preprocessor definition `JUCE_UNIT_TESTS=1`, built independently from `Berlin.jucer`, sharing all JUCE-free source tiers — currently `Source/core/*`, `Source/generation/*`, `Source/playback/*`, `Source/midi/*`, and the JUCE-free half of `Source/export/*` — by relative-path file registration (no source duplication). A source tier or file that depends on any module beyond `juce_core` (e.g. `Source/export/MidiFileWriter.*`, which depends on `juce_audio_basics`'s `juce::MidiFile`) MUST NOT be registered in this project and stays manual-gate-only, consistent with the harness's `juce_core`-only module list.

(Previously: named only `Source/core/*` and `Source/generation/*` as the shared tiers, without a general "all JUCE-free tiers" statement or an explicit rule excluding JUCE-aware files.)

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

- GIVEN `Source/export/MidiExportTimeline.*` (JUCE-free) and `Source/export/MidiFileWriter.*` (depends on `juce_audio_basics`)
- WHEN `Tests/BerlinTests.jucer`'s file list is inspected
- THEN `MidiExportTimeline.*` is registered by relative path
- AND `MidiFileWriter.*` is absent, and the project's module list remains `juce_core`-only
