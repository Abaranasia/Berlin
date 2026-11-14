# Archive Report: step-gate

**Change**: step-gate (Per-Step Gate Length End to End)
**Date**: 2026-11-14
**Mode**: hybrid (Engram + openspec filesystem)
**Branch**: feat/step-gate
**Implementation commit**: ea3296b "feat: add per-step gate length"
**Prerequisite**: step-velocity archived in 3abc2ab; its deltas are the base of the main specs merged here.
**Archive folder move**: performed by the orchestrator (`openspec/changes/step-gate/` to `openspec/changes/archive/2026-11-14-step-gate/`). This phase did not move or copy the change folder.

## Gates

- **Task completion**: `openspec/changes/step-gate/tasks.md` has 39 of 39 tasks marked `[x]`. Task 11.2 was marked by this phase. Manual checks 10.1 to 10.5 were confirmed by the user on 2026-11-14. Task 11.1 (archive order) is complete.
- **Review receipt**: `.git/gentle-ai/review-transactions/v2/review-3775692d156d02cc/review-receipt.json` read and confirmed: `terminal_state: approved`, `lineage_id: review-3775692d156d02cc`, `risk_level: high`, lenses review-risk, review-resilience, review-readability, review-reliability, `final_candidate_tree: 6012d22ed50469f1f738edcf7f5c86e40e0576b1`, `paths_digest: sha256:81b773261b68b3525b5bc8b8ae6877b81c7e87055d689fa49ce2c533cf109036`, `policy_hash: sha256:34fb63d7f29f8613cd4431382b1057398a4816f8a4c20fc34677fffc80a184f6`, `evidence_hash: sha256:1b596b879e25bda7566de53c5d93f5d7f282faf5b939415c2596fe67d47521ab`. The `gentle-ai review validate --gate pre-commit` result (`allow`) was reported by the orchestrator and was not re-run in this phase. No Engram review topics exist, which is expected.
- **Verify**: `openspec/changes/step-gate/verify-report.md` is PASS WITH WARNINGS: 0 CRITICAL, 4 WARNING, 4 SUGGESTION.

## Engram Traceability (observation IDs)

| Artifact | Topic key | Observation ID | Note |
|---|---|---|---|
| Explore | sdd/step-gate/explore | 620 | |
| Proposal | sdd/step-gate/proposal | 624 | |
| Spec | sdd/step-gate/spec | 627 | |
| Design | sdd/step-gate/design | 628 | |
| Tasks | sdd/step-gate/tasks | 630 | Summary only. Its "32 of 39" count is stale. The file is authoritative (39 of 39). |
| Apply progress | sdd/step-gate/apply-progress | 633 | |
| Verify report | sdd/step-gate/verify-report | 635 | |
| Review topics | sdd/step-gate/review/* | none | Expected for this change |

## Main Specs Merged

| Capability | ADDED | MODIFIED | REMOVED | Main spec file |
|---|---|---|---|---|
| sequencing-core | 1 | 1 | 0 | `openspec/specs/sequencing-core/spec.md` |
| step-event-scheduling | 1 | 1 | 0 | `openspec/specs/step-event-scheduling/spec.md` |
| midi-export-timeline | 1 | 2 | 0 | `openspec/specs/midi-export-timeline/spec.md` |
| step-editing | 0 | 4 | 0 | `openspec/specs/step-editing/spec.md` |
| ui-bridge | 0 | 5 | 0 | `openspec/specs/ui-bridge/spec.md` |
| web-ui-controls | 1 | 4 | 0 | `openspec/specs/web-ui-controls/spec.md` |
| web-ui-layout | 1 | 0 | 0 | `openspec/specs/web-ui-layout/spec.md` |
| **Total** | **5** | **17** | **0** | 7 files |

Details:
- sequencing-core: MODIFIED Step Value Type (five fields, `gatePercent`). ADDED Generated Steps Use the Default Gate Length.
- step-event-scheduling: MODIFIED Note-Off Before Note-On At a Shared Boundary. ADDED Gate-Scaled Note-Off.
- midi-export-timeline: MODIFIED Ordered Events With Note-Off Before Note-On; MODIFIED Terminal Note-Off Correctness. ADDED Gate-Scaled Note-Off Ticks.
- step-editing: MODIFIED Absolute Note and Gate Assignment; MODIFIED Keep-Position Edit Playback; MODIFIED Interactive Step Grid; MODIFIED Velocity Is Session State.
- ui-bridge: MODIFIED Fixed Error Tokens; MODIFIED setStep Command; MODIFIED Encoding Conventions; MODIFIED Snapshot Contents; MODIFIED Message-Thread-Only Dispatch.
- web-ui-controls: MODIFIED Per-Step Coalescing With Busy Retry; MODIFIED Typed Protocol; MODIFIED Velocity Levels And Snapping; MODIFIED Mock Mirrors setStep Validation. ADDED Step Gate Cycle Button.
- web-ui-layout: ADDED Step Controls Hold Three Buttons.

Merge rules applied: each MODIFIED block replaces the whole requirement with the same title, with the delta's "(Previously: ...)" annotation dropped. ADDED blocks are appended to the end of the Requirements section. Requirements not named in a delta are untouched. All seven files were read back after editing and checked for duplicated scenarios and leftover Previously notes in the touched requirements.

## Non-Delta Edits (made on request)

- **Purpose lines**: `sequencing-core` now says velocity and per-step gate length exist, and that duration, probability and accent remain future work. `step-editing` now says "Per-step mute, note, skip, velocity and gate-length editing".
- **web-ui-layout, Step View**: the text "MainArea MUST show 16 steps" now reads "one cell per step of the current sequence (`steps.length`, 1 to 64 steps), wrapping into rows of 16". This is outside the step-gate delta, which is ADDED-only for web-ui-layout. The stale "(Previously: read-only ...)" note on that requirement was removed as part of the same edit.

## Follow-Ups (non-blocking, recorded for the next change)

1. `SequencePlayer` comments cite D2 to D5 without naming the design document.
2. Add a gate-specific event-buffer-capacity test. Coverage is currently indirect through the gate-100 capacity test.
3. Enter and Space keyboard coverage for the gate button is asserted only through the native `type="button"`. jsdom cannot synthesize the key-to-click path.
4. `nearestLevel` and `nextLevel` are generalized to `(levels, value)` without velocity wrappers (design D8). Task 8.4 wording differs slightly. Accepted in verify.
5. Gate values above 100, tie handling beyond the spec, and preset persistence of velocity and gate are deferred.
6. `openspec/specs/internal-synth-output/spec.md` (not modified by this change) contains the pre-existing absolute scenario "sustained full-level note does not clip with effects enabled". It conflicts with the known effects gain-staging issue tracked as the standalone clipping follow-up.

## Risks and Notes

- **Size**: verify WARNING 1 records about 1477 authored lines against the 800-line budget (forecast 530). The orchestrator reports the user accepted this by committing ea3296b. Recorded here for the audit trail.
- **Helper file**: verify required `Tests/Source/GateStreamSupport.h` to be staged with the change. The session-start git status showed only the change folder as untracked, which is consistent with the helper being committed. Not re-checked with git in this phase.
- **Stale Engram tasks topic**: observation 630 still reads "32 of 39" and lists Phase 10 and 11 as remaining. It was not updated. Its authoritative checklist is the file.
- **Pre-existing Previously notes**: several requirements not touched by this change still carry "(Previously: ...)" notes from earlier archives, for example in step-event-scheduling "Inactive Steps and Empty Sequence Emit Nothing", midi-export-timeline "Configurable Repeat Count", ui-bridge "Standalone-Only Settings Functions" and "Preset And Export Commands". They were left unchanged as out of scope.
- **Shell access**: this archive phase had no shell tool. Git state and the review validation command were not re-run here.
