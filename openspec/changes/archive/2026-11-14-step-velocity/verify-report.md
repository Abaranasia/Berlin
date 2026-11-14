# Verify Report: step-velocity

Mode: Strict TDD, hybrid. Verdict: FAIL (1 CRITICAL needing a spec decision, not a code bug), 3 WARNING, 3 SUGGESTION.

## Runtime evidence (re-run by verify)
| Command | Exit | Result |
|---|---|---|
| MSBuild Tests/Builds/VisualStudio2026/BerlinTests.sln Debug x64 | 0 | builds |
| BerlinTests.exe --category=Berlin | 0 | 522 test cases, "All tests completed successfully" (baseline 495) |
| MSBuild Builds/VisualStudio2026/Berlin.sln Debug x64 | 0 | builds |
| pnpm tsc (ui) | 0 | clean |
| pnpm test (ui) | 0 | 40 files, 567 passed (baseline 554) |
Goldens / ReproducibilityTests: no diff in git (not in the changed-file list), suites pass unchanged.

## Tasks
36/36 tasks in phases 0-10 are [x]. Phase 11 (manual, user) and 12 (archive) are open by design.

## Strict TDD compliance
Apply-progress has a per-phase RED/GREEN table (not the full 6-column format; no Triangulate/Safety Net columns) -> WARNING. RED files exist and all pass now. Test layers: C++ unit/integration (processor, bridge), Vitest unit + component integration. Assertion audit: no tautologies. Note the effects-on no-clip test is relative (accent <= vel100 peak x ceiling), not absolute.

## Spec compliance of flagged items
1. No-clip with effects ON: CRITICAL. Scenario "Accent at the default level does not clip" (internal-synth-output) requires |sample| <= 1.0 with effects enabled. Not met: effects-on peak is ~1.99 at velocity 100 (pre-existing gain staging). No covering test asserts it; the only absolute test is effects-bypassed (<= 1.0, passes, peak 0.967). Root cause is not velocity and not fixable by the ceiling, so this needs a user decision: amend the scenario to effects-bypassed, or fix effects gain staging in the "standalone clipping" follow-up. Velocity 100 stays exactly 1.0 (velocityToGain(100)=100/100.0f; SmoothedValue returns target exactly), and a voice fingerprint (0x8dd3ab4822fcb12c, captured pre-change) pins bit identity; all pass.
2. Ceiling 1.27 -> 1.12: WARNING. Allowed by the ceiling clause, but scenario "Idle voice ... exact 1.27 ratio" now reads velocityToGain(127)/velocityToGain(100) = 1.12. Amend the spec text at archive.
3. No MidiEventTranslator / MidiFileWriter test files: WARNING. Behavior is covered at runtime (processor processBlock note-on velocity, clampNoteOnVelocity static_asserts, MidiExportTimeline round-trip through MidiFileWriter(1) incl. clamp 0->1, 200->127). Acceptable; tasks 3.2/5.2 are annotated as deviations.
4. Generated web assets not regenerated: SUGGESTION. Must run the UI build step before manual phase 11.

## Other checks
- keepPosition velocity-only edit sends no note-off: PASS (SequencePlayerHandoffTests line ~502 expects buffer.size()==0; adopt predicate unchanged; velocity+note change still emits note-off).
- Out-of-range velocity -> "velocity-out-of-range": PASS (UiBridgeTests 927-940, incl. validation precedes busy; processor and StepEdit tests green).
- Default 100 everywhere: PASS (Step, StepEvent, MidiExportEvent, noteOn default arg, encodeSteps).
- Design coherence: D1-D11 match code (translator clamp, FileWriter(int), field-by-field editStep, readOptionalInt). Deviation: ceiling value only.
- Diff size ~1235 lines vs 800 budget: WARNING, size:exception pre-accepted.

## Issues
CRITICAL: effects-on accent no-clip scenario unmet (item 1).
WARNING: ceiling 1.12 vs spec "exact 1.27"; missing translator/writer test files; TDD evidence table lacks Triangulate/Safety Net columns (and diff over budget, pre-accepted).
SUGGESTION: regenerate web assets; follow-up for effects gain staging; accent peak test could also assert a fixed absolute bound for the effects-on case once staging is fixed.
