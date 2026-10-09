# MIDI File Output Specification

## Purpose

A thin `juce::MidiFile`/`juce::MidiMessageSequence` wrapper that translates an already-correct `MidiExportTimeline` into Standard MIDI File bytes — PPQ header, tempo meta-event, hardcoded 4/4 time-signature meta-event, fixed channel/velocity note messages, end-of-track, and a reported write outcome. It contains no musical decisions of its own.

## Requirements

### Requirement: SMF Header PPQ Matches the Timeline

The system MUST set the written file's ticks-per-quarter-note header to the exact PPQ constant used to build the source timeline, so an external reader interprets every tick position identically to how it was computed.

#### Scenario: Header PPQ equals the timeline's PPQ

- GIVEN a timeline built with a known PPQ constant
- WHEN the file is written
- THEN the file's ticks-per-quarter-note header equals that same PPQ value

### Requirement: Tempo Meta-Event From Transport BPM

The system MUST write exactly one tempo meta-event, at tick 0, derived from the transport BPM used to build the timeline.

#### Scenario: Tempo event reflects the transport BPM

- GIVEN a timeline built at a known BPM
- WHEN the file is written
- THEN exactly one tempo meta-event exists at tick 0, encoding that BPM

### Requirement: Hardcoded 4/4 Time-Signature Meta-Event

The system MUST write exactly one 4/4 time-signature meta-event at tick 0, regardless of sequence content, repeat count, or any other input. No runtime configuration of time signature exists.

#### Scenario: Time signature is always 4/4

- GIVEN any timeline, sequence content, or repeat count
- WHEN the file is written
- THEN exactly one time-signature meta-event exists at tick 0 encoding 4/4

### Requirement: Fixed-Channel, Fixed-Velocity Note Translation

The system MUST translate each timeline note-on/note-off pair into `juce::MidiMessage` note-on/note-off events on a single fixed channel, preserving the timeline's tick position and its fixed velocity unchanged. The writer MUST NOT introduce a different channel or velocity than what the timeline already carries.

#### Scenario: Written notes preserve channel, velocity, and tick

- GIVEN a timeline with note-on/note-off events
- WHEN the file is written
- THEN each written MIDI message carries the fixed channel, the timeline's fixed velocity, and the timeline's exact tick position

### Requirement: Well-Formed File With End-of-Track

The system MUST append an end-of-track meta-event after the last timeline-derived event, producing a file a DAW or MIDI file reader opens without error or repair prompt — verified by the project's manual gate (see Known Coverage Gap).

#### Scenario: Manual gate — file opens cleanly

- GIVEN an exported `.mid` file
- WHEN it is opened in a DAW or MIDI file reader
- THEN it opens without error or repair prompt, reporting the correct BPM, 4/4 signature, note pitches, channel, velocity, note lengths, and total bar count

### Requirement: Reported Write Success and Failure

The system MUST report whether writing to the destination succeeded or failed (e.g. a boolean return), and MUST NOT crash, throw uncaught, or silently report success when the write fails (e.g. an unwritable path or a missing directory).

#### Scenario: Successful write to a valid path

- GIVEN a writable destination path
- WHEN the file is written
- THEN the write reports success and the file exists with valid contents

#### Scenario: Write failure is reported, not swallowed or crashed

- GIVEN an unwritable destination (e.g. a read-only directory or a nonexistent path)
- WHEN a write is attempted
- THEN the write reports failure, no partial/corrupt file is left silently claimed as valid, and the application does not crash

### Requirement: Degenerate Timeline Still Produces a Valid File

An empty timeline (from an empty or fully-inactive `Sequence`) MUST still produce a valid `.mid` file containing the tempo and time-signature meta-events and end-of-track, with zero note events.

#### Scenario: Zero-note timeline yields a valid, empty-of-notes file

- GIVEN a timeline with zero note events
- WHEN the file is written
- THEN the resulting file is valid, contains the tempo and time-signature meta-events and end-of-track, and contains no note-on/note-off events

## Known Coverage Gap

This entire specification's requirements lack automated test coverage. `Source/export/MidiFileWriter.*` depends on `juce::MidiFile`/`juce::MidiMessageSequence` from `juce_audio_basics`, which is deliberately absent from `Tests/BerlinTests.jucer`'s `juce_core`-only module list (see `unit-test-harness`). All musical correctness is pushed into the JUCE-free `midi-export-timeline` specification, which is fully unit-tested; this writer only translates an already-correct timeline into bytes. Verification here is: (1) source code review of the thin translation, and (2) the human-confirmed manual gate — opening the exported `.mid` in a DAW or MIDI file reader and checking BPM, 4/4, note pitches, channel, velocity, note lengths, and total bar count for the configured repeat count. This is the same accepted tradeoff used for `Source/midi/MidiEventTranslator.*` and `Source/midi/MidiOutputSink.*` in Phases 4-5.
