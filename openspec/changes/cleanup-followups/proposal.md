# Proposal: Cleanup Follow-ups

## Intent

Close the follow-ups left by step-editing, ui-redesign and ui-legacy-removal. The one real defect: a host state restore that hits a pending publish is only half applied (seed and patch restored, sequence and `edited` stale). The rest is UI hardening and readability debt.

## Scope

### In Scope
1. Busy restore retry in `BerlinAudioProcessor` (`restorePending` flag, timer retry).
2. Merge the duplicated `.step` CSS rule.
3. Rewrite the `retryLater` lifecycle comment in `store.ts` (comment only).
4. Knob `move()` guard (`disabled`, `e.buttons`) plus `onLostPointerCapture`.
6. READ-001/002/003: `Skew` into `lib/knob.ts`, single `positionOf`, exported `KNOB_SIZE`/`FACE_RADIUS`.
7. SettingsDialog "Output level" indentation.
- "legacy" comment sweep: 7 comment hits in `ui/src` (at most 10), comments only; test names unchanged.

### Out of Scope
- Item 5: DPI frame re-apply (RES-001), open follow-up.
- ui-settings headless test gaps (needs a StandaloneHost seam).
- Standalone clipping, a separate change.
- Calling `onSettled(null)` for commands replaced during a busy retry: no `setStep` caller passes `onSettled` (`actions.ts:21`), so we only document the gap.

## Capabilities

### New Capabilities
None

### Modified Capabilities
- `plugin-state-recall`: a busy restore stays pending and is retried until it publishes. `edited` stays true until then. A successful explicit regenerate or preset load clears the pending restore.
- `auto-evolution`: the timer runs iff auto-evolve is on or a restore is pending. A tick retries the restore before any due mutation.
- `sequence-mutation`: `mutate()` returns busy while a restore is pending, and the auto-evolve trigger skips.
- `step-editing`: `editStep` returns `busy` while a restore is pending (validation still comes first).
- `web-ui-layout`: Knob Control. A move with no pressed button, or on a disabled Knob, dispatches nothing. Losing pointer capture ends the drag.

`web-ui-controls` has no delta (item 3 is comment-only). Items 2, 6, 7 and the sweep are pure refactors.

## Approach

- Follow the exploration: `updateTimer()` decides whether the timer runs. `runAutoEvolveTick` retries `regenerate(false)` first and sends a change message on success. We rejected `Timer::callAfterDelay` because it has no lifetime guard.
- Order: 7, 2, 6, 4, 3, then 1. Strict TDD: items 1 and 4 start from a failing test.

## Affected Areas

| Area | Impact |
|------|--------|
| `Source/plugin/BerlinAudioProcessor.{h,cpp}` | Modified |
| `Tests/Source/BerlinAudioProcessorTests.cpp` | Modified |
| `ui/src/components/controls/Knob.tsx`, `Knob.test.tsx`, `Slider.tsx`, `ui/src/lib/knob.ts` | Modified |
| `ui/src/store/store.ts`, `ui/src/styles/components.css`, `ui/src/components/SettingsDialog.tsx` | Modified |
| 7 `ui/src` files with "legacy" comments | Comments only |

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| Timer lifecycle regression (auto-evolve off stops the retry) | Med | `[T]` test with auto-evolve off |
| A restore retry racing a mutation or edit | Low | busy gate on `editStep`/`mutate`, all on the message thread |
| The Knob guard breaks drag tests | Low | add `buttons: 1` to the `fire` helper |

## Rollback Plan

This is a single PR, so revert the merge commit. Item 1 is self-contained in the processor and its tests.

## Dependencies

None

## Success Criteria

- [ ] A busy restore ends with the sequence equal to the seeded one, `edited` false and the flag cleared, even with auto-evolve off.
- [ ] `loadPreset` and a successful regenerate clear the pending flag.
- [ ] The Knob ignores button-less moves, and lost capture ends the drag.
- [ ] tsc, Vitest and the JUCE unit tests pass, and the diff stays within 800 lines.

## Proposal question round

Pace is automatic, so no questions were asked. Assumptions for the user to review:
- While a restore is pending, the UI shows "Busy, try again" for a manual mutate or edit.
- Losing the `onSettled` callback during a busy retry is documented, not fixed.
- The legacy sweep rewords only comments.
