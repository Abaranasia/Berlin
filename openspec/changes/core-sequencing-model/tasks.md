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

- [x] 2.1 RED: `Tests/Source/SequenceTests.cpp` (resize grow/shrink, indexed access); register+resave+build: confirm link failure. **Deviation**: same pattern as 1.1 — actual RED proof was a compile-time error (`C1083: cannot open include file 'core/Sequence.h'`), not a link error, because `SequenceTests.cpp` `#include`s `"core/Sequence.h"` directly. Still exit code 1, still proves `Sequence.h` doesn't exist yet.
- [x] 2.2 GREEN: `Source/core/Sequence.h`/`.cpp` (`std::vector<Step>` backing).
- [x] 2.3 Run full 6-step registration checklist (both `.jucer` FILE entries, both `--resave`, sync-check, both builds); `BerlinTests.exe --category=Berlin` exit 0.

## Phase 3: Scale

- [x] 3.1 RED: `Tests/Source/ScaleTests.cpp` (major/minor factories, `contains` incl. notes below root, `getDegree`); register+resave+build: confirm failure. **Deviation**: same pattern as 1.1/2.1 — actual RED proof was a compile-time error (`C1083: cannot open include file 'core/Scale.h'`), not a link error, because `ScaleTests.cpp` `#include`s `"core/Scale.h"` directly. Still exit code 1, still proves `Scale.h` doesn't exist yet.
- [x] 3.2 GREEN: `Source/core/Scale.h`/`.cpp` (private ctor, floored-modulo `contains`).
- [x] 3.3 Full registration checklist; `BerlinTests.exe --category=Berlin` exit 0.

## Phase 4: DeterministicRandom

- [x] 4.1 RED: `Tests/Source/DeterministicRandomTests.cpp` (`getSeed()`, same-seed same-draws, `static_assert(!std::is_default_constructible_v<DeterministicRandom>)`); register+resave+build: confirm failure. **Deviation**: same pattern as Phases 1-3 — actual RED proof was a compile-time error (`C1083: cannot open include file 'generation/DeterministicRandom.h'`), not a link error, because `DeterministicRandomTests.cpp` `#include`s `"generation/DeterministicRandom.h"` directly. Still exit code 1, still proves `DeterministicRandom.h` doesn't exist yet.
- [x] 4.2 GREEN: `Source/generation/DeterministicRandom.h` (header-only wrapper over `juce::Random`; deleted default constructor; `getSeed`/`nextInt`/`nextFloat` per design.md's interface exactly).
- [x] 4.3 Full registration checklist; exit 0. New `generation` `<GROUP>` added to both `Berlin.jucer` and `Tests/BerlinTests.jucer` (mirrors the `core` group's convention, `../` prefix only). Sync-check passed (`Source/generation/DeterministicRandom.h` present in both, modulo `../`). Both projects resaved and built clean (0 warnings/0 errors); `BerlinTests.exe --category=Berlin` exit 0, all 20 beginTest blocks passed (1 Smoke + 4 Step + 5 Sequence + 6 Scale + 4 DeterministicRandom). `Berlin.jucer` GUI app also rebuilt clean.

## Phase 5: PitchGenerator

- [x] 5.1 RED: `Tests/Source/PitchGeneratorTests.cpp` (in-range/in-scale, empty-range clamp + lower-note tie-break); register+resave+build: confirm failure. **Deviation**: same pattern as Phases 1-4 — actual RED proof was a compile-time error (`C1083: cannot open include file 'generation/PitchGenerator.h'`), not a link error, because `PitchGeneratorTests.cpp` `#include`s `"generation/PitchGenerator.h"` directly. Still exit code 1, still proves `PitchGenerator.h` doesn't exist yet.
- [x] 5.2 GREEN: `Source/generation/PitchGenerator.h`/`.cpp` (ctor-time candidate list within `[rangeLow, rangeHigh]`; ctor swaps an inverted range; empty-candidate fallback expands outward one semitone at a time, checking the lower side first at each radius so an exact tie resolves to the LOWER note per design.md; `generateNextNote` spends one RNG draw to index candidates, or returns the fallback note consuming NO draw).
- [x] 5.3 Full registration checklist; exit 0. New `<FILE>` entries added to the existing `generation` `<GROUP>` in both `Berlin.jucer` (`Source/generation/PitchGenerator.h`/`.cpp`) and `Tests/BerlinTests.jucer` (`../Source/generation/PitchGenerator.h`/`.cpp`). Both projects resaved and sync-checked (identical entries modulo `../`), both built clean (0 warnings/0 errors); `BerlinTests.exe --category=Berlin` exit 0, all suites passed (Smoke, Step, Sequence, Scale, DeterministicRandom, PitchGenerator = 25 beginTest blocks). `Berlin.jucer` GUI app also rebuilt clean.

## Phase 6: RhythmGenerator

- [x] 6.1 RED: `Tests/Source/RhythmGeneratorTests.cpp` (size==numSteps, only `active` set, density 0.0/1.0 endpoints, statistical mean-density across seeds); register+resave+build: confirm failure. **Deviation**: same pattern as Phases 1-5 — actual RED proof was a compile-time error (`C1083: cannot open include file 'generation/RhythmGenerator.h'`), not a link error, because `RhythmGeneratorTests.cpp` `#include`s `"generation/RhythmGenerator.h"` directly. Still exit code 1, still proves `RhythmGenerator.h` doesn't exist yet. Also added a test not explicitly enumerated in the task list but required by design.md's per-step-draw guarantee: "density never shifts the RNG stream length" — verifies two same-seed `DeterministicRandom` instances stay in lock-step after `generate()` regardless of whether density 0.0 or 1.0 was used, proving the endpoint guards still consume one draw per step. The statistical mean-density test uses `numSteps=16`, `density=0.5`, 500 distinct seeds (8000 total draws), asserting `|mean - 8| < 0.75` — design.md specifies the convention ("many distinct seeds", "within statistical tolerance") but no exact sample count/tolerance, so these values were chosen to keep the test fast while the tolerance stays far outside plausible sampling noise (~0.09 std-dev of the mean at this sample size).
- [x] 6.2 GREEN: `Source/generation/RhythmGenerator.h`/`.cpp` (one `nextFloat()` per step, ascending order, endpoint guards). Guard logic per design.md exactly: `density >= 1.0f` → always active (never relies on `nextFloat()`'s upper bound excluding 1.0), `density <= 0.0f` → always inactive, else `draw < density` — the `nextFloat()` draw happens unconditionally before the guard branches so the per-step draw count never depends on density. Constructor clamps density to `[0, 1]` via `std::clamp`.
- [x] 6.3 Full registration checklist; exit 0. New `<FILE>` entries added to the existing `generation` `<GROUP>` in both `Berlin.jucer` (`Source/generation/RhythmGenerator.h`/`.cpp`) and `Tests/BerlinTests.jucer` (`../Source/generation/RhythmGenerator.h`/`.cpp`). Both projects resaved and sync-checked (identical entries modulo `../`), both built clean (0 warnings/0 errors); `BerlinTests.exe --category=Berlin` exit 0, all suites passed (Smoke, Step, Sequence, Scale, DeterministicRandom, PitchGenerator, RhythmGenerator = 32 beginTest blocks). `Berlin.jucer` GUI app also rebuilt clean (0 warnings/0 errors). `git diff --stat -- Source/Main.cpp Source/MainComponent.h Source/MainComponent.cpp Source/core` confirmed empty.

## Phase 7: End-to-End Reproducibility

- [x] 7.1 RED: `Tests/Source/ReproducibilityTests.cpp` — 16 steps, root note, `Scale::minor`, seed 12345, generate twice via `RhythmGenerator`+`PitchGenerator`, assert `Sequence::operator==`; register+resave+build. **Deviation**: unlike Phases 1-6, this task does NOT introduce a new type/header, so there is no compile-time RED proof available (no missing-include failure possible). Per design.md ("no production class composes these — the 16-step demo is a plain loop inside a juce::UnitTest"), the test file defines a file-local (anonymous-namespace) `generateFullSequence(numSteps, rootNote, DeterministicRandom&)` helper that: constructs `Scale::minor(rootNote)`, a `RhythmGenerator(numSteps, 0.5f)`, and a `PitchGenerator(scale, rootNote, rootNote+24)`; calls `rhythmGenerator.generate(random)` first (writes `active` for all steps), then for each active step only calls `pitchGenerator.generateNextNote(random)` to fill `note` — both draws share the SAME `DeterministicRandom&` instance, in that fixed order, matching design.md's data-flow diagram exactly. Registered `<FILE>` in `Tests/BerlinTests.jucer` only (no `Berlin.jucer` change — this test lives under `Tests/Source`, no new `Source/` file). Built first-try, 0 warnings/0 errors.
- [x] 7.2 GREEN: confirmed passes with existing implementations — no new production code was needed, no bug found. **Deviation/finding**: reproducibility held naturally on the first run (exit code 0, "Reproducibility / same seed produces an identical 16-step Sequence end-to-end" passed). Reasoning why no bug was possible given the existing `RhythmGenerator.cpp`/`PitchGenerator.cpp`: (1) `juce::Random`'s draw sequence is a pure function of seed + call order only, never of the bound/argument passed to `nextInt`/`nextFloat`; (2) `RhythmGenerator::generate` always consumes exactly one `nextFloat()` per step regardless of density (verified already by the Phase 6 "density never shifts the RNG stream length" test); (3) `PitchGenerator::generateNextNote` consumes exactly one `nextInt()` draw per active step unless the ctor-time `fallbackNote` path is taken (which depends only on the fixed scale+range, never on RNG state, so its emptiness is identical across both runs). Since two `DeterministicRandom(12345)` instances are independently constructed but draw in the exact same order through the exact same sequence of calls, the two `Sequence` results are provably identical — no fix needed to `RhythmGenerator.cpp` or `PitchGenerator.cpp`, `Source/core/*`, or `DeterministicRandom.h`. Also added an unenumerated second `beginTest` ("different seeds are permitted to produce a different Sequence") that only asserts both sequences have the correct size — deliberately does NOT assert inequality (would be a false invariant), keeping the suite honest about what is/isn't guaranteed.
- [x] 7.3 Full registration checklist: `<FILE id="btRprC" name="ReproducibilityTests.cpp">` added to `Tests/BerlinTests.jucer`'s `Source` GROUP only (no `Berlin.jucer` entry needed — test-only file, not under `Source/`). `Projucer.exe --resave Tests/BerlinTests.jucer` exit 0. Built `Tests/Builds/VisualStudio2026/BerlinTests.sln` (Debug|x64) clean, 0 warnings/0 errors. `BerlinTests.exe --category=Berlin` exit 0, all suites passed (Smoke, Step, Sequence, Scale, DeterministicRandom, PitchGenerator, RhythmGenerator, Reproducibility = 34 beginTest blocks total, 2 new). `Berlin.jucer` GUI app (`Berlin.sln` Debug|x64) also rebuilt clean, 0 warnings/0 errors — untouched since this phase touched no `Source/` file. `git diff --stat -- Source/Main.cpp Source/MainComponent.h Source/MainComponent.cpp Source/core Berlin.jucer` confirmed empty.

## Phase 8: Final Gate

- [ ] 8.1 Run `Projucer.exe --resave Berlin.jucer`; check exit code 0.
- [ ] 8.2 Build `Berlin.jucer` (GUI app) in `Builds/VisualStudio2026`; check build succeeds.
- [ ] 8.3 Run `git diff --stat -- Source/Main.cpp Source/MainComponent.*`: confirm empty output (untouched).
- [ ] 8.4 Run `git diff --stat` overall and confirm only the file set from design's "File Changes" table appears.
