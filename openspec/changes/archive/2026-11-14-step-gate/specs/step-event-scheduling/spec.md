# Delta for Step Event Scheduling

Built on top of the pending `step-velocity` delta (which adds `velocity` to `StepEvent`). `StepEvent` gets no new field in this change.

## MODIFIED Requirements

### Requirement: Note-Off Before Note-On At a Shared Boundary

A note MUST be turned off no later than the next step boundary. A step with `gatePercent == 100` MUST emit its note-off at that boundary, exactly as before this change. A step with `gatePercent < 100` MUST emit its note-off earlier, per "Gate-Scaled Note-Off". When a note-off and the following step's note-on fall at the same sample offset, the system MUST order the note-off before the note-on.
(Previously: because `Step` had no gate field, every note sounded for its full step and the note-off was always emitted at the next step boundary.)

#### Scenario: Coincident boundary orders note-off first

- GIVEN an active step followed immediately by another active step, both at gate 100
- WHEN `SequencePlayer` processes across their shared boundary
- THEN the emitted note-off for the ending step appears before the note-on for the starting step at that sample offset

#### Scenario: Gate 100 is bit-identical to before

- GIVEN any sequence whose steps all have `gatePercent == 100`
- WHEN it is processed over any block sizes, tempo and loop count
- THEN the emitted events (count, order, sample offsets, notes, velocities) equal those produced before this change
- AND the existing golden, reproducibility and handoff suites pass unmodified

## ADDED Requirements

### Requirement: Gate-Scaled Note-Off

For an active step with `gatePercent < 100`, `SequencePlayer` MUST emit the note-off a number of samples after that step's note-on equal to `max(1, round(samplesPerStep * gatePercent / 100))`, instead of at the next boundary. The note-off MUST be emitted at that exact sample even when it falls in a later audio block than the note-on, and MUST be emitted exactly once per note. Within every block the complete emitted event list MUST be non-decreasing in sample offset, and the note-off for a note MUST precede any later note-on. A step that is inactive or skipped emits no events regardless of its `gatePercent`, and its gate does not change which step is next or how long any step lasts. The gate is a fraction of the step that owns the note.

#### Scenario: Short gate ends the note inside the step

- GIVEN an active step with `gatePercent == 50`, 24000 samples per step, followed by an active step
- WHEN the player processes across both steps
- THEN the first note-on is at the step boundary, its note-off is 12000 samples later, and the next note-on is at the following boundary
- AND event offsets are non-decreasing

#### Scenario: Gate levels scale the note length

- GIVEN 24000 samples per step and active steps with `gatePercent` 25, 50, 75 and 100
- WHEN a full cycle is processed
- THEN the note lengths are 6000, 12000, 18000 and 24000 samples respectively

#### Scenario: Note-off in a later block

- GIVEN an active step with `gatePercent == 75` whose note-on is in one block and whose note-off sample falls in the next block
- WHEN the two blocks are processed
- THEN the note-off is emitted in the later block at the correct block-relative offset and exactly one note-off is emitted for the note

#### Scenario: Gate rounding to one sample

- GIVEN a step length so short that `samplesPerStep * gatePercent / 100` rounds to 0 (for example `gatePercent == 1` with 10 samples per step)
- WHEN the step is processed
- THEN the note-off is emitted exactly 1 sample after the note-on, never at the same sample as its note-on
- AND event offsets stay non-decreasing

#### Scenario: Tempo change in the middle of a gate

- GIVEN an active step with `gatePercent == 50` that is sounding and the tempo changes before its note-off
- WHEN processing continues
- THEN the note-off is still emitted at the absolute sample computed when the note started
- AND if the next boundary now falls before that sample, the note-off is emitted first, no later than that boundary, and before the next note-on
- AND no note-off is lost or duplicated

#### Scenario: Gate on rest and skipped steps is ignored

- GIVEN an inactive step and a skipped step, each with `gatePercent == 25`, between two active gate-100 steps
- WHEN a full cycle is processed
- THEN the events and their offsets equal those of the same sequence with `gatePercent == 100` on those steps

#### Scenario: Stop or shutdown after a short gate

- GIVEN a note has already been turned off by its short gate
- WHEN the player is stopped or flushed
- THEN no second note-off is emitted for that note
- AND if the note is still sounding at stop, exactly one note-off is emitted, as before this change

#### Scenario: Event buffer capacity unchanged

- GIVEN any sequence and block size that fit before this change
- WHEN gate-scaled note-offs are emitted
- THEN the number of events per block is no greater than before and the fixed-capacity buffer does not newly overflow
