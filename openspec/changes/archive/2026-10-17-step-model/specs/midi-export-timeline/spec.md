# Delta for MIDI Export Timeline

## ADDED Requirements

### Requirement: Skipped Steps Take Zero Ticks

The export timeline MUST mirror live playback for skip exactly: a step with `skipped == true` MUST emit no events and MUST occupy zero ticks, so the following non-skipped step starts at the tick where the skipped step would have begun. A non-skipped step with `active == false` MUST remain a silent step that occupies one `ticksPerStep`. The timeline's total length MUST be `repeats * nonSkippedCount * ticksPerStep`. A skipped step 0 MUST NOT shift or leave a gap at the start. A sequence where every step is skipped MUST produce zero note events for any valid repeat count.

#### Scenario: Skipped step occupies no ticks

- GIVEN a 4-step all-active sequence with step 1 skipped, repeats = 1
- WHEN the timeline is built
- THEN note-ons occur at ticks 0, `ticksPerStep`, `2*ticksPerStep` for steps 0, 2, 3, and the final note-off is at `3*ticksPerStep`

#### Scenario: Inactive non-skipped step still takes time

- GIVEN steps active, inactive (not skipped), active, repeats = 1
- WHEN the timeline is built
- THEN the second active step's note-on is at `2*ticksPerStep`

#### Scenario: Skipped step 0 starts at tick 0

- GIVEN a sequence whose step 0 is skipped and step 1 is active
- WHEN the timeline is built
- THEN step 1's note-on is at tick 0

#### Scenario: All skipped yields no events

- GIVEN every step is skipped and any valid repeat count
- WHEN the timeline is built
- THEN the event list is empty

#### Scenario: Export matches live playback

- GIVEN a sequence containing skipped, inactive and active steps
- WHEN the export timeline's note order and relative spacing are compared with `SequencePlayer`'s emitted events over the same number of cycles
- THEN they are identical (shared skip-rule test cases)

## MODIFIED Requirements

### Requirement: Terminal Note-Off Correctness

Because live playback loops forever but an exported file must terminate cleanly, the system MUST emit a note-off for the last sounding note at the final tick of the last repeat (`repeats * nonSkippedCount * ticksPerStep`, where `nonSkippedCount` is the number of non-skipped steps; equals `sequence.size()` when none are skipped), even though no further step follows it. No note-on in the exported timeline MUST be left without a matching note-off.
(Previously: final tick was `repeats * sequence.size() * ticksPerStep`.)

#### Scenario: Final active step gets a closing note-off

- GIVEN a sequence whose last non-skipped step is active, exported with repeat count *N*
- WHEN the timeline is built
- THEN a note-off for that step's note appears at the exact final tick of the grid, and no note-on anywhere in the timeline lacks a matching note-off

### Requirement: Configurable Repeat Count With Contiguous Seams

The system MUST accept a repeat count of 1 or greater and produce `repeats * nonSkippedCount` contiguous steps (skipped steps excluded) with no gap or overlap at any loop seam. A repeat count less than 1 MUST be rejected (e.g. an error/exception), never silently reinterpreted as 1 or ignored.
(Previously: `repeats * sequence.size()` steps.)

#### Scenario: Repeats produce contiguous, non-overlapping passes

- GIVEN a sequence exported with repeat count *N* ≥ 2
- WHEN the timeline is inspected at each repeat boundary
- THEN the last non-skipped step of one repeat and the first non-skipped step of the next produce no gap and no duplicated event at the seam

#### Scenario: Invalid repeat count is rejected

- GIVEN a repeat count of 0 or negative
- WHEN the timeline is requested
- THEN the system rejects the request instead of producing a timeline with a silently substituted count

### Requirement: Inactive Steps and Degenerate Inputs Emit No Notes

An inactive step MUST produce no note-on/note-off pair. A skipped step MUST produce none either. A `Sequence` with zero steps, one where every step is inactive, or one where every step is skipped, MUST produce a timeline with zero note events for any valid repeat count.
(Previously: skipped steps and fully-skipped sequences were not addressed.)

#### Scenario: Inactive step is silent

- GIVEN a step with `active = false`
- WHEN the timeline is built
- THEN no note-on or note-off event is emitted for that step

#### Scenario: Empty, fully-inactive or fully-skipped sequence yields no events

- GIVEN a `Sequence` with zero steps, every step inactive, or every step skipped, and any valid repeat count
- WHEN the timeline is built
- THEN the resulting event list is empty
