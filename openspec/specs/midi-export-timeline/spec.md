# MIDI Export Timeline Specification

## Purpose

A JUCE-free, tick-domain transform: a `Sequence`, a tempo/`stepsPerBeat` pair, and a repeat count become an ordered, tick-stamped note-on/note-off event timeline — the sole source of musical correctness for the exported `.mid` file, mirroring live playback's note-off convention exactly.

## Requirements

### Requirement: Integer Tick Derivation from PPQ and Steps-Per-Beat

The system MUST derive ticks-per-step from a PPQ constant that is evenly divisible by `stepsPerBeat`, so every step boundary lands on an exact integer tick with no sample rate, no floating-point round-trip, and no rounding error accumulated across repeats.

#### Scenario: Step boundaries land on exact integer ticks

- GIVEN a chosen PPQ divisible by `stepsPerBeat`
- WHEN ticks-per-step is computed
- THEN it is an exact integer, and every step index `n`'s boundary tick equals `n * ticksPerStep` with no fractional remainder

#### Scenario: No accumulated drift across repeats

- GIVEN a sequence exported with repeat count *N* > 1
- WHEN tick positions are computed for every repeat
- THEN each repeat's step boundaries land on the same exact multiples as a single pass, offset by whole multiples of the pattern's total tick length, with zero rounding drift

### Requirement: Ordered Events With Note-Off Before Note-On

The system MUST emit, for each active step, a note-off event at the tick where that step ends and the next step begins, immediately followed (at the same tick) by the next active step's note-on. The full emitted event list MUST be non-decreasing in tick position, and at any shared tick a note-off MUST precede a note-on.

#### Scenario: Consecutive active steps share a boundary tick

- GIVEN two consecutive active steps
- WHEN their events are emitted
- THEN the first step's note-off and the second step's note-on both appear at the shared boundary tick, with the note-off ordered first

#### Scenario: Full timeline is tick-ordered

- GIVEN a sequence with multiple active steps across multiple repeats
- WHEN the complete event list is produced
- THEN every event's tick is greater than or equal to the previous event's tick

### Requirement: Terminal Note-Off Correctness

Because live playback loops forever but an exported file must terminate cleanly, the system MUST emit a note-off for the last sounding note at the final tick of the last repeat (`repeats * sequence.size() * ticksPerStep`), even though no further step follows it. No note-on in the exported timeline MUST be left without a matching note-off.

#### Scenario: Final active step gets a closing note-off

- GIVEN a sequence whose last step is active, exported with repeat count *N*
- WHEN the timeline is built
- THEN a note-off for that step's note appears at the exact final tick of the grid, and no note-on anywhere in the timeline lacks a matching note-off

### Requirement: Configurable Repeat Count With Contiguous Seams

The system MUST accept a repeat count of 1 or greater and produce `repeats * sequence.size()` contiguous steps with no gap or overlap at any loop seam. A repeat count less than 1 MUST be rejected (e.g. an error/exception), never silently reinterpreted as 1 or ignored.

#### Scenario: Repeats produce contiguous, non-overlapping passes

- GIVEN a sequence exported with repeat count *N* ≥ 2
- WHEN the timeline is inspected at each repeat boundary
- THEN the last step of one repeat and the first step of the next produce no gap and no duplicated event at the seam

#### Scenario: Invalid repeat count is rejected

- GIVEN a repeat count of 0 or negative
- WHEN the timeline is requested
- THEN the system rejects the request instead of producing a timeline with a silently substituted count

### Requirement: Fixed Velocity From the Shared Constant

Every note-on event in the timeline MUST carry the existing fixed velocity constant (`MidiEventTranslator::kNoteVelocity`). No per-step or per-note velocity value is introduced.

#### Scenario: Every note-on uses the shared velocity constant

- GIVEN any sequence with one or more active steps
- WHEN the timeline is built
- THEN every emitted note-on's velocity equals `kNoteVelocity`, with no other value present anywhere in the timeline

### Requirement: Inactive Steps and Degenerate Inputs Emit No Notes

An inactive step MUST produce no note-on/note-off pair. A `Sequence` with zero steps, or one where every step is inactive, MUST produce a timeline with zero note events for any valid repeat count.

#### Scenario: Inactive step is silent

- GIVEN a step with `active = false`
- WHEN the timeline is built
- THEN no note-on or note-off event is emitted for that step

#### Scenario: Empty or fully-inactive sequence yields no events

- GIVEN a `Sequence` with zero steps, or every step inactive, and any valid repeat count
- WHEN the timeline is built
- THEN the resulting event list is empty
