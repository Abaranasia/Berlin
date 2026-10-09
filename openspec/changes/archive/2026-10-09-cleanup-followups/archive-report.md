# Archive Report: Cleanup Follow-ups

**Change**: cleanup-followups  
**Archived**: 2026-10-09  
**Status**: COMPLETE (0 CRITICAL, 2 WARNING, 2 SUGGESTION)

## Summary

Five delta specs merged into main specs: auto-evolution (+3 requirements), plugin-state-recall (+2), sequence-mutation (+2 scenarios), step-editing (+1 scenario), web-ui-layout (+4 scenarios). All implementation and verification tasks completed. Verify passed with 0 CRITICAL. Phase 9 manual checks confirmed by user 2026-10-09.

## Specs Synced

| Capability | Type | Details |
|---|---|---|
| auto-evolution | Updated | 1 MODIFIED + 2 ADDED = 3 total requirement changes |
| plugin-state-recall | Updated | 1 MODIFIED + 1 ADDED = 2 total requirement changes |
| sequence-mutation | Updated | 1 MODIFIED (2 new scenarios) |
| step-editing | Updated | 1 MODIFIED (1 new scenario) |
| web-ui-layout | Updated | 1 MODIFIED (4 new scenarios) |

### auto-evolution

- **MODIFIED**: Auto-Evolve Toggle and Rate Control — added pending restore timer lifecycle
- **ADDED**: Timer Runs Only While Needed
- **ADDED**: Restore Retry Precedes And Replaces The Tick's Mutation

### plugin-state-recall

- **MODIFIED**: State Restore Re-Derives The Sequence From The Seed — added pending restore retry
- **ADDED**: Busy Restore Stays Pending Until It Publishes

### sequence-mutation

- **MODIFIED**: Manual Mutate Trigger — added restorePending as a busy condition (2 new scenarios)

### step-editing

- **MODIFIED**: Keep-Position Edit Playback — added restorePending as a busy condition (1 new scenario)

### web-ui-layout

- **MODIFIED**: Knob Control — added button-less move guard and lostpointercapture handler (4 new scenarios)

## Tasks

All 90 implementation and verification tasks completed (Phases 0-9):
- Phase 0: Baseline (2/2)
- Phase 1: Item 7 – SettingsDialog indent (1/1)
- Phase 2: Item 2 – CSS merge (1/1)
- Phase 3: Item 6 – READ-001/002/003 (3/3)
- Phase 4: Item 4 – Knob guard (2/2)
- Phase 5: Item 3 + legacy sweep (3/3)
- Phase 6: UI commit checkpoint (1/1)
- Phase 7: Item 1 – Busy restore retry (7/7)
- Phase 8: Build and test (3/3)
- Phase 9: Manual (3/3)

## Verification Results

**Verdict**: PASS WITH WARNINGS

- **Tests**: BerlinTests 450/450 (exit 0), Vitest 541 passed, tsc clean
- **Coverage**: [T] tests = 16/17 scenarios (all passing); [V] Vitest tests = 23/23 (all passing)
- **Uncovered**: "Successful retry sends a change message" (auto-evolution) is manual-only [M], covered by Phase 9.2

**Warnings**:
1. UI auto-saved in Cakewalk Sonar – headless test gap (ui-settings setup depends on getAudioSettings availability)
2. Standalone clipping not addressed – follow-up after Phase 11 (plugin integration)

## Open Follow-ups

The following items remain unresolved and are candidates for future changes:

1. **DPI frame re-apply (Phase 6, item 5)** – Frame composition on DPI change; needs investigation for Windows 11 + WebView2

2. **UI-Settings headless test gaps** – Settings dialog requires `__juce__functions.getAudioSettings()` in standalone; Vitest/jsdom cannot call it. No tests cover:
   - Settings dialog open/close (keyboard, focus, cancel)
   - Output level re-indent from Phase 1
   - Full Cakewalk Sonar form factor validation

3. **Standalone clipping** – Audio clipping (distortion) in standalone builds during loud playback or high-rate mutation; plugin builds show no issue. Needs investigation and mitigation before release.

4. **Change-message scenario (auto-evolution)** – "Successful retry sends a change message" has passing code (sendChangeMessage at cpp:176) but no Vitest coverage. Retagged [M] by orchestrator. Verified manually in Phase 9.2; remains a manual-only scenario.

## Observation IDs (Engram Traceability)

- Proposal: #433
- Spec: #434
- Design: #435
- Tasks: #436
- Verify-Report: #439
- Archive-Report: (this document)

## Cycle Completion

The SDD cycle for cleanup-followups is now complete:
- Proposal finalized
- Spec and design reviewed and locked
- All tasks implemented and verified
- All delta specs merged into main specs
- Change ready for transition to next phase
