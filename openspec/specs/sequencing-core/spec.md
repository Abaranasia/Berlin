# Sequencing Core Specification

## Purpose

Minimal domain value types for a musical step sequence: a single `Step`, a resizable `Sequence` of steps, and a `Scale` used to validate/query pitch membership. No timing, velocity, gate, duration, probability, or accent fields yet — those are explicit future additive work.

## Requirements

### Requirement: Step Value Type

The system MUST provide a `Step` value type with exactly three fields: `note` (integer pitch), `active` (boolean gate flag) and `skipped` (boolean, default `false`). `skipped` MUST be declared after `active` so `Step` stays an aggregate and `Step{note, active}` still compiles with `skipped == false`. A skipped step is removed from the playback/export cycle (takes no time); `active == false` on a non-skipped step remains a timed rest. `Step` MUST NOT expose velocity, gate length, duration, probability, timing-offset, or accent fields.
(Previously: exactly two fields, `note` and `active`.)

#### Scenario: Construct a step

- GIVEN a `note` value and an `active` flag
- WHEN a `Step` is constructed with them
- THEN `step.note` equals the given note
- AND `step.active` equals the given flag
- AND `step.skipped` is false

#### Scenario: Construct a skipped step

- GIVEN a `note`, an `active` flag and `skipped = true`
- WHEN a `Step` is aggregate-initialised with all three
- THEN each field reads back the given value

#### Scenario: Step has no extra fields

- GIVEN the `Step` type definition
- WHEN inspecting its public members
- THEN only `note`, `active` and `skipped` are present

### Requirement: Sequence Resizable Collection

The system MUST provide a `Sequence` type holding an ordered collection of `Step` that is resizable after construction (not fixed-length).

#### Scenario: Sequence grows after construction

- GIVEN a `Sequence` constructed with 4 steps
- WHEN the sequence is resized to 16 steps
- THEN `sequence.size()` equals 16

#### Scenario: Sequence shrinks after construction

- GIVEN a `Sequence` constructed with 16 steps
- WHEN the sequence is resized to 4 steps
- THEN `sequence.size()` equals 4

#### Scenario: Indexed step access

- GIVEN a `Sequence` with at least one step
- WHEN a step at a valid index is read or written
- THEN the operation affects only that index and leaves others unchanged

### Requirement: Scale Root and Intervals

The system MUST provide a `Scale` type composed of a root note and an ordered set of intervals, with static factory methods `Scale::minor(root)` and `Scale::major(root)` producing the corresponding diatonic scale. No other factory (e.g. dorian, phrygian, pentatonic) is in scope for this change.

#### Scenario: Major scale factory

- GIVEN a root note (e.g. C)
- WHEN `Scale::major(root)` is called
- THEN the resulting scale's degrees match the standard major interval pattern relative to root

#### Scenario: Minor scale factory

- GIVEN a root note (e.g. C)
- WHEN `Scale::minor(root)` is called
- THEN the resulting scale's degrees match the standard natural-minor interval pattern relative to root

### Requirement: Scale Degree and Containment Queries

The system MUST allow querying whether a given note belongs to a `Scale`, and MUST allow retrieving a scale degree by index.

#### Scenario: Note in scale

- GIVEN a `Scale` (e.g. C major)
- WHEN checking containment of a note that is a scale degree (e.g. E)
- THEN the containment query returns true

#### Scenario: Note not in scale

- GIVEN a `Scale` (e.g. C major)
- WHEN checking containment of a note that is not a scale degree (e.g. C#)
- THEN the containment query returns false

#### Scenario: Containment across octaves

- GIVEN a `Scale` with root C
- WHEN checking containment of the same pitch class in a different octave (e.g. E one or more octaves above/below root)
- THEN the containment query still returns true

### Requirement: Sequence Supports O(1) Swap

The system MUST provide `Sequence::swap(Sequence&)`, exchanging the contents of two `Sequence` instances in constant time with no heap allocation or deallocation. This exists specifically so the audio thread can adopt a newly-published `Sequence` (per `realtime-audio-wiring`'s handoff contract) without ever calling into the allocator.

#### Scenario: Swap exchanges contents

- GIVEN two distinct `Sequence` instances, A and B, with different contents
- WHEN `A.swap(B)` is called
- THEN A now holds B's original contents and B now holds A's original contents

#### Scenario: Swap allocates nothing

- GIVEN two `Sequence` instances of any size
- WHEN `swap` is called
- THEN no heap allocation or deallocation occurs, and the call is `noexcept`

### Requirement: Sequence Capacity Constant

The system MUST define a single `kMaxSteps = 64` constant as the maximum sequence length for generation, editing and display. `Sequence` itself remains resizable and MUST NOT be hard-limited by this constant.

#### Scenario: Constant value

- GIVEN the core headers
- WHEN `kMaxSteps` is read
- THEN it equals 64
