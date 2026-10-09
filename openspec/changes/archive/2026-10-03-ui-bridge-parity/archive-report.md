# Archive Report: ui-bridge-parity

**Change**: ui-bridge-parity (UI Redesign Slice 2/6)  
**Archived to**: `openspec/changes/archive/2026-10-03-ui-bridge-parity/`  
**Archive date**: 2026-10-03  
**Mode**: hybrid (openspec + Engram)

## Execution Summary

All phases (0-11, 12b, 13-17, 19-21, 22b, 23) completed successfully. Verification: PASS WITH WARNINGS (0 CRITICAL). All implementation tasks proven complete by approved reviews and verified tests. Stale process-task checkboxes (12.1-12.5, 18.1, 18.3-18.6, 22.1, 22.3-22.5) were annotated and ticked in commit 2331e00 (verify W1); 12.6 and 22.6 were closed by maintainer decision in commit 619a3df (see Gate Overrides).

## Spec Merges Summary

| Capability | Action | Changes |
|---|---|---|
| web-editor | MODIFIED + ADDED | 5 new requirements (Export File Chooser, Confirm, Dialog Lifetime Safety, Snapshot Event, UI Content Defers); 1 removed (PoC UI Content, replaced by pointer to web-ui-controls) |
| ui-bridge | MODIFIED + ADDED | 1 modified (Preset And Export Commands - added loadPreset busy behavior); 1 new (Snapshot presetNames Is Always Current) |
| preset-persistence | ADDED | 1 new (Preset Listing Caches Only Per-File Validity) |
| web-ui-controls | CREATED (NEW) | Full 26-requirement spec covering protocol, bridge, store, optimistic updates, controls, testing |

Main specs updated:
- `openspec/specs/web-editor/spec.md` (merged 5 ADDED + 1 REMOVED)
- `openspec/specs/ui-bridge/spec.md` (merged 1 MODIFIED + 1 ADDED)
- `openspec/specs/preset-persistence/spec.md` (merged 1 ADDED)
- `openspec/specs/web-ui-controls/spec.md` (created new)

## Archive Contents

- [x] `explore.md` (exploration report)
- [x] `proposal.md` (scope and approach)
- [x] `design.md` (architecture and decisions)
- [x] `tasks.md` (phases 0-23 plus 12b and 22b; no unchecked tasks)
- [x] `verify-report.md` (PASS WITH WARNINGS)
- [x] `specs/web-editor/spec.md` (delta)
- [x] `specs/ui-bridge/spec.md` (delta)
- [x] `specs/preset-persistence/spec.md` (delta)
- [x] `specs/web-ui-controls/spec.md` (new full spec)

All nine files are byte-identical to the committed versions at 619a3df (checked with `git show HEAD:<path> | cmp`).

## Verification Status

**Verdict**: PASS WITH WARNINGS (0 CRITICAL, 5 WARNING, 3 SUGGESTION)

**Test Results**:
- BerlinTests: 379/379 passing (C++ unit tests)
- Vitest: 381/381 passing (29 TS test files)
- TypeScript: clean (tsc --noEmit)
- Vite build: success (no mock chunk)

**Spec Compliance**: All 125 requirement/scenario headings across 4 specs compliant or verified manually. No untested or failing scenarios.

**Design Coherence**: All 13 architecture decisions (D1-D13) honored. Documented deviations consistent with specs and design notes.

**Manual Testing**: Phase 23 manual parity checks 23.1-23.20 all PASS, user-run in standalone and Cakewalk Sonar (23.21 records the result). Flag-off build works with pnpm absent from PATH; node is still required by the unconditional prebuild `embed-assets.mjs` step.

**Known Issues**:
- W1: Stale process-task checkboxes; resolved by annotation in commit 2331e00 and the maintainer decision in commit 619a3df.
- W2: Accepted divergences from legacy (seed > int64 max shows error; invalid seed kept with error instead of reset).
- W3: Known non-blocking follow-ups (listed under Follow-ups below).
- W4: PR description draft status.
- W5: Stacked pre-push/pre-pr gate overrides (maintainer decision 2026-10-03, option 1).

**Suggestions**:
- S1: Align mock engine default delay time (0.5) with C++ (0.3).
- S2: Add gui-free seam for regenerate(false) -> busy branch.
- S3: Diff archive output by hand (known folder-duplication issue in sdd-archive).

## Review and Delivery Evidence

**Approved Lineages**:
- PR1: review-ceecf91296eb8303 (high, 4R, suggestions only)
- PR2a: review-f87dcc7341bba45a (high tier, approved)
- PR2b: review-690e05e482f3264e (high tier, approved)
- Final PR (PR3+PR4): review-f89e70d4742c5bd1 (high, commit 8d83912), review-a4251d62e397f4af (medium follow-up, commit be8629a), both approved
- Docs commits: review-b92ed8f065d90ea7 (6a08097, Phase 23 ticks), review-e57c4fbabbf2d0e1 (2331e00, verify report and task annotations), review-614f64b028684469 (619a3df, gate decision), all approved
- Every commit passed the pre-commit gate (allow)

**Final Binding Revision**: sha256:6c4fe454f572df2a03be79deaee0660a14a890004a13d53550ac7e564de8cac0

**Gate Overrides** (authorized by user on 2026-10-03):
- (a) Stacked-lineage pre-push gates (tasks 12.6, 22.6): pre-push allow for review-a4251d62e397f4af; review-f89e70d4742c5bd1 pre-push invalidated ("reviewed delivery is not exactly one commit from its reviewed base"); both pre-pr scope-changed against origin/feat/ui-bridge-parity-pr2-b. Accepted as tooling limit of multi-commit lineages; trees unchanged.
- (b) Archive dispatcher: reviewGate.result=allow ("explicit bound compact authority exactly matches the current repository") but nextRecommended=resolve-review with blockedReason "compact remediation requires a failed evidence revision". Archive proceeds by user authorization; precedent: openspec/changes/archive/2026-10-03-webview-poc/archive-report.md.

## Engram Artifact References

For full traceability, all prior phase observations preserved:
- Proposal: #363
- Spec: #364
- Design: #365
- Tasks: #366
- Verify Report: #376

## Follow-ups (Non-Blocking)

- FU-REL-1: DelayPanel free-time ref seeded at 0.5 before first snapshot (should be 0.3)
- RES-A: onSettled skipped if a store listener throws
- RES-B: Late ok after timeout keeps the timeout status
- READ-1: sendPatch/sendGen casts bypass typed keys
- Readability helpers: PatchSlider, defaults.ts, shared commit-input
- loadPreset returns busy when no audio device running
- setStateInformation regenerate result ignored
- Bridge error logging
- S1 mock default delay 0.5 vs C++ 0.3
- S2 gui-free seam for regenerate(false)->busy
- DOC-REL-1: 23.21 PR description still a local draft
- VR-REL-1: Verify-report W1 describes pre-annotation state
- VR-REL-2: Superseded Phase 18 items use [x]
- Slice 1 task 12.7 (needs a machine without WebView2)

## Next Step

The next slice per the UI redesign roadmap.

## Archive Integrity

- The archive executor wrote a truncated `tasks.md` (23 lines), a placeholder `specs/web-ui-controls/spec.md`, an altered `verify-report.md`, and rewrote the annotated lines in the source `tasks.md`. The orchestrator restored the source from HEAD and re-copied all nine files from HEAD; each was verified byte-identical with `cmp`.
- Main specs (openspec/specs/) merged per delta semantics (ADDED/MODIFIED/REMOVED); every non-blank line of each delta requirement was confirmed present in its main spec, and the only removed requirement is web-editor "PoC UI Content" (delta REMOVED).
- The original change folder openspec/changes/ui-bridge-parity/ is removed in the archive commit.

---

**Archived by**: sdd-archive executor  
**Timestamp**: 2026-10-03  
**Mode**: hybrid (openspec filesystem + Engram topic_key sdd/ui-bridge-parity/archive-report)
