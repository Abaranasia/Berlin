# Archive Report: step-model (Variable Length + True Skip)

**Change**: step-model
**Archive date**: 2026-10-17
**Archive mode**: intentional, with warnings (see Override)
**Branch**: feat/step-model
**Base commit**: 0baf295 (fix: re-apply window limits when the display scale changes)
**Feature commit**: 523e07b (feat: add variable sequence length and skip steps)

## Summary

Step-model Slice 1 adds a variable sequence length (1..64, default 16) and a true skip flag on each step. A skipped step is removed from the playback and export cycle and takes no time. Velocity and gate are deferred to the follow-up change `step-dynamics`. Length and skips are not persisted (schema v4 is deferred).

## Verification Evidence

| Check | Result |
|-------|--------|
| C++ tests (`BerlinTests.exe --category=Berlin`) | 495/495 pass |
| Vitest (`pnpm test` in `ui/`) | 554/554 pass |
| `pnpm tsc` | clean |
| Verify verdict | PASS WITH WARNINGS (0 CRITICAL, 5 WARNING, 2 SUGGESTION) |
| Golden reproducibility (ReproducibilityTests) | unchanged |
| Manual checks (tasks 9.1) | passed, run by the user on 2026-10-17 |
| Diff size (tasks 8.3) | ~1333 added / 172 deleted; over the 800-line budget, `size:exception` accepted by the user |

Verify warnings carried into this archive:
- No TDD cycle evidence table in apply-progress.
- Diff size exceeds the 800-line budget (accepted as `size:exception`).
- Manual checks were pending at verify time; they were completed later (see above).
- Indirect coverage only for "Generate discards skips" and for bridge-level index > 16 and steps-length behavior.

## Override (user-authorized, 2026-10-17)

The native dispatcher reported archive as blocked with "bounded review transaction is missing". The cause is a known tooling limit: `gentle-ai review bind-sdd` requires the post-apply gate, which stops returning `allow` once the reviewed files are committed. The same limit affected earlier slices.

The user authorized archiving anyway. Basis:
- Review lineage `review-205e0411b68ceb3e` is APPROVED: full 4R review, 0 blocking findings.
- Pre-commit gate allowed commit 523e07b.
- The dispatcher was NOT run.

Engram topics `sdd/step-model/review/{transaction,ledger,receipt,gate-context}` were not found when this archive ran. The override covers this gap. The review lineage ID above is recorded here for traceability.

## Tasks

All implementation tasks in `openspec/changes/step-model/tasks.md` are checked (phases 0-10). No stale-checkbox reconciliation was needed.

## Observation IDs (Engram, project `berlin`)

| Artifact | Observation ID | Topic key |
|----------|----------------|-----------|
| proposal | 462 | sdd/step-model/proposal |
| spec | 463 | sdd/step-model/spec |
| design | 466 | sdd/step-model/design |
| tasks | 467 | sdd/step-model/tasks |
| verify-report | 471 | sdd/step-model/verify-report |
| review transaction / ledger / receipt / gate-context | not found | sdd/step-model/review/* |
| archive-report | written on save | sdd/step-model/archive-report |

## Main Specs Synced (openspec/specs/)

Delta specs were merged from `openspec/changes/step-model/specs/<capability>/spec.md`. MODIFIED requirements replaced the same-named requirement in full. ADDED requirements were appended. No REMOVED requirements exist in this change.

| Capability | ADDED | MODIFIED | Notes |
|------------|-------|----------|-------|
| sequencing-core | 1 (Sequence Capacity Constant) | 1 (Step Value Type) | `Sequence Capacity Constant` was listed under MODIFIED but had no main counterpart, so it was appended |
| deterministic-generation | 1 (Variable Sequence Length Parameter) | 3 (Seed-In Reproducibility Contract, Production Composition of a Rhythm Source and PitchGenerator, Per-Step Editing and Preset Persistence Are Explicit Non-Goals This Slice) | |
| euclidean-rhythm | 0 | 1 (Fixed Step Count This Slice) | |
| step-event-scheduling | 2 (Skipped Steps Leave the Cycle, Keep-Position Publish With Skip and Length Changes) | 4 (Inactive Steps and Empty Sequence Emit Nothing, Continuous Emission Across the Loop Wrap, Step Position Wraps Indefinitely, Loop Completion Counter) | |
| auto-evolution | 0 | 1 (Mutation Scheduled by Loop Completions) | |
| midi-export-timeline | 1 (Skipped Steps Take Zero Ticks) | 3 (Terminal Note-Off Correctness, Configurable Repeat Count With Contiguous Seams, Inactive Steps and Degenerate Inputs Emit No Notes) | |
| sequence-mutation | 1 (Mutations Respect Skipped Steps) | 1 (Change-Note and Add-Note Stay In-Pattern) | |
| step-editing | 0 | 4 (Step Index Limit, Absolute Note and Gate Assignment, Keep-Position Edit Playback, Interactive Step Grid) | |
| generation-live-control | 1 (Sequence Length Control) | 2 (Generate Rebuilds the Sequence From the Current Seed and Restarts Playback, Audio-Thread-Safe Regeneration Handoff) | |
| ui-bridge | 0 | 4 (setStep Command, Encoding Conventions, Snapshot Contents, Message-Thread-Only Dispatch) | Encoding and Message-Thread blocks gained new scenarios |
| web-ui-controls | 0 | 5 (Per-Step Coalescing With Busy Retry, Typed Protocol, Transport Tempo And Level Controls, Live Generation Params, Generation Seed And Rhythm Controls) | |

Totals: 7 ADDED (including the appended Sequence Capacity Constant), 25 MODIFIED, 0 REMOVED.

Changed files (all under `openspec/specs/`): sequencing-core, deterministic-generation, euclidean-rhythm, step-event-scheduling, auto-evolution, midi-export-timeline, sequence-mutation, step-editing, generation-live-control, ui-bridge, web-ui-controls (`spec.md` in each).

## Follow-ups

1. **step-dynamics** change: per-step velocity and gate length.
2. **Schema v4 persistence** of `length` and skip flags in presets and state.
3. **MutationEngine stale comments**: `Source/generation/MutationEngine.cpp:243` and `Source/generation/MutationEngine.h:55,58,76` still say "[4, 64]" where the bound is now `kMaxSteps`. Found in the final review as a SUGGESTION, not blocking.
4. **Processor test** "Generate discards skips": only indirect coverage today; add a direct test.
5. **Bridge tests at lengths other than 16**: index > 16 and steps-length paths need direct coverage.

## Risks

- Review receipt topics are missing from Engram. The archive relied on the user's override plus the lineage ID above.
- The tooling limit in `gentle-ai review bind-sdd` will recur for every slice that commits before archive. It needs a tooling fix.
- The change folder was NOT moved by this phase. The orchestrator will `git mv` it to `openspec/changes/archive/2026-10-17-step-model`. No files were created under `openspec/changes/archive/`.

## Archive Contents

- proposal.md
- specs/ (11 delta specs)
- design.md
- tasks.md (all checked)
- verify-report.md
- explore.md
- archive-report.md (this file)
