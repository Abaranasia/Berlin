# Delta for Realtime Audio Wiring

## MODIFIED Requirements

### Requirement: Allocation-Free, Lock-Free, Log-Free Audio Callback

The system MUST NOT allocate heap memory or call any logging/`juce::String`-formatting function inside `getNextAudioBlock`, for any block size. The system MUST NOT acquire any lock inside `getNextAudioBlock` **except** the one named, documented exception below. This is a structural property verified by code review against the project's real-time-safety constitution rule, not by a generic runtime assertion.
(Previously: forbade any lock acquisition with no exception; now permits exactly one named, bounded exception for MIDI dispatch.)

**Named exception**: `juce::MidiOutput::sendBlockOfMessages`, invoked from the MIDI dispatch step, takes a short, bounded, internal lock to append to its own pending-message list. No I/O, no unbounded-size allocation, and no driver syscall occurs on the calling (audio) thread as part of this lock. This is the only lock permitted anywhere in the callback's call path.

#### Scenario: Callback body contains no allocation or logging call

- GIVEN the implementation of `getNextAudioBlock`
- WHEN its body is inspected during code review
- THEN no heap allocation and no `juce::String`/`Logger` call is present anywhere in the call path executed per block

#### Scenario: Exactly one documented lock exists in the callback path

- GIVEN the implementation of `getNextAudioBlock`
- WHEN its full call path is inspected during code review
- THEN the only lock acquisition found is the documented `sendBlockOfMessages` internal lock, and no other lock acquisition is present

### Requirement: Output Stays Silent

The system MUST NOT produce any audible **audio** signal in `getNextAudioBlock`: the audio output buffer stays cleared, and no oscillator, filter, envelope, or voice is introduced. This requirement governs audio output only; the callback legitimately emits MIDI output as part of MIDI dispatch.
(Previously: prohibited any audible signal without distinguishing audio from MIDI output, since no MIDI output existed yet.)

#### Scenario: Audio buffer remains silent after processing

- GIVEN `getNextAudioBlock` has advanced the transport, player, and MIDI dispatch for a block
- WHEN the audio output buffer contents are inspected
- THEN every sample remains at its cleared (silent) value

#### Scenario: MIDI output does not violate audio silence

- GIVEN a block in which MIDI events are dispatched to an output device
- WHEN the audio output buffer contents are inspected
- THEN the audio buffer is still fully cleared, because MIDI dispatch is a distinct output path from the audio buffer
