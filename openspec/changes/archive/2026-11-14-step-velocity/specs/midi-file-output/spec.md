# Delta for MIDI File Output

Archive note: the Purpose line "fixed channel/velocity note messages" and the manual-gate wording MUST be updated to "fixed channel, per-event velocity".

## RENAMED Requirements

### Requirement: Fixed-Channel, Fixed-Velocity Note Translation → Fixed-Channel, Per-Event-Velocity Note Translation

(Reason: velocity now comes from each timeline event.)
(Migration: `MidiFileWriter` no longer takes a fixed-velocity constructor argument; callers and tests construct it with the channel only.)

## MODIFIED Requirements

### Requirement: Fixed-Channel, Per-Event-Velocity Note Translation

The system MUST translate each timeline note-on/note-off pair into `juce::MidiMessage` note-on/note-off events on a single fixed channel, preserving the timeline's tick position and writing each note-on with that event's own velocity. `MidiFileWriter` MUST NOT take a fixed velocity and MUST NOT substitute another velocity or channel. Default-velocity sequences MUST export byte-identically to before this change.
(Previously: a fixed velocity was a constructor argument and applied to every note.)

#### Scenario: Written notes preserve channel, velocity, and tick

- GIVEN a timeline with note-on events of different velocities
- WHEN the file is written
- THEN each note-on carries the fixed channel, its own event velocity and the exact tick position

#### Scenario: Default velocity bytes unchanged

- GIVEN a timeline of velocity-100 notes
- WHEN the file is written
- THEN the bytes equal those written before this change

#### Scenario: Manual gate (M)

- GIVEN an exported file with mixed velocities opened in a DAW
- WHEN note velocities are inspected
- THEN they match the step velocities
