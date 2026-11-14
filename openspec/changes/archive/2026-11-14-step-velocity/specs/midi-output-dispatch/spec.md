# Delta for MIDI Output Dispatch

Archive note: the capability Purpose says "on a fixed channel with fixed velocity"; it MUST read "fixed channel, per-event velocity". `kNoteVelocity` survives only as the default velocity.

## MODIFIED Requirements

### Requirement: Non-Allocating StepEvent-to-MidiBuffer Translation

The system MUST translate each `StepEvent` in a `StepEventBuffer` into a 3-byte MIDI message inside a pre-reserved `juce::MidiBuffer`, on the translator's configured channel, at a timestamp equal to `StepEvent::sampleOffset` unmodified. A note-on MUST use the event's `velocity`, defensively clamped to `[1, 127]` (never 0, which would read as a note-off). Note-off messages MUST be unchanged (real `0x80` status, release velocity 0). Translation MUST NOT grow the destination buffer (no `ensureSize` call inside translation).
(Previously: every note-on used a fixed velocity constant.)

#### Scenario: Multiple events translate in order

- GIVEN a `StepEventBuffer` containing several note-on/note-off events
- WHEN translation runs into a pre-reserved `juce::MidiBuffer`
- THEN each event appears as one MIDI message at its `sampleOffset` on the configured channel, with no buffer growth

#### Scenario: Note-on carries the event velocity

- GIVEN note-on events with velocities 40 and 127
- WHEN translated
- THEN the messages carry data2 40 and 127

#### Scenario: Out-of-range event velocity is clamped

- GIVEN note-on events with velocity 0 and 200
- WHEN translated
- THEN the messages carry data2 1 and 127

#### Scenario: Default velocity is byte-identical to before

- GIVEN note-on events with velocity 100
- WHEN translated
- THEN the bytes equal those produced before this change

#### Scenario: Empty input clears the destination

- GIVEN an empty `StepEventBuffer`
- WHEN translation runs
- THEN the destination buffer ends up cleared and empty
