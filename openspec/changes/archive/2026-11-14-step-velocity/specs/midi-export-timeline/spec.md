# Delta for MIDI Export Timeline

## RENAMED Requirements

### Requirement: Fixed Velocity From the Shared Constant → Per-Step Velocity On Note-On

(Reason: velocity is now per step, not a shared constant.)
(Migration: tests that assert `kNoteVelocity` on every note-on assert the step's velocity instead.)

## MODIFIED Requirements

### Requirement: Per-Step Velocity On Note-On

Every note-on event in the timeline (`MidiExportEvent`, with `velocity` appended last) MUST carry the `velocity` of the step that produced it, in every repeat. Note-off events MUST be unchanged. A sequence whose steps all have the default velocity 100 MUST produce a timeline, and exported bytes, identical to those before this change.
(Previously: every note-on carried the fixed `MidiEventTranslator::kNoteVelocity`; no per-step velocity.)

#### Scenario: Each note-on uses its step's velocity

- GIVEN active steps with velocities 40, 70 and 127, repeats = 2
- WHEN the timeline is built
- THEN the note-ons carry 40, 70, 127, 40, 70, 127 in order

#### Scenario: Default-velocity sequence is unchanged

- GIVEN a sequence of default-velocity steps
- WHEN the timeline is built and written
- THEN the timeline and file bytes equal those produced before this change

#### Scenario: Skipped and inactive steps contribute no velocity

- GIVEN a skipped step and an inactive step, each with velocity 127
- WHEN the timeline is built
- THEN no event carries 127
