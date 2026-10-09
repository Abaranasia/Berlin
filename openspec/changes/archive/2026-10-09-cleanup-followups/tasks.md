# Tasks: Cleanup Follow-ups

## Review Workload Forecast

| Field | Value |
|-------|-------|
| Estimated changed lines | ~280 (range 250-350) |
| 400-line budget risk | Low (800-line review budget also Low) |
| Chained PRs recommended | No |
| Suggested split | Single PR, 3 commits |
| Delivery strategy | single-pr |
| Chain strategy | size-exception not needed |

Decision needed before apply: No
Chained PRs recommended: No
Chain strategy: pending
400-line budget risk: Low

### Suggested Work Units (commits in one PR)

| Unit | Goal | Likely PR | Focused test command | Runtime harness | Rollback boundary |
|------|------|-----------|----------------------|-----------------|-------------------|
| 1 | UI refactors, comment sweep (Phases 1,2,4,5 + sweep) | PR 1 | `pnpm test` in ui/ | N/A: no behavior change | ui/src refactor files only |
| 2 | Knob guard + lostpointercapture (Phase 3) | PR 1 | `pnpm vitest run src/components/controls/Knob.test.tsx` | Manual drag (Phase 9) | Knob.tsx, Knob.test.tsx |
| 3 | Busy restore retry (Phase 7) | PR 1 | `BerlinTests.exe --category=Berlin` | Sonar restore while busy (Phase 9) | BerlinAudioProcessor.{h,cpp} + tests |

## Phase 0: Baseline

- [x] 0.1 Run `Tests/Builds/VisualStudio2026/x64/Debug/ConsoleApp/BerlinTests.exe --category=Berlin`; record 442 passing.
- [x] 0.2 Run `pnpm test` in `ui/`; record 538 passing.

## Phase 1: Item 7 - SettingsDialog indent

- [x] 1.1 `ui/src/components/SettingsDialog.tsx`: re-indent "Output level" block (~:195-206) by +2. Existing SettingsDialog tests stay green.

## Phase 2: Item 2 - CSS merge

- [x] 2.1 `ui/src/styles/components.css`: fold the duplicate `.step` rule (:58) into the rule at :43; no visual change.

## Phase 3: Item 6 - READ-001/002/003 (refactor, existing tests are the guard)

- [x] 3.1 `ui/src/lib/knob.ts`: move `Skew` here; export `KNOB_SIZE = 56`, `FACE_RADIUS = 16`.
- [x] 3.2 `Slider.tsx`: `export type { Skew } from` knob; `lib/skew.ts` imports from `./knob` (removes lib->components cycle).
- [x] 3.3 `Knob.tsx`: `CENTER = KNOB_SIZE / 2`, compute `positionOf` once. Run tsc + Vitest.

## Phase 4: Item 4 - Knob guard

- [x] 4.1 RED: `Knob.test.tsx` `fire` helper defaults `buttons: 1`; add K1 "button-less move ends the drag and dispatches nothing" (buttons:0), K2 "lostpointercapture ends the drag", K3 "disabled flipped mid-drag dispatches nothing" (inline stateful wrapper). Confirm K1-K3 fail.
- [x] 4.2 GREEN: `Knob.tsx` `move`: `if (disabled || e.buttons === 0) { stop(); return; }`; add `onLostPointerCapture={stop}`. K1-K3 + existing drag tests pass.

## Phase 5: Item 3 + legacy sweep (comments only)

- [x] 5.1 `ui/src/store/store.ts`: rewrite `retryLater` lifecycle comment, points a-e only.
- [x] 5.2 Reword 7 "legacy" comments to "the original JUCE editor" (DelayPanel x2, status, EvolutionPanel, skew, delayRecs, SynthPanel). Test names untouched; verify with rg that no "legacy" hits remain in `ui/src` comments.
- [x] 5.3 Run `pnpm test` + tsc; expect 538 + 3 (K1-K3).

## Phase 6: UI commit checkpoint

- [x] 6.1 Commits: `refactor: tidy knob helpers, settings indent and step css`; `fix: ignore button-less knob moves and end drag on lost capture`.

## Phase 7: Item 1 - Busy restore retry (C++)

All RED tests in `Tests/Source/BerlinAudioProcessorTests.cpp` after ~:1020; use `adoptPending()` and direct `runAutoEvolveTick()`. Busy restore: `editStep` before `setStateInformation`. Each test ends with restore cleared.

- [x] 7.1 RED: add getters usage `isRestorePending()` / `isTickTimerRunning()` (compile-fail counts as RED), then tests:
  - T1 "Busy restore publishes on a later retry" (sequence == seeded, `edited` false, flag cleared; also "retry still busy keeps pending" and "restore without pending publish unchanged").
  - T2 "Restore retry survives auto-evolve off" and "Timer stops when neither condition holds" (timer assertions).
  - T3 "Pending restore blocks a due mutation" (tick retries first, no mutation) and "Restore publish restarts the schedule" (A3: no back-to-back mutation; sends change message).
  - T4 "Mutate returns false while restore pending" (counter/sequence unchanged) and "Mutate works after restore publishes".
  - T5 "editStep busy while restore pending" and "Validation precedes busy".
  - T6 "Successful regenerate and loadPreset clear pending; busy regenerate leaves it set"; "Turning auto-evolve off keeps pending restore alive".
- [x] 7.2 GREEN `BerlinAudioProcessor.h`: add `restorePending`, public `isRestorePending()`, `isTickTimerRunning()`.
- [x] 7.3 GREEN `.cpp`: `regenerate()` clears flag on success then `updateTimer()` (A1); `setStateInformation` sets `restorePending = !regenerate(false)`, `updateTimer()`, `sendChangeMessage()`.
- [x] 7.4 GREEN: `updateTimer()` runs timer iff `autoEvolveEnabled || restorePending`; `setAutoEvolveEnabled` delegates, keeps schedule reset on enable (A2).
- [x] 7.5 GREEN: tick retries `regenerate(false)` while pending; on success `autoEvolveSchedule.reset(loopCount)` + `sendChangeMessage`; return either way; then `if (!autoEvolveEnabled) return;` (A3/A4).
- [x] 7.6 GREEN: `editStep` returns `busy` on `(restorePending || publishPending)` after validation; `mutate()` returns `false` while `restorePending`.
- [x] 7.7 REFACTOR: update header comments (lines ~29, ~124, cpp ~171). Commit left to the orchestrator.

## Phase 8: Build and test

- [x] 8.1 Build BerlinTests (Debug); run `--category=Berlin`; expect 442 + new T-tests, 0 failures.
- [x] 8.2 `pnpm tsc`/`pnpm test` in `ui/`; expect 541 passing.
- [x] 8.3 Check `git diff --stat` stays under 800 lines.

## Phase 9: Manual [M]

- [x] 9.1 Knob drag feel; release outside window; disabled Knob inert. (user 2026-10-09)
- [x] 9.2 Restore plugin state in Sonar while a publish is pending, if reproducible; verify sequence/edited settle. (user 2026-10-09)
- [x] 9.3 Visual check of the step grid after the `.step` merge. (user 2026-10-09)

## Phase 10: Archive (separate)

- [x] 10.1 `sdd-verify`, then `sdd-archive`; merge deltas into main specs and diff the archive output (folder duplication risk).
