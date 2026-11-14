# Delta for Step Event Scheduling

## MODIFIED Requirements

### Requirement: Project-Local StepEvent Type

The system MUST provide a `StepEvent` value type with fields `{ sampleOffset, stepIndex, note, isNoteOn, velocity }`, `velocity` appended last. A note-on event MUST carry the originating step's `velocity`. Note-off events carry no meaningful velocity and MUST be unchanged in every other field. The system MUST NOT use `juce::MidiMessage`, `juce::MidiBuffer`, or any type from `juce_audio_basics`.
(Previously: four fields, no velocity.)

#### Scenario: Construct a StepEvent

- GIVEN values for `sampleOffset`, `stepIndex`, `note`, `isNoteOn` and `velocity`
- WHEN a `StepEvent` is constructed with them
- THEN each field reads back the given value

#### Scenario: No MIDI-library dependency

- GIVEN the `StepEvent` type definition and its translation unit's includes
- WHEN inspecting its dependencies
- THEN no `juce::MidiMessage`, `juce::MidiBuffer`, or `juce_audio_basics` symbol is referenced

## ADDED Requirements

### Requirement: Note-On Carries the Step Velocity

`SequencePlayer` MUST fill each note-on `StepEvent::velocity` from the `velocity` of the step that starts. Event count, order, sample offsets and note-off events MUST be identical to a sequence of default-velocity steps.

#### Scenario: Note-on velocity follows the step

- GIVEN steps with velocities 40, 100 and 127, all active
- WHEN a full cycle is processed
- THEN the note-ons carry 40, 100 and 127 in order

#### Scenario: Default velocity changes nothing else

- GIVEN a sequence of default-velocity steps
- WHEN processed
- THEN offsets, order and notes equal the pre-change behavior and every note-on velocity is 100
