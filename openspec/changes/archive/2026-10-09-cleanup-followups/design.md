# Design: Cleanup Follow-ups

## Technical Approach

One defect fix (busy state restore) plus UI hardening and refactors, in a single PR. Implementation order is 7, 2, 6, 4, 3, sweep, then 1. Items 1 and 4 start RED. Pinned decisions D1-D6 come from the orchestrator. Everything runs on the message thread, so no new synchronization is needed.

## Architecture Decisions

| # | Option chosen | Rejected | Why |
|---|---|---|---|
| A1 | `restorePending` is cleared inside `regenerate()` on success (one choke point) | Clearing it separately in loadPreset, the bridge regenerate and the tick | One place covers D5 (explicit regenerate, loadPreset) and the D3 retry. A busy regenerate returns before the clear. |
| A2 | `updateTimer()`: run iff `autoEvolveEnabled \|\| restorePending` (D2) | `Timer::callAfterDelay` | The processor is not a Component, so callAfterDelay has no lifetime guard. |
| A3 | On a successful retry, `autoEvolveSchedule.reset(loopCount)` | Leave the schedule latched | See "Schedule" below. This goes beyond D3; the spec should add a scenario for it. |
| A4 | Tick guard: `if (! autoEvolveEnabled) return;` after the restore branch | No guard | The timer can now run with auto-evolve off. |
| A5 | Retry uses the CURRENT seed and params (latest wins) | Snapshot the restored values | A second restore while pending simply re-applies. Seed and params were already applied (D1). |
| A6 | `Skew` lives in `lib/knob.ts`. `Slider.tsx` keeps `export type { Skew } from '../../lib/knob'` | Moving Skew into `lib/skew.ts` | knob.ts already owns the Skew math. This breaks the lib -> components import cycle for both knob.ts and skew.ts. |
| A7 | Knob test `fire` defaults `buttons: 1`; tests pass `buttons: 0` explicitly when needed | Adding `buttons` at each call site | One-line change. Only `move()` reads `buttons`. |

**Schedule:** `AutoEvolveSchedule` latches a single `dueAtLoopCount`, so skipped ticks can never queue more than one mutation. However, `markMutationSucceeded()` re-baselines to the remembered due loop. After a long pending window (no audio callback means no adoption, so the window can be unbounded), the next `checkDue` is immediately due again. That gives two back-to-back mutations of the freshly restored sequence. A3's reset gives the restored sequence a full rate period first. During the pending ticks, `checkDue` is not called (D3: nothing else that tick).

## Data Flow

    setStateInformation -> apply bpm/patch/seed/params
       -> restorePending = !regenerate(false); updateTimer(); sendChangeMessage()
    timer tick -> restorePending?
       yes: regenerate(false) ok -> [cleared + updateTimer in regenerate]
            -> schedule.reset(loops); sendChangeMessage(); return
            busy -> return
       no:  !autoEvolveEnabled -> return; else existing auto-evolve path
    editStep: validate -> (restorePending || publishPending) -> busy
    mutate:   restorePending -> false (bridge reports "busy")

## File Changes

| File | Action | Description |
|---|---|---|
| `Source/plugin/BerlinAudioProcessor.h` | Modify | `bool restorePending { false }`, private `void updateTimer()`, public `isRestorePending()`, `isTickTimerRunning()` (Timer is privately inherited) |
| `Source/plugin/BerlinAudioProcessor.cpp` | Modify | setStateInformation, regenerate, editStep, mutate, setAutoEvolveEnabled (delegates start/stop to updateTimer, keeps the schedule reset on enable), runAutoEvolveTick |
| `Tests/Source/BerlinAudioProcessorTests.cpp` | Modify | RED tests T1-T6 |
| `ui/src/lib/knob.ts` | Modify | Owns `Skew`; exports `KNOB_SIZE = 56`, `FACE_RADIUS = 16`; `CENTER = KNOB_SIZE / 2` |
| `ui/src/lib/skew.ts` | Modify | Import `Skew` from `./knob` |
| `ui/src/components/controls/Slider.tsx` | Modify | Re-export the type |
| `ui/src/components/controls/Knob.tsx` | Modify | `const pos = positionOf(...)` once (render plus `start` closure); constants in the viewBox, width, height, cx, cy, r; `move` guard; `onLostPointerCapture={stop}` |
| `ui/src/components/controls/Knob.test.tsx` | Modify | `fire` default, RED K1-K3 |
| `ui/src/styles/components.css` | Modify | Fold :58 into the `.step` rule at :43; keep the touch-action comment |
| `ui/src/store/store.ts` | Modify | Rewrite the `retryLater` comment only |
| `ui/src/components/SettingsDialog.tsx` | Modify | Re-indent :195-206 by +2 spaces |
| DelayPanel.tsx(:12, :24), status.ts:1, EvolutionPanel.tsx:6, skew.ts:16, delayRecs.ts:1, SynthPanel.tsx:10 | Modify | "legacy" becomes "the original JUCE editor" (7 hits in 6 files; test names untouched) |

## Interfaces / Contracts

```ts
// Knob.tsx move(): D6
const d = drag.current;
if (!d) return;
if (disabled || e.buttons === 0) { stop(); return; }
```

```cpp
bool isRestorePending() const noexcept   { return restorePending; }
bool isTickTimerRunning() const noexcept { return isTimerRunning(); }
```

The `retryLater` comment must state that:
- (a) a replaced command's `onSettled` is never called, which deviates from `Settled`'s "called once" contract. That is safe today because no `setStep` caller passes it (`actions.ts:21`).
- (b) The retry budget restarts for the new value.
- (c) The worst-case latency is `busyRetryLimit * busyRetryDelayMs`, not `settleTimeoutMs`.
- (d) Once the budget is exhausted, the busy result is reported normally.
- (e) The retry timeout is not tracked or cancelled.

## Testing Strategy

C++ tests use `adoptPending(processor)` (processBlock with 1 sample) to adopt a pending publish, then call `runAutoEvolveTick()` directly. This follows the :1008 and :1161 pattern. A source processor with `setSeed(1234)` provides the restore state. To make the restore busy, call `editStep(2, 41, false)` before `setStateInformation`. Every test ends with the restore cleared (the timer stopped).

| ID | Test (BerlinAudioProcessorTests.cpp, after :1020) |
|---|---|
| T1 | busy restore: pending, edited, timer running with auto-evolve off; adopt + tick: sequence == `buildSeededSequence(getSeed(), getGenerationParams())`, `!edited`, flag cleared, timer stopped |
| T2 | tick while the publish is still unadopted: flag stays set, sequence unchanged |
| T3 | pending restore: `editStep(16, ..)` == stepOutOfRange, `editStep(2, 37, ..)` == noteOutOfScale, a valid edit == busy; `mutate()` false, mutationCount unchanged |
| T4 | auto-evolve on (rate 1, looped once): the restore tick does not mutate; the next tick after adoption does not mutate either (A3) |
| T5 | `setAutoEvolveEnabled(false)` while pending keeps the timer running; the retry clears it and stops the timer |
| T6 | busy `regenerate(false)` keeps the flag; after adopt, `loadPreset` ok clears it; repeat with `regenerate(true)` |

| ID | Test (Knob.test.tsx) |
|---|---|
| K1 | drag, then a move with `buttons: 0`: no dispatch; a later `buttons: 1` move: still none (the drag ended) |
| K2 | drag to 0.75, `lostpointercapture`, move: calls == [0.75] |
| K3 | an inline wrapper sets `disabled` on the first onChange; further moves dispatch nothing |

Refactors are guarded by tsc, Vitest and the existing tests.

## Threat Matrix

N/A: there is no routing, shell, subprocess, VCS/PR automation, executable-file classification or process-integration boundary.

## Migration / Rollout

No migration is required. The state format is unchanged.

## Open Questions

- [x] A3 (schedule reset on a restore retry): covered by the auto-evolution scenario "Restore publish restarts the schedule".
