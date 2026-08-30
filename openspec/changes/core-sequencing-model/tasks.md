# Tasks: Core Sequencing Model

## Review Workload Forecast

| Field | Value |
|-------|-------|
| Estimated changed lines | ~1000-1100 authored (excl. generated `Builds/`/`JuceLibraryCode/`) |
| 400-line budget risk | High |
| Chained PRs recommended | Yes |
| Suggested split | PR1 harness -> PR2 core -> PR3 generation+e2e+final gate |
| Delivery strategy | ask-on-risk |
| Chain strategy | feature-branch-chain |

Decision needed before apply: Yes
Chained PRs recommended: Yes
Chain strategy: feature-branch-chain
400-line budget risk: High

### Suggested Work Units

| Unit | Goal | Likely PR | Focused test command | Runtime harness | Rollback boundary |
|------|------|-----------|----------------------|-----------------|-------------------|
| 1 | Harness green/red proof, no domain code | PR 1 (base: tracker) | `BerlinTests.exe --category=Berlin` | Console run, check exit code | Delete `Tests/` tree, no other file touched |
| 2 | `Source/core` (Step, Sequence, Scale) + tests | PR 2 (base: PR1 branch) | `BerlinTests.exe --category=Berlin --name=Step\|Sequence\|Scale` | Same console run | Delete `Source/core/*`, revert both `.jucer` FILE entries for core |
| 3 | `Source/generation` + reproducibility + final gate | PR 3 (base: PR2 branch) | `BerlinTests.exe --category=Berlin` (full) | Console run + `Berlin.exe` GUI launch | Delete `Source/generation/*`, revert generation FILE entries, revert `Berlin.jucer` |

## Phase 0: Test Harness (binding first step — no domain code yet)

- [x] 0.1 Create `Tests/BerlinTests.jucer` (`consoleapp`, `JUCE_UNIT_TESTS=1`, `juce_core` only, `useAppConfig="0"`, `addUsingNamespaceToJuceHeader="0"`, `useGlobalPath="1"`, VS2026 exporter -> `Builds/VisualStudio2026`, `headerPath="../../Source"`). DoD: file exists, well-formed XML.
- [x] 0.2 Create `Tests/Source/Main.cpp` adapted from `JUCE/extras/UnitTestRunner/Source/Main.cpp`: `juce::`-qualify all symbols, `#include <juce_core/juce_core.h>` (no `<JuceHeader.h>`), drop `DeletedAtShutdown::deleteAll()`, keep `--help/-l/-c/-n/-s` + 0/1 exit code. DoD: compiles once linked.
- [x] 0.3 RED: Create `Tests/Source/SmokeTests.cpp`, one `juce::UnitTest` category `"Berlin"` asserting a deliberate failure (`expect(false)`). Register `<FILE>` in `Tests/BerlinTests.jucer`. Run `Projucer.exe --resave Tests/BerlinTests.jucer` (check exit code 0), build, run `BerlinTests.exe --category=Berlin`: confirm exit code 1 (red proof).
- [x] 0.4 GREEN: Flip smoke assertion to a tautology (`expect(true)`). Rebuild, rerun: confirm exit code 0 (green proof, harness done).

## Phase 1: Step (first shared-source slice — retires `../` path risk)

- [x] 1.1 RED: `Tests/Source/StepTests.cpp` — construct/equality/no-extra-field-review-note tests; register `<FILE>` in `Tests/BerlinTests.jucer` only. `--resave` + build: confirm link failure (no `Step.h` yet). **Deviation**: the actual RED proof was a compile-time error (`C1083: cannot open include file 'core/Step.h'`), not a link error, because `StepTests.cpp` `#include`s `"core/Step.h"` directly rather than only referencing `berlin::Step` symbols. Still exit code 1, still proves `Step.h` doesn't exist yet — same RED intent, earlier failure point.
- [x] 1.2 GREEN: Create `Source/core/Step.h` (`berlin::Step{note,active}` + non-member `operator==`); `static_assert(std::is_aggregate_v<Step>)`.
- [x] 1.3 Register `<FILE file="Source/core/Step.h">` in `Berlin.jucer` and `<FILE file="../Source/core/Step.h">` in `Tests/BerlinTests.jucer`. Run `Projucer.exe --resave Berlin.jucer` then `Projucer.exe --resave Tests/BerlinTests.jucer`, check both exit codes 0. Sync-check the two FILE lists match modulo `../`. Build both projects, run `BerlinTests.exe --category=Berlin`: exit 0. **Deviation**: discovered and fixed a latent Phase-0 defect — `Tests/BerlinTests.jucer`'s `headerPath` was `"../../Source"` (2 levels), which resolves relative to the generated vcxproj directory (`Tests/Builds/VisualStudio2026`) to `Tests/Source`, not the repo-root `Source`. Corrected to `"../../../Source"` (3 levels). This is exactly the `../Source/...` path risk the design flagged Phase 1 as retiring.

## Phase 2: Sequence

- [ ] 2.1 RED: `Tests/Source/SequenceTests.cpp` (resize grow/shrink, indexed access); register+resave+build: confirm link failure.
- [ ] 2.2 GREEN: `Source/core/Sequence.h`/`.cpp` (`std::vector<Step>` backing).
- [ ] 2.3 Run full 6-step registration checklist (both `.jucer` FILE entries, both `--resave`, sync-check, both builds); `BerlinTests.exe --category=Berlin` exit 0.

## Phase 3: Scale

- [ ] 3.1 RED: `Tests/Source/ScaleTests.cpp` (major/minor factories, `contains` incl. notes below root, `getDegree`); register+resave+build: confirm failure.
- [ ] 3.2 GREEN: `Source/core/Scale.h`/`.cpp` (private ctor, floored-modulo `contains`).
- [ ] 3.3 Full registration checklist; `BerlinTests.exe --category=Berlin` exit 0.

## Phase 4: DeterministicRandom

- [ ] 4.1 RED: `Tests/Source/DeterministicRandomTests.cpp` (`getSeed()`, same-seed same-draws, `static_assert(!std::is_default_constructible_v<DeterministicRandom>)`); register+resave+build: confirm failure.
- [ ] 4.2 GREEN: `Source/generation/DeterministicRandom.h` (header-only wrapper over `juce::Random`).
- [ ] 4.3 Full registration checklist; exit 0.

## Phase 5: PitchGenerator

- [ ] 5.1 RED: `Tests/Source/PitchGeneratorTests.cpp` (in-range/in-scale, empty-range clamp + lower-note tie-break); register+resave+build: confirm failure.
- [ ] 5.2 GREEN: `Source/generation/PitchGenerator.h`/`.cpp` (ctor-time candidate list + fallback).
- [ ] 5.3 Full registration checklist; exit 0.

## Phase 6: RhythmGenerator

- [ ] 6.1 RED: `Tests/Source/RhythmGeneratorTests.cpp` (size==numSteps, only `active` set, density 0.0/1.0 endpoints, statistical mean-density across seeds); register+resave+build: confirm failure.
- [ ] 6.2 GREEN: `Source/generation/RhythmGenerator.h`/`.cpp` (one `nextFloat()` per step, ascending order, endpoint guards).
- [ ] 6.3 Full registration checklist; exit 0.

## Phase 7: End-to-End Reproducibility

- [ ] 7.1 RED: `Tests/Source/ReproducibilityTests.cpp` — 16 steps, root note, `Scale::minor`, seed 12345, generate twice via `RhythmGenerator`+`PitchGenerator`, assert `Sequence::operator==`; register+resave+build.
- [ ] 7.2 GREEN: confirm passes with existing implementations (no new production code expected); if it fails, fix the responsible generator only.
- [ ] 7.3 Full registration checklist; `BerlinTests.exe --category=Berlin` exit 0 (all suites).

## Phase 8: Final Gate

- [ ] 8.1 Run `Projucer.exe --resave Berlin.jucer`; check exit code 0.
- [ ] 8.2 Build `Berlin.jucer` (GUI app) in `Builds/VisualStudio2026`; check build succeeds.
- [ ] 8.3 Run `git diff --stat -- Source/Main.cpp Source/MainComponent.*`: confirm empty output (untouched).
- [ ] 8.4 Run `git diff --stat` overall and confirm only the file set from design's "File Changes" table appears.
