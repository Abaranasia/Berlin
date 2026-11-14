# Archive Report: step-velocity

**Date**: 2026-11-14
**Mode**: hybrid (filesystem merge + Engram archive report)
**Status**: archived. The change folder move to `openspec/changes/archive/2026-11-14-step-velocity/` is performed by the orchestrator (`git mv`).
**Commit**: c0178968 "feat: add per-step velocity and accents" (short c017896). HEAD is currently `feat/step-gate` with ea3296b on top; the velocity commit sits on the `feat/step-velocity` lineage.

## Gates

| Gate | Evidence | Result |
|---|---|---|
| Task completion | `openspec/changes/step-velocity/tasks.md`: phases 0-10 all `[x]`, 11.1-11.3 `[x]` (user confirmed 2026-11-14). 12.1 is this archive step. No stale unchecked implementation tasks. | Pass, no reconciliation needed |
| Review receipt | `.git/gentle-ai/review-transactions/v2/review-9524fab7533c4b81/review-receipt.json`: `terminal_state: approved`, `final_candidate_tree` a87bce48ad5ca9ba8947edabd2ad155f9eef7154 (equals the staged tree a87bce4), `risk_level: high`, four lenses (review-risk, review-resilience, review-readability, review-reliability). Also present: `review-state.json`, `finalize-attempt-journal.json`. | Pass |
| Verify | `openspec/changes/step-velocity/verify-report.md` still reads FAIL with one CRITICAL (effects-on accent no-clip). Resolution per the orchestrator: the delta spec `specs/internal-synth-output/spec.md` was amended (effects-off absolute scenario, effects-on relative ceiling scenario, note on the pre-existing effects gain staging), and `specs/internal-synth-voice/spec.md` now states the 1.12 ceiling. Both amendments were confirmed in the delta files. | Pass by spec amendment; see Risks R1 and R2 |

## Engram Observation IDs (traceability)

| Artifact | Observation | Note |
|---|---|---|
| proposal | #480 | current |
| spec | #482 | stale: predates the spec amendments. Filesystem delta specs are authoritative |
| design | #483 | current |
| tasks | #484 | stale: phases 11-12 still pending in the observation |
| verify-report | #619 | matches the on-disk report (FAIL, 1 CRITICAL) |
| review (transaction, ledger, receipt, gate-context) | none | no `sdd/step-velocity/review/*` topics in Engram. The receipt was read from `.git/gentle-ai/review-transactions/...`. `transaction.json`, `ledger.json` and `gate-context.json` were not present in that directory (see R4) |

## Merged Capabilities

Delta specs merged into `openspec/specs/<cap>/spec.md`. Unrelated requirements were kept as they were.

| Capability | ADDED | MODIFIED | RENAMED | REMOVED | Purpose updated |
|---|---|---|---|---|---|
| sequencing-core | 1 (Generated Steps Use the Default Velocity) | 1 (Step Value Type) | 0 | 0 | yes |
| step-event-scheduling | 1 (Note-On Carries the Step Velocity) | 1 (Project-Local StepEvent Type) | 0 | 0 | no |
| midi-output-dispatch | 0 | 1 (Non-Allocating StepEvent-to-MidiBuffer Translation) | 0 | 0 | yes |
| internal-synth-voice | 1 (Velocity-Scaled Note Gain) | 0 | 0 | 0 | no |
| internal-synth-output | 0 | 2 (StepEventBuffer Consumption Alongside MIDI Dispatch, Mixing Without Clipping) | 0 | 0 | no |
| midi-export-timeline | 0 | 1 (Per-Step Velocity On Note-On) | 1 (Fixed Velocity From the Shared Constant) | 0 | no |
| midi-file-output | 0 | 1 (Fixed-Channel, Per-Event-Velocity Note Translation) | 1 (Fixed-Channel, Fixed-Velocity Note Translation) | 0 | yes |
| sequence-mutation | 1 (Mutations Preserve Step Velocity) | 0 | 0 | 0 | no |
| step-editing | 1 (Velocity Is Session State) | 3 (Absolute Note and Gate Assignment, Keep-Position Edit Playback, Interactive Step Grid) | 0 | 0 | yes (16-step line replaced) |
| ui-bridge | 0 | 5 (Fixed Error Tokens, setStep Command, Encoding Conventions, Snapshot Contents, Message-Thread-Only Dispatch) | 0 | 0 | no |
| web-ui-controls | 2 (Velocity Levels And Snapping, Mock Mirrors setStep Validation) | 2 (Per-Step Coalescing With Busy Retry, Typed Protocol) | 0 | 0 | no |
| **Total (11 capabilities)** | **7** | **17** (2 of them also RENAMED) | **2** | **0** | 4 Purpose lines |

### Main spec files modified

- `openspec/specs/sequencing-core/spec.md`
- `openspec/specs/step-event-scheduling/spec.md`
- `openspec/specs/midi-output-dispatch/spec.md`
- `openspec/specs/internal-synth-voice/spec.md`
- `openspec/specs/internal-synth-output/spec.md`
- `openspec/specs/midi-export-timeline/spec.md`
- `openspec/specs/midi-file-output/spec.md`
- `openspec/specs/sequence-mutation/spec.md`
- `openspec/specs/step-editing/spec.md`
- `openspec/specs/ui-bridge/spec.md`
- `openspec/specs/web-ui-controls/spec.md`

Delta annotations `(Previously: ...)`, `(Note: ...)`, `(Reason: ...)`, `(Migration: ...)` and the delta "Archive note" preambles were not copied into the main specs.

## Purpose Line Corrections

- step-editing: "limited to the visible 16 steps" replaced with "Per-step mute, note, skip and velocity editing of the live sequence (up to 64 steps)".
- sequencing-core: now states velocity (1..127, default 100), the skip flag, variable length up to `kMaxSteps` = 64, and the remaining future fields (gate length, duration, probability, timing-offset, accent).
- midi-output-dispatch: "fixed velocity" replaced with "per-event velocity". Notes that skipped steps never reach translation.
- midi-file-output: "fixed channel/velocity" replaced with "fixed channel, per-event velocity".

## Tasks

`tasks.md` marks 12.1 `[x]` at archive time, since the archive is that task. All implementation (0.1-10.3) and manual (11.1-11.3) tasks are closed.

## Follow-ups (not blocking archive)

1. effects-path gain staging peaks above 1.0 at velocity 100 (pre-existing, not caused by velocity). After it is fixed, add an absolute effects-on no-clip test.
2. Stale comment at `Tests/Source/MidiExportTimelineTests.cpp:76` (`offVelocities`).
3. The 1..127 clamp is duplicated in `MidiFileWriter.cpp`, `MidiMessageBytes.h` and `SynthPatch.h`. It should use the `Step.h` constants.
4. No dedicated `MidiEventTranslator` or `MidiFileWriter` test files. Coverage is through processor processBlock and `MidiExportTimeline` round-trip tests (tasks 3.2 and 5.2 are annotated as deviations).

## Risks

- **R1 (CRITICAL resolution by spec amendment)**: `verify-report.md` on disk still says FAIL with one CRITICAL. The archive relies on the amended delta. The verify report was not re-run or edited, to keep the audit trail intact. Consider a short addendum to the verify report.
- **R2 (internal contradiction left in main spec)**: `internal-synth-output` keeps the pre-existing scenario "Sustained full-level note does not clip", which requires `no sample exceeds the valid output range` with effects enabled. The amended requirement says absolute no-clip with effects enabled is out of scope, and the `(Note:)` line was dropped per the merge rule. The effects-on sustained scenario is therefore unmet and contradicts the amended intent. It needs a decision: amend that scenario to effects-bypassed, or fix the effects gain staging (follow-up 1).
- **R3 (pre-existing annotations)**: `(Previously: ...)` annotations from earlier archives remain in requirements this change did not touch (for example step-editing "Step Index Limit", and several ui-bridge and web-ui-controls requirements). They were left unchanged to keep unrelated requirements byte-for-byte. The requirements this change modified contain no annotations. A separate cleanup can strip them.
- **R4 (review artifacts)**: `transaction.json`, `ledger.json` and `gate-context.json` are not in the review directory, and no `sdd/step-velocity/review/*` topics exist in Engram. The gate was verified from `review-receipt.json` and `review-state.json`, as the orchestrator instructed.
- **R5 (branch lineage)**: the velocity commit is on the `feat/step-velocity` lineage. HEAD is `feat/step-gate` with ea3296b on top. This is informational only.
- **R6 (no shell in this run)**: the commit hash and branch facts were checked through `.git/logs/HEAD` and the review directory, not through `git` commands.

## Verification of the Merge

- Each of the 11 main specs was read at its pre-change state, and each delta block was applied by requirement name.
- Each MODIFIED block replaces the full requirement. ADDED blocks are appended at the end of the Requirements section, except that the RENAMED blocks replace the old name in place.
- No change folder was moved or copied. No `Source/`, `Tests/`, `ui/` or `openspec/changes/step-gate/` file was touched.
- Nothing was committed.
