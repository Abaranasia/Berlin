# Auto-Evolution Specification

## Purpose

Unattended periodic mutation: while enabled, `MainComponent` calls the existing `mutate()` (from `sequence-mutation`) once every N completed loops, with no click. Reuses the existing transform library, publish path, and determinism/undo contract verbatim — this spec adds only the scheduling trigger, its UI, and its busy-collision contract. Explicitly excludes (deferred to a later phase) drift/arc-back-to-original behavior and separate rhythm/pitch/octave/density variation parameters — this spec covers only periodic single-random-transform auto-mutation via the existing `MutationEngine`.

## Requirements

### Requirement: Auto-Evolve Toggle and Rate Control

The system MUST provide a new full-width UI row below `generationButtonRow` containing an Auto-Evolve enable/disable toggle (off by default) and a rate selector (loops-per-mutation: 1/2/4/8/16, default 4). While the toggle is off, behavior MUST be byte-identical to today (no timer-driven mutation, no polling).

#### Scenario: Off by default

- GIVEN the application starts with no prior Auto-Evolve interaction
- WHEN playback runs for any number of loops
- THEN `mutate()` is never called automatically

#### Scenario: Enabling schedules future mutations only

- GIVEN Auto-Evolve is toggled on with rate N
- WHEN loops complete after the toggle was enabled
- THEN the first automatic mutation fires after N loop completions counted from enable time, not retroactively for loops already completed

### Requirement: Mutation Scheduled by Loop Completions

While Auto-Evolve is enabled, `MainComponent` (a `juce::Timer`) MUST poll `SequencePlayer::getLoopCount()` and, when it has advanced by the selected rate N since the last successfully-triggered mutation, mark that threshold crossing as due and call the existing `mutate()` unchanged as soon as the system is not busy (see the Busy-Collision requirement below for the busy case). No new publish path or duplicated mutation logic MUST be introduced.

#### Scenario: Automatic mutation fires once per N loops

- GIVEN Auto-Evolve enabled at rate N with no pending busy publish
- WHEN `getLoopCount()` advances by N since the last triggered mutation
- THEN `mutate()` is called exactly once for that threshold crossing

#### Scenario: Disabling stops further scheduling

- GIVEN Auto-Evolve is enabled and then toggled off before the next threshold is reached
- WHEN additional loops complete after the toggle-off
- THEN no further automatic `mutate()` calls occur, even though `getLoopCount()` keeps advancing

### Requirement: Poll Latency Bounded Relative to Step Duration

The timer poll rate MUST be fast enough that loop-boundary detection does not drift audibly: polling MUST detect a completed loop within one step duration of it occurring, for any configured tempo/step-resolution combination in this change (fixed 120 BPM, 16th-note steps per `playback-transport`). The concrete poll interval is a design-level decision bound by this contract, not a hardcoded requirement value.

#### Scenario: Detection latency stays within one step duration

- GIVEN a completed loop at time T and the current step duration D (samples-per-step converted to time)
- WHEN the next timer poll after T occurs
- THEN it occurs no later than T + D

### Requirement: Busy-Collision Check Precedes the Mutate Call

Before invoking `mutate()` for a due automatic mutation, the timer callback MUST check whether a publish is already pending/unadopted (via `SequencePlayer::isPublishPending()`) and MUST NOT call `mutate()` at all while one is pending — because `mutate()` unconditionally shows a "Busy, try again" status label on its own rejection path, and that label MUST NOT appear for an automatic attempt the user did not initiate. The due threshold MUST NOT be marked as consumed until `mutate()` is actually called and succeeds. The same due mutation MUST be re-checked and retried on the next timer tick until a tick finds no publish pending, landing delayed by ticks rather than being skipped for that rate window. (This is distinct from a manual click's busy path, which still calls `mutate()` directly and shows the label on rejection — see `sequence-mutation`'s modified "Manual Mutate Trigger".)

#### Scenario: Busy automatic attempt skips the call entirely and retries silently

- GIVEN Auto-Evolve's loop-count threshold has just been reached and a publish is already pending/unadopted
- WHEN the timer callback runs
- THEN it detects the pending publish via `isPublishPending()` and does NOT call `mutate()` this tick, so no status label is shown and the due threshold remains unconsumed
- AND on a later timer tick, once `isPublishPending()` is false, `mutate()` is called and, on success, the due threshold is marked consumed

### Requirement: Due-Mutation Scheduling Anchors to the Triggering Threshold, Not Delivery Time

The scheduler MUST remember the specific loop-count value at which a threshold crossing became due, and on successful delivery MUST advance its "last triggered" baseline to that remembered due value — NOT to whatever `getLoopCount()` reads at the moment of successful delivery. This keeps the cadence anchored to when the mutation was actually scheduled, so a busy-retry delay of one or more ticks cannot drift the schedule forward or cause a catch-up burst.

#### Scenario: A delayed delivery does not drift the schedule

- GIVEN a due threshold is remembered at loop-count value V, and delivery is delayed by busy retries until `getLoopCount()` has since advanced past V
- WHEN `mutate()` finally succeeds
- THEN the "last triggered" baseline advances to V (the remembered due value), not to the later count observed at delivery
- AND the next threshold is due at V + N, preserving the original N-loop cadence

### Requirement: Inherits Phase-Reset-On-Publish (Non-Goal, Not Re-Decided Here)

Automatic mutations published by Auto-Evolve MUST reset transport/playhead to step 0 on adopt, identically to manual Mutate (decision #209, `sequence-mutation`'s "Publish Always Resets Playback Phase"). This is inherited, unchanged behavior — this spec does not introduce or re-litigate a phase-preserving publish path.

#### Scenario: Automatic mutation restarts the loop at step 0

- GIVEN playback running when an automatic mutation is successfully published
- THEN playback resumes from step 0 on the next process block, same as a manual mutation

### Requirement: Auto-Evolve State Is Session-Only, Not Persisted

Auto-Evolve's enabled flag and rate MUST NOT be written to or read from preset files. `Source/preset/Preset.h` MUST remain `{name, patch, seed}` with no schema/version change for this feature, consistent with `sequenceSeed`/`mutationCount` already being excluded and reset on preset load.

#### Scenario: Preset save/load does not touch Auto-Evolve state

- GIVEN Auto-Evolve is enabled at some rate
- WHEN a preset is saved and later loaded
- THEN the saved preset contains no Auto-Evolve fields, and loading it leaves the current Auto-Evolve enabled/rate UI state untouched
