# Explore: cleanup-followups

Explored at HEAD cbd21a3, branch chore/cleanup-followups. The Engram mirror is `sdd/cleanup-followups/explore` (id 432).

## 1. Busy setStateInformation restore [KEEP, T]

**Where:** `Source/plugin/BerlinAudioProcessor.cpp:129-177`. `regenerate(false)` (:173) returns false when `player.isPublishPending()` (:189-190), and the result is ignored.

**Problem:** seed and generation params are already updated, but `currentSequence`, `sequenceSeed` and `edited` stay stale, so the state is only half-restored. `loadPreset` is not affected because it checks busy first.

**Threading:** `setStateInformation` asserts the message thread, debug builds only (:139). Everything `regenerate` touches is message-thread-only, and `player.publishSequence` has a single producer. The juce::Timer runs only while auto-evolve is on (`setAutoEvolveEnabled` :405-418).

**Fix:**
- Add `bool restorePending`, set to `! regenerate (false)`.
- Add `updateTimer()`, which runs the timer iff `autoEvolveEnabled || restorePending`. `setAutoEvolveEnabled(false)` must not stop the retry.
- `runAutoEvolveTick` retries the regenerate first. On success it clears the flag and sends a change message.
- A successful explicit regenerate or loadPreset clears the flag.
- `editStep` and `mutate` return busy or skip while the flag is set.
- `Timer::callAfterDelay` was rejected: the processor is not a Component, so there is no lifetime guard.

**Tests [T]:** drive `runAutoEvolveTick` directly (as in the test at `Tests/Source/BerlinAudioProcessorTests.cpp:1161`), then extend the busy-restore test (:1008) to:
- adopt the pending publish, then tick, and assert the sequence equals the seeded sequence, `edited` is false and the flag is cleared;
- assert `loadPreset` clears the flag;
- assert that with auto-evolve off the restore still retries.

**Size:** about 35 source lines and 60 test lines.

## 2. Duplicated `.step` CSS rule [KEEP, V/M]

**Where:** `ui/src/styles/components.css:43-52` and :58.

**Fix:** merge the second rule's 4 declarations into the first. About 5 lines, no risk.

## 3. Store retry lifecycle comment [KEEP, V]

**Where:** `ui/src/store/store.ts:145-156` (`retryLater`), plus :69-70 and :14.

**What the comment gets wrong or leaves out:**
- **(a)** When a newer pending value replaces the retried command, the replaced command's `onSettled` is never called. That contradicts the "called once" contract at :45-48.
- **(b)** The retry budget resets for the new value.
- **(c)** The latency bound is `limit * delay`, not `settleTimeoutMs`.
- **(d)** When the limit is exhausted, the busy result is reported.
- **(e)** The retry setTimeout is untracked.

**Fix:** rewrite the comment. Calling `onSettled?.(null)` for the replaced command (with a Vitest) is optional; check the setStep callers first.

## 4. RES-002 Knob move() guard [KEEP, V]

**Where:** `ui/src/components/controls/Knob.tsx:24-31` has no `disabled` or `e.buttons` guard, and there is no `onLostPointerCapture`.

**Fix:** add the guard and `onLostPointerCapture={stop}`.

**Tests:** the `fire` helper in `Knob.test.tsx` needs `buttons: 1` on moves.

## 5. RES-001 frame not re-applied on DPI change [DROP]

**Where:** `Source/Main.cpp` `MainWindow::applyEditorLimits` (:110-125).

**Why drop:** the right JUCE hook is uncertain, it can only be checked by building plus a manual test with two monitors at different scaling, and the payoff is low. It stays an open follow-up.

## 6. READ-001/002/003 Knob readability [KEEP, V]

These are behavior-preserving, with tsc and the existing tests as guards.
- **READ-001:** move `Skew` into `lib/knob.ts` and re-export it from `Slider.tsx`. This breaks a type-level import cycle.
- **READ-002:** compute `positionOf` once in `Knob.tsx` (it is computed at both :16 and :20).
- **READ-003:** export `KNOB_SIZE` (56) and `FACE_RADIUS` (16) from `knob.ts` and use them in `Knob.tsx` instead of hardcoded literals.

## 7. SettingsDialog indentation [KEEP, V]

**Where:** `ui/src/components/SettingsDialog.tsx:195-206`. The "Output level" section is indented 2 spaces less than its siblings.

**Fix:** whitespace only. The indentation is all spaces, not mixed tabs.

## 8. Other archived follow-ups

- **ui-settings:**
  - The missing headless tests ("Registered only with a host", "Panic uses current channel") need a StandaloneHost seam. DEFER.
  - The ASIO observation is manual only. DROP.
  - Standalone clipping becomes a separate change.
- **ui-legacy-removal:**
  - Stray "legacy" wording in `ui/src` comments: OPTIONAL comment-only sweep, only if there are 10 or fewer hits.
  - The fallback seam and the display-scaling item: DROP.
- **step-editing:** only item 1. The ui-bridge "16 commands" fix is already done.

## Recommendation

- **Scope:** keep 1, 2, 3, 4, 6 and 7, plus the optional legacy-wording sweep. Drop 5 and the ui-settings test gaps.
- **Delivery:** about 200-260 changed lines in a single PR.
- **Order:** 7, 2, 6, 4, 3 (UI), then 1 (C++).
- **Strict TDD:** items 1, 4 and any behavior change in 3 go RED first.
