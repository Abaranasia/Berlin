# Realtime Audio Wiring Specification

## Purpose

The `MainComponent` lifecycle contract for driving `Transport` and `SequencePlayer` from the real JUCE audio callback: the sequence is generated before audio starts, timing is derived once in `prepareToPlay`, `getNextAudioBlock` stays allocation-free/lock-free/log-free and outputs silence, and exactly one atomic playhead is exposed as the observability seam for later phases.

## Requirements

### Requirement: Sequence Built Before Audio Starts

The system MUST generate/construct the `Sequence` on the message thread before `setAudioChannels` is called, so the audio thread only ever reads an immutable, fully-built `Sequence` — never one being generated or mutated concurrently.

#### Scenario: Sequence exists and is immutable before audio starts

- GIVEN `MainComponent`'s constructor runs
- WHEN `setAudioChannels` is invoked
- THEN a complete, immutable `Sequence` already exists and is never modified afterward from the audio thread

### Requirement: Timing Derived in prepareToPlay

The system MUST derive `Transport`'s samples-per-step from the `sampleRate` argument JUCE supplies to `prepareToPlay`, not compute or recompute it inside `getNextAudioBlock`.

#### Scenario: prepareToPlay configures the transport

- GIVEN JUCE calls `prepareToPlay(samplesPerBlockExpected, sampleRate)`
- WHEN `MainComponent` handles this callback
- THEN `Transport::prepare(sampleRate)` is invoked and its resulting `samplesPerStep` reflects that `sampleRate`

#### Scenario: Repeated prepareToPlay updates timing consistently

- GIVEN `prepareToPlay` is called again with a different `sampleRate` (e.g. device change)
- WHEN the transport is re-prepared
- THEN `samplesPerStep` is recomputed from the new `sampleRate` and no stale value from the previous rate is used

### Requirement: Allocation-Free, Lock-Free, Log-Free Audio Callback

The system MUST NOT allocate heap memory or call any logging/`juce::String`-formatting function inside `getNextAudioBlock`, for any block size. The system MUST NOT acquire any lock inside `getNextAudioBlock` **except** the one named, documented exception below. This is a structural property verified by code review against the project's real-time-safety constitution rule, not by a generic runtime assertion.

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

#### Scenario: Audio buffer remains silent after processing

- GIVEN `getNextAudioBlock` has advanced the transport, player, and MIDI dispatch for a block
- WHEN the audio output buffer contents are inspected
- THEN every sample remains at its cleared (silent) value

#### Scenario: MIDI output does not violate audio silence

- GIVEN a block in which MIDI events are dispatched to an output device
- WHEN the audio output buffer contents are inspected
- THEN the audio buffer is still fully cleared, because MIDI dispatch is a distinct output path from the audio buffer

### Requirement: Single Atomic Playhead Observability Seam

The system MUST expose exactly one `std::atomic<int>` playhead, written by the audio thread to reflect the current step position, safely readable from another thread without locks. Nothing in this change consumes it; it exists for later phases.

#### Scenario: Playhead advances with the transport

- GIVEN audio is running and steps are advancing
- WHEN the atomic playhead is read from a non-audio thread after several blocks
- THEN its value reflects a current or very recently current step position, obtained without any lock

### Requirement: Standalone App Still Launches and Runs Silently

The system MUST still allow the standalone app to launch, open its audio device, and run the transport continuously without dropouts or asserts. This is a manual smoke gate, not automatable in the headless `juce_core`-only test harness.

#### Scenario: Manual launch gate passes

- GIVEN the built standalone app
- WHEN it is launched manually
- THEN it opens its audio device and runs the transport silently, with no dropouts, asserts, or crashes, until manually closed
