# Sequencing Core Specification

## Purpose

Minimal domain value types for a musical step sequence: a single `Step`, a resizable `Sequence` of steps, and a `Scale` used to validate/query pitch membership. No timing, velocity, gate, duration, probability, or accent fields yet — those are explicit future additive work.

## Requirements

### Requirement: Step Value Type

The system MUST provide a `Step` value type with exactly two fields: `note` (integer pitch) and `active` (boolean gate flag). `Step` MUST NOT expose velocity, gate length, duration, probability, timing-offset, or accent fields.

#### Scenario: Construct a step

- GIVEN a `note` value and an `active` flag
- WHEN a `Step` is constructed with them
- THEN `step.note` equals the given note
- AND `step.active` equals the given flag

#### Scenario: Step has no extra fields

- GIVEN the `Step` type definition
- WHEN inspecting its public members
- THEN only `note` and `active` are present

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
