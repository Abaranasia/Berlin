# SDD Archive Report: core-sequencing-model

**Archived**: 2026-08-30  
**Change**: core-sequencing-model  
**Status**: CLOSED — all 29 tasks complete, verified PASS, specs merged, change archived

## Executive Summary

The core-sequencing-model change has been fully implemented, independently verified (PASS verdict, 0 CRITICAL issues), and archived. Three new domain specifications (sequencing-core, deterministic-generation, unit-test-harness) have been merged into the project's main specs directory (`openspec/specs/`). The change folder has been moved to the archive directory and the SDD cycle is complete.

## Change Overview

**Scope**: First vertical slice of Berlin School generative sequencer.

**Deliverables**:
- Domain model: `Step` value type, `Sequence` resizable collection, `Scale` with major/minor factories
- Seeded generation: `DeterministicRandom`, `PitchGenerator`, `RhythmGenerator` with deterministic reproducibility contract
- Unit test harness: Projucer console test runner (`Tests/BerlinTests.jucer`) modeled on `JUCE/extras/UnitTestRunner`

**Work completed**: 29/29 tasks across 8 phases (Phase 0 harness → Phase 1-6 domain → Phase 7 reproducibility → Phase 8 final gate)

**Verification**: Independent spot-check of all artifacts, full suite execution (34 test cases, all passing), code inspection of design coherence decisions. Verdict: **PASS** with 0 CRITICAL, 2 low-risk WARNINGs (both transparent and documented).

## Specs Merged (Main Specs Created)

Since this is the project's first change, main specs did not exist prior. Delta specs from the change were copied directly as full specifications to the new main specs location:

| Domain | Source Path | Destination Path | Requirements | Status |
|--------|-------------|------------------|--------------|--------|
| sequencing-core | `openspec/changes/core-sequencing-model/specs/sequencing-core/spec.md` | `openspec/specs/sequencing-core/spec.md` | 9 scenarios (Step, Sequence, Scale) | Created |
| deterministic-generation | `openspec/changes/core-sequencing-model/specs/deterministic-generation/spec.md` | `openspec/specs/deterministic-generation/spec.md` | 10 scenarios (DeterministicRandom, PitchGenerator, RhythmGenerator, reproducibility) | Created |
| unit-test-harness | `openspec/changes/core-sequencing-model/specs/unit-test-harness/spec.md` | `openspec/specs/unit-test-harness/spec.md` | 7 scenarios (console runner, discovery, exit-code, seed) | Created |

**Total merged**: 26 scenarios across 3 new specifications, all verified compliant by independent verification run.

## Task Completion

All 29 implementation tasks marked complete with checkboxes:

- Phase 0 (Test Harness): 4 tasks ✓ — Projucer console app scaffolding, red/green proof
- Phase 1-6 (Domain Model): 18 tasks ✓ — `Step`, `Sequence`, `Scale`, `DeterministicRandom`, `PitchGenerator`, `RhythmGenerator` with full TDD lifecycle each
- Phase 7 (Reproducibility): 3 tasks ✓ — End-to-end seed reproducibility test and verification
- Phase 8 (Final Gate): 4 tasks ✓ — Final Projucer resave, build confirmation, file diff validation

**Verification**: 29/29 tasks independently re-verified in `verify-report.md`:
- All claimed artifacts (files, `.jucer` entries) exist and match descriptions
- RED/GREEN cycles documented with honest deviation notes
- All 34 `juce::UnitTest` cases execute and pass (exit code 0)
- Design coherence decisions (Scale private ctor, floored-modulo contains, PitchGenerator tie-break, RhythmGenerator endpoint guards, DeterministicRandom no-default-ctor) all verified against implementation

No stale unchecked tasks remain.

## Verification Summary

**Verdict**: PASS  
**Date verified**: 2026-08-30  
**Branch verified**: chore/add-openspec at commit 5d78126

**Key findings**:
- 29/29 tasks genuinely complete with spot-checked evidence
- 34/34 unit test cases pass (Smoke, Step, Sequence, Scale, DeterministicRandom, PitchGenerator, RhythmGenerator, Reproducibility suites)
- All 26 spec scenarios verified compliant via runtime execution or code inspection
- Both Projucer projects resaved byte-idempotent, rebuilt from clean with 0 errors (GUI app only pre-existing warnings)
- Source/Main.cpp and Source/MainComponent.* confirmed untouched across entire change
- Design coherence: 8 architectural decisions spot-checked, all implemented as specified

**Issues**:
- CRITICAL: None
- WARNING (non-blocking): 1 tautology-pattern assertion for non-runtime-testable "Step has no extra fields" (already documented in design, confirmed by code inspection); 1 assertion-failure exit path not re-triggered (covered by code inspection + equivalent proof via no-match-filter path)
- SUGGESTION: design.md's File Changes table does not explicitly enumerate SmokeTests.cpp (cosmetic); Phase 1-6 RED proofs are compile-time missing-include rather than link-time (weaker but valid, honestly documented)

**Evidence artifacts in change folder**:
- `verify-report.md` — full independent verification with all spot-checks and build/test evidence
- `verify-report-phase0.md` — historical Phase 0 (PR1 harness) verification, superseded by full report
- `tasks.md` — complete 29-task record with all RED/GREEN narratives and deviation notes

## Archive Contents

Change folder moved to: `openspec/changes/archive/2026-08-30-core-sequencing-model/`

**Archived artifacts**:
- ✅ `proposal.md` — Change intent, scope, approach, risks, rollback plan
- ✅ `design.md` — Technical decisions, interface contracts, architecture rationale
- ✅ `tasks.md` — 29 tasks, 8 phases, TDD red/green narrative with deviations
- ✅ `exploration.md` — Pre-proposal exploration notes (historical)
- ✅ `specs/sequencing-core/spec.md` — Delta spec (now synced to main)
- ✅ `specs/deterministic-generation/spec.md` — Delta spec (now synced to main)
- ✅ `specs/unit-test-harness/spec.md` — Delta spec (now synced to main)
- ✅ `verify-report.md` — Full verification evidence
- ✅ `verify-report-phase0.md` — Phase 0 historical verification

**Audit trail**: Complete SDD lifecycle (proposal → design → tasks → implementation via 8 phases → verification → archive) is preserved in the archive folder.

## Source of Truth Updated

The following locations now reflect the change's delivered behavior:

1. `openspec/specs/sequencing-core/spec.md` — Primary spec for `Step`, `Sequence`, `Scale` types
2. `openspec/specs/deterministic-generation/spec.md` — Primary spec for `DeterministicRandom`, `PitchGenerator`, `RhythmGenerator`, reproducibility contract
3. `openspec/specs/unit-test-harness/spec.md` — Primary spec for console test runner and exit-code contract

All future domain work references these main specs. The archived change folder serves as the design rationale and historical context for these specs.

## Review Gate

**Review status**: PASS verdict from independent verification.  
**Artifacts**: 29/29 tasks complete, 0 CRITICAL issues in verify-report, all 26 scenarios compliant.  
**No review issues block archival**.

## SDD Cycle Complete

| Phase | Status | Completion |
|-------|--------|-----------|
| Proposal | Done | `sdd-proposal` — intent and scope settled |
| Specification | Done | `sdd-spec` — 3 domain specs drafted |
| Design | Done | `sdd-design` — architecture and interfaces defined |
| Tasks | Done | `sdd-tasks` — 29 tasks planned across 8 phases |
| Implementation | Done | `sdd-apply` — all 29 tasks completed, committed, pushed |
| Verification | Done | `sdd-verify` — independent PASS verdict |
| Archive | Done | `sdd-archive` — specs merged, change archived, cycle closed |

**Ready for**: The next change. `openspec/specs/` now has established baseline specifications. Any follow-up work (velocity fields, gate-time duration, mutation engine, etc.) will reference and extend these established specs.

## Notes for Future Maintainers

1. **Reproducibility durability**: The `DeterministicRandom` wrapper and the entire generation contract are guaranteed only within a single build (same JUCE version, same binary). Not guaranteed across JUCE upgrades. This is an explicit non-goal for this change; if durability across versions becomes required, the decision must be revisited.

2. **Density semantics**: `density` is per-step independent probability, not a guaranteed active-step count. A 16-step sequence with density 0.5 activates on average 8 steps, but the exact count is seed-dependent (7, 8, 9, etc.). This is the settled semantics per design decision 8.

3. **Scale factory set**: Only `major` and `minor` factories exist. Modal scales (dorian, phrygian, pentatonic, harmonic minor, etc.) are explicit future additive work and do not break the current API when added.

4. **Shared source registration**: `Source/core/*` and `Source/generation/*` are registered by relative path in both `Berlin.jucer` and `Tests/BerlinTests.jucer`. No source duplication. If a new shared file is added, both projects' file lists must be updated and resaved via `Projucer.exe --resave`.

5. **Test harness stability**: The `Tests/BerlinTests.jucer` console runner and its `Main.cpp` are modeled on `JUCE/extras/UnitTestRunner`. Both are stable artifacts; the test harness is not expected to change frequently.

## Closure

This SDD change (core-sequencing-model) is now **closed**. All work is committed, verified, archived, and the main specs are the new source of truth. The SDD cycle is complete. Ready for the next change.

---

**Archived by**: sdd-archive executor  
**Date**: 2026-08-30  
**Verification reference**: verify-report.md (0 CRITICAL, 2 low-risk WARNINGs)  
**Specs created**: 3 (sequencing-core, deterministic-generation, unit-test-harness)  
**Archive folder**: openspec/changes/archive/2026-08-30-core-sequencing-model/
