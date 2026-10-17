# Delta for Step Event Scheduling

## ADDED Requirements

### Requirement: Skipped Steps Leave the Cycle

`SequencePlayer` MUST treat a step with `skipped == true` as absent from the playback cycle: it MUST NOT emit note-on/note-off for it, and it MUST consume no time (the boundary that would have started it is not a step boundary). A non-skipped step with `active == false` MUST remain a timed rest, unchanged. The cursor MUST find the next non-skipped step by a forward scan bounded by the sequence size, with no allocation, lock or logging on the audio thread. A note sounding from the previous non-skipped step MUST end at the boundary where the next non-skipped step begins, with the existing note-off-before-note-on ordering.

#### Scenario: Skipped step takes no time

- GIVEN a 4-step sequence with step 1 skipped and all other steps active
- WHEN playback runs one full cycle
- THEN the note-ons occur for steps 0, 2, 3 on three consecutive step boundaries, with no gap where step 1 would have been

#### Scenario: Inactive non-skipped step is still a timed rest

- GIVEN a step with `active = false` and `skipped = false` between two active steps
- WHEN playback runs
- THEN the two active steps' note-ons are two step-durations apart

#### Scenario: Skipped step 0

- GIVEN a sequence whose step 0 is skipped
- WHEN playback runs
- THEN the first emitted step is the first non-skipped step and the cycle continues without ever emitting step 0

#### Scenario: All steps skipped

- GIVEN a non-empty sequence where every step is skipped
- WHEN `SequencePlayer` processes any number of blocks
- THEN no step events are emitted, `loopCount` does not increment, the playhead reports no current step (or holds its value), and there is no crash or hang

#### Scenario: No audio-thread allocation

- GIVEN the skip-aware cursor
- WHEN processing a sequence with many consecutive skipped steps
- THEN the scan is bounded by the sequence size and performs no allocation or locking

### Requirement: Keep-Position Publish With Skip and Length Changes

When a keep-position publish is adopted (step edit, including a skip toggle or any length change reaching the player without reset), the cursor MUST continue from its current index and MUST NOT reset to step 0. If that index is `>=` the new sequence size, or now lands on a skipped step, the cursor MUST advance to the next non-skipped step, wrapping to the start if needed, and that wrap MUST NOT increment `loopCount`.

#### Scenario: Edit does not reset the cursor

- GIVEN playback is at index 9 and an edit publishes with the same length
- WHEN it is adopted
- THEN playback continues from index 9 and `loopCount` is unchanged

#### Scenario: Cursor beyond the new length

- GIVEN playback is at index 12 and a keep-position publish shrinks the sequence to 8 steps
- WHEN it is adopted
- THEN the cursor moves to the first non-skipped step at or after the clamped position (wrapping to the start if none), without incrementing `loopCount`

#### Scenario: Cursor lands on a newly skipped step

- GIVEN the sounding or next cursor step is skipped by the adopted edit
- WHEN playback continues
- THEN the cursor advances to the next non-skipped step and no false loop is counted

## MODIFIED Requirements

### Requirement: Inactive Steps and Empty Sequence Emit Nothing

The system MUST emit no note-on/note-off events for a step whose `active` flag is false, nor for a step whose `skipped` flag is true. An empty `Sequence` (zero steps), a `Sequence` where every step is inactive, or a `Sequence` where every step is skipped, MUST produce no events at all.
(Previously: covered only inactive steps and empty or fully-inactive sequences.)

#### Scenario: Inactive step is silent

- GIVEN a step with `active = false`
- WHEN `SequencePlayer` processes the boundary containing that step
- THEN no note-on or note-off event is emitted for it

#### Scenario: Skipped step is silent

- GIVEN a step with `skipped = true`
- WHEN `SequencePlayer` processes a full cycle
- THEN no note-on or note-off event is emitted for it

#### Scenario: Empty, fully-inactive or fully-skipped sequence emits nothing

- GIVEN a `Sequence` with zero steps, with every step inactive, or with every step skipped
- WHEN `SequencePlayer` processes any number of blocks
- THEN no events are emitted

### Requirement: Continuous Emission Across the Loop Wrap

The system MUST continue emitting correctly-timed events across the loop boundary: the last non-skipped step's note-off (if any) and the wrap to the first non-skipped step MUST occur with no timing gap and no duplicated or dropped event, regardless of skipped steps at the end or start of the sequence.
(Previously: wrap was from step N-1 to step 0.)

#### Scenario: Wrap boundary has no gap or duplicate

- GIVEN a `Sequence` of N steps being processed past its final non-skipped step
- WHEN `SequencePlayer` processes the boundary between the last and the first non-skipped step
- THEN any pending note-off and the first non-skipped step's note-on (if active) are emitted at their correct, contiguous sample offsets with no gap and no duplicate event

#### Scenario: Trailing and leading skips do not add time at the wrap

- GIVEN a sequence with a skipped last step and a skipped step 0
- WHEN playback wraps
- THEN the wrap boundary is exactly one step duration after the last non-skipped step's start

### Requirement: Step Position Wraps Indefinitely

The system MUST expose, via `SequencePlayer`, a step position (playhead) that advances over non-skipped steps with each `Transport` boundary and wraps from the last non-skipped step back to the first non-skipped step, continuing indefinitely. `SequencePlayer` combines `Transport`'s boundary count with the driving `Sequence` (its length and skip flags) to compute this; `Transport` itself has no loop-length concept. When every step is skipped, the player MUST report no current step (or hold its last value) and MUST NOT advance or wrap.
(Previously: wrapped to 0 upon completing `Sequence::size()` steps.)

#### Scenario: Step position wraps at loop end

- GIVEN a `Sequence` of N steps with none skipped and a running `SequencePlayer`
- WHEN the playhead reaches step N-1 and one more boundary is crossed
- THEN the reported next step position is 0, with no gap or skipped boundary

#### Scenario: Wrap skips skipped steps

- GIVEN a 4-step sequence with steps 0 and 3 skipped
- WHEN the playhead is at step 2 and one more boundary is crossed
- THEN the next step position is 1

### Requirement: Loop Completion Counter

The system MUST expose, via `SequencePlayer`, an RT-safe `std::atomic<int> loopCount` and `getLoopCount() const noexcept`, mirroring the existing `playhead`/`getPlayheadStep()` cross-thread pattern exactly. `loopCount` MUST increment exactly once per genuine loop completion, defined as the playback cursor crossing from the last non-skipped step back to the first non-skipped step (one pass over the non-skipped steps) during normal playback advancement. A skipped step 0 MUST NOT break loop counting. `loopCount` MUST NOT increment when the cursor is set by `reset()` (e.g. on whole-sequence publish-adopt), when a keep-position adoption re-positions the cursor (see "Keep-Position Publish With Skip and Length Changes"), or when all steps are skipped. `loopCount` MUST remain monotonic — never reset or decremented.
(Previously: increment defined as the step position wrapping from a non-zero step back to 0.)

#### Scenario: Loop completion increments the counter

- GIVEN a running `SequencePlayer` processing a `Sequence` of N non-skipped steps
- WHEN the cursor wraps from step N-1 back to step 0 during normal advancement
- THEN `getLoopCount()` reflects exactly one additional completed loop

#### Scenario: Loop counting with a skipped step 0

- GIVEN a sequence of 4 steps with step 0 skipped
- WHEN the cursor passes from step 3 back to step 1
- THEN `getLoopCount()` increments exactly once per such pass

#### Scenario: Single non-skipped step loops every boundary

- GIVEN a sequence where exactly one step is non-skipped
- WHEN playback advances across successive boundaries
- THEN `loopCount` increments once per boundary

#### Scenario: Publish-triggered reset does not count as a loop completion

- GIVEN a running `SequencePlayer` with `loopCount` at value V, mid-loop at a non-zero step
- WHEN a new sequence is published and adopted, calling `reset()` and setting the step position to 0
- THEN `getLoopCount()` remains at V, unchanged by the reset

#### Scenario: Counter stays exact across many wraps, including around publishes

- GIVEN a `SequencePlayer` processed across many loop wraps interspersed with occasional publish-adopts
- WHEN `getLoopCount()` is inspected after each wrap and after each adopt
- THEN it increases by exactly one per genuine loop completion only, with no missed or double counts

#### Scenario: Device restart does not fabricate a loop completion

- GIVEN a running `SequencePlayer` mid-loop at a non-zero step, with `loopCount` at value V
- WHEN `prepare()` is called and playback then advances past the first wrap boundary
- THEN `getLoopCount()` is exactly V + 1 for that one genuine boundary

#### Scenario: All steps skipped never increments

- GIVEN a sequence where every step is skipped
- WHEN playback runs for many boundaries
- THEN `getLoopCount()` is unchanged
