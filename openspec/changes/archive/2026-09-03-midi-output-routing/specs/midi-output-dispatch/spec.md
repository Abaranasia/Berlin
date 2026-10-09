# MIDI Output Dispatch Specification

## Purpose

Translation of scheduled `StepEvent`s into MIDI on a fixed channel with fixed velocity, dispatched via `juce::MidiOutput::sendBlockOfMessages` with sample-accurate timestamps derived from elapsed audio samples, plus the one named, documented exception to the audio callback's no-lock rule and the panic guard sent on device close.

## Requirements

### Requirement: JUCE-Free MIDI Byte Packing

The system MUST provide `constexpr`, JUCE-free functions `clampMidiChannel` (clamping to `[1, 16]`), `clampMidiData` (clamping to `[0, 127]`), `makeNoteOn`, `makeNoteOff`, and `makeAllNotesOff` (CC 123), each returning a 3-byte status/data1/data2 value with no dependency on `juce_audio_basics`. Note-off MUST be a real `0x80` status byte, never note-on with velocity 0.

#### Scenario: Correct bytes for note-on and note-off

- GIVEN a channel and note/velocity values
- WHEN `makeNoteOn` and `makeNoteOff` are called
- THEN the returned bytes have status `0x90 | (channel-1)` and `0x80 | (channel-1)` respectively, with note-off's release velocity `0`

#### Scenario: Out-of-range channel and data values are clamped

- GIVEN a channel outside `[1, 16]` or a note/velocity outside `[0, 127]`
- WHEN the packing functions are called
- THEN the returned values are clamped into their valid ranges, never left out of range

### Requirement: Non-Allocating StepEvent-to-MidiBuffer Translation

The system MUST translate each `StepEvent` in a `StepEventBuffer` into a 3-byte MIDI message inside a pre-reserved `juce::MidiBuffer`, at a fixed velocity constant, on the translator's configured channel, at a timestamp equal to `StepEvent::sampleOffset` unmodified. Translation MUST NOT grow the destination buffer (no `ensureSize` call inside translation).

#### Scenario: Multiple events translate in order

- GIVEN a `StepEventBuffer` containing several note-on/note-off events
- WHEN translation runs into a pre-reserved `juce::MidiBuffer`
- THEN each event appears as one MIDI message at its `sampleOffset`, at the fixed velocity and configured channel, with no buffer growth

#### Scenario: Empty input clears the destination

- GIVEN an empty `StepEventBuffer`
- WHEN translation runs
- THEN the destination buffer ends up cleared and empty

### Requirement: Sample-Derived Timestamp for Dispatch

The system MUST compute the dispatch timestamp for each block from a start-of-playback anchor plus cumulative elapsed samples divided by sample rate, never from a per-block wall-clock read. The elapsed-sample counter MUST advance every block, including blocks with no MIDI events and blocks where no device is open.

#### Scenario: Timestamp follows the anchor-plus-elapsed formula

- GIVEN playback has been running for a known number of elapsed samples since preparation
- WHEN a block is dispatched
- THEN the timestamp equals the anchor plus elapsed samples converted to milliseconds at the current sample rate

#### Scenario: Elapsed samples advance without a device

- GIVEN no MIDI output device is open
- WHEN blocks are processed
- THEN the elapsed-sample counter still advances by each block's sample count

### Requirement: Named, Bounded Lock Exception in Dispatch

The system's use of `juce::MidiOutput::sendBlockOfMessages` from the audio thread MAY acquire that call's short internal lock; this MUST be the only lock acquisition permitted anywhere in the audio callback, and MUST remain bounded to list-append work with no I/O or unbounded-size allocation.

#### Scenario: Exactly one documented lock site exists

- GIVEN the audio callback's full call path for one block
- WHEN it is inspected during code review
- THEN the only lock acquisition found is the documented `sendBlockOfMessages` call, and no other lock exists anywhere in the path

### Requirement: All-Notes-Off Panic Guard On Close

The system MUST send an All Notes Off (CC 123) message on the configured channel when closing an open MIDI output device, independent of and in addition to any application-tracked note-off. Closing an already-closed device MUST be a no-op.

#### Scenario: Closing an open device sends the panic guard

- GIVEN an open MIDI output device
- WHEN the device is closed
- THEN a CC 123 All Notes Off message on the configured channel is sent before the close completes

#### Scenario: Closing an already-closed device is idempotent

- GIVEN a device that is already closed
- WHEN close is invoked again
- THEN no message is sent and no crash occurs
