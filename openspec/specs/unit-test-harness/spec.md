# Unit Test Harness Specification

## Purpose

A runnable, headless console test target for Berlin, modeled on `JUCE/extras/UnitTestRunner`, that discovers and runs all `juce::UnitTest` suites and reports success/failure via process exit code — enabling CTest-less CI/automation and strict TDD red/green cycles for the domain and generation code.

## Requirements

### Requirement: Console Test Runner Project

The system MUST provide a sibling Projucer project `Tests/BerlinTests.jucer` with `projectType="consoleapp"` and the preprocessor definition `JUCE_UNIT_TESTS=1`, built independently from `Berlin.jucer`, sharing all JUCE-free source tiers — currently `Source/core/*`, `Source/generation/*`, `Source/playback/*`, `Source/midi/*`, and the JUCE-free half of `Source/export/*` — by relative-path file registration (no source duplication). A source tier or file that depends on any module beyond `juce_core` (e.g. `Source/export/MidiFileWriter.*`, which depends on `juce_audio_basics`'s `juce::MidiFile`) MUST NOT be registered in this project and stays manual-gate-only, consistent with the harness's `juce_core`-only module list.

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
