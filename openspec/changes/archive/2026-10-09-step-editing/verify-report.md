# Verification Report: step-editing

**Change**: step-editing | **Mode**: hybrid, Strict TDD | **Branch/HEAD**: feat/step-editing @ 592b43d (clean) | **Date**: 2026-10-09
**Verdict**: PASS WITH WARNINGS (0 CRITICAL, 2 WARNING, 2 SUGGESTION)

## Completeness
Tasks: Phases 0-6 and 7.1-7.2 ticked; Phase 8 (8.1-8.10) ticked in this verify run with "(user 2026-10-09)" on the user's report "manual checks all pass" (standalone + VST3 in Sonar). 7.3 is the archive note for sdd-archive (intentionally open, not a core task).

## Execution evidence
| Command | Result |
|---|---|
| `BerlinTests.exe --category=Berlin` | exit 0, "All tests completed successfully", 442 "Completed tests" lines |
| `pnpm test` (ui) | exit 0, 39 files, 538 passed |
| `pnpm typecheck` (ui) | clean |
Build (MSBuild/vite) not re-run: the Tests exe already reflects HEAD (count 442 matches apply-progress); apply-progress records Debug x64 Rebuild OK for standalone, plugin and tests, and `pnpm build` OK without a mock chunk.

## Spec compliance matrix
Legend: T = BerlinTests, V = Vitest, M = manual (user 2026-10-09).

| Scenario | Evidence | Status |
|---|---|---|
| step-editing: Valid index edits the step [T] | StepEditTests "index limits"; BerlinAudioProcessorTests "editStep() ok ..." | COMPLIANT |
| Index beyond 16 or size [T] / Negative index [T] | StepEditTests "index limits"; processor "rejected edit (index, scale)" | COMPLIANT |
| Mute keeps the note [T] | StepEditTests "muted step's note stays valid"; processor "editStep() ok" | COMPLIANT |
| Out-of-scale note rejected [T] | StepEditTests "out-of-scale ... noteOutOfScale"; processor "rejected edit" | COMPLIANT |
| Mute allowed on out-of-scale note [T] | StepEditTests "CURRENT note is always accepted"; processor "mute is allowed on a note that became out of scale"; mock.test | COMPLIANT |
| Idempotent re-send [T] | processor "idempotent re-send" | COMPLIANT |
| Up/down within scale, snap by direction, ends clamped [V] | lib/stepEdit.test.ts (adjacent note, snap, ends, multi-step) | COMPLIANT |
| scaleNotes follows scale/root/range [T] | StepEditTests (C minor, major 48..60, D Dorian 40..64, normalization); UiBridgeTests "snapshot carries edited and scaleNotes" | COMPLIANT |
| Edited flag: sets/clears/mutate keeps/rejected [T] | processor "edited is cleared by regenerate, randomize, loadPreset and setStateInformation; kept by mutate", "editStep() ok", "rejected edit" | COMPLIANT |
| Export reflects edits [T] | processor "exportMidiTo() exports the edited sequence" | COMPLIANT |
| Regenerate discards edits [T] | processor "regenerate with the same seed restores the seeded original" | COMPLIANT |
| Playhead continues / Edit does not restart loop [T] | HandoffTests "keepPosition keeps playhead, step counter and loopCount"; M 8.1 | COMPLIANT |
| Sounding step edit sends note-off [T] | HandoffTests "editing the sounding step's note ...", "muting the sounding step"; M 8.2 | COMPLIANT |
| Other step edit sends no note-off [T] | HandoffTests "unchanged sounding step, another step edited, or nothing sounding" | COMPLIANT |
| Busy rejects edit / Validation precedes busy [T] | processor "while a publish is pending fails busy", "validation precedes busy" | COMPLIANT |
| Click toggles / Drag no toggle / Wheel / Keyboard / A11y+dimming / Edited indicator [V] | StepView.test.tsx (click, unmute, drag up/down, wheel with defaultPrevented, roving tabindex, Space/Arrows/End/Home, labels+aria-pressed, data-gate dimming, Edited badge) | COMPLIANT |
| WebView2 input [M] | Manual 8.1, 8.3-8.5 (user 2026-10-09) | COMPLIANT (manual) |
| ui-bridge: setStep valid / out-of-range / out-of-scale / busy / malformed [T] | UiBridgeTests lines 635, 654, 697 | COMPLIANT |
| ui-bridge: Snapshot fields, scaleNotes tracks params [T] | UiBridgeTests "snapshot carries edited and scaleNotes" | COMPLIANT |
| ui-bridge: Fixed error tokens | UiBridgeTests 654; protocol.test.ts; store.test "non-busy failure reverts" | COMPLIANT |
| ui-bridge: Dispatch surface is 16 commands [V]/[T] | protocol.test "16 bridge commands"; native.test "exactly the 16 commands" | COMPLIANT |
| ui-bridge: Dispatch is message-thread operation | Code review only (no runtime test) | COMPLIANT (review) |
| web-ui-controls: Fast wheel / Busy retry / Different steps / Non-busy failure [V] | store.test.ts 481, 490, 537, 544 (+507, 519, 558) | COMPLIANT |
| web-ui-controls: Typed protocol | protocol.test.ts (setStep args type-level, snapshot type) | COMPLIANT |
| web-ui-layout: Gate distinction / Playhead [V] | StepView.test "still highlights only the playhead step"; App.test | COMPLIANT |
| web-ui-layout: Muted note dimmed, Edited indicator follows snapshot [V] | StepView.test dimming and badge removed on regenerate | COMPLIANT |
| web-ui-layout: Focus ring on cells [M] | Manual 8.4 (user 2026-10-09); CSS global :focus-visible | COMPLIANT (manual) |
| generation-live-control: keep-position adoption / whole-sequence publish resets [T] | HandoffTests keepPosition and "restart mode still resets to step 0" | COMPLIANT |
| sequence-mutation: Mutation builds on edits [T]; Mutate restarts loop; Edit does not restart | processor "mutate() after an edit builds on the edited sequence"; HandoffTests restart regression | COMPLIANT |
| auto-evolution: Evolve mutates edited sequence; pending publish skips tick [T] | processor "auto-evolve: a pending edit publish makes the due tick skip mutate() ..." | COMPLIANT |
| auto-evolution: Edit while evolve on [M] | Manual 8.9 (user 2026-10-09) | COMPLIANT (manual) |

No UNTESTED or FAILING scenarios.

## Design coherence
D1-D8 followed. Deviations (all recorded in apply-progress, none break a spec): aria "off" vs design text "muted" (spec wins); `runAutoEvolveTick()` public test seam; Plugin jucer also updated; App.test assertion changed per spec "Muted note dimmed". Fix 592b43d: note-equal-to-current accepted (spec text updated). The fix agent's forced `edited=false` after busy restore was reverted by the orchestrator (test pins edited=true); consistent with the "edited" meaning.

## TDD compliance
RED/GREEN evidence is reported per phase in prose in apply-progress (#427) with counts, not in the formal "TDD Cycle Evidence" table. Test files exist and pass at execution; triangulation present (multiple cases per behavior). Assertion audit on sampled tests: no tautologies, ghost loops or smoke-only tests found.

## Issues
CRITICAL: none.

WARNING
1. apply-progress lacks the formal TDD Cycle Evidence table (evidence is narrative only). Non-blocking.
2. Pre-existing, out of scope: a busy `setStateInformation` restore does not replace the playing (edited) sequence; `edited` stays true. Recorded in bugfix #428 as a possible follow-up.

SUGGESTION
1. Task 7.3 for sdd-archive: update the ui-bridge main spec Purpose line "Exactly 15 commands" to 16 and diff the archive output (folder duplication, dropped files).
2. Delivery is a single PR with a size exception (~835+ lines plus fix); chained PR to main remains unmerged per project memory.

Bounded reviews: main review and review-b10810dc54bb49ae (fix) approved earlier; none started here.
