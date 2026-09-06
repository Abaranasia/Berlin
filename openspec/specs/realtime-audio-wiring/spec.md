# Realtime Audio Wiring Specification

## Purpose

The `MainComponent` lifecycle contract for driving `Transport` and `SequencePlayer` from the real JUCE audio callback: the sequence is generated before audio starts, timing is derived once in `prepareToPlay`, `getNextAudioBlock` stays allocation-free/lock-free/log-free, audio output is governed by the internal synth's enable state (silent when disabled or no note sounds — see `internal-synth-output` for the synth path itself), and exactly one atomic playhead is exposed as the observability seam for later phases.

## Requirements

### Requirement: Sequence Built Before Audio Starts, Replaceable Only Via Published Handoff

The system MUST generate/construct the initial `Sequence` on the message thread before `setAudioChannels` is called, so the audio thread only ever reads a complete, fully-built `Sequence`. After construction, the live `Sequence` MAY be replaced at runtime by a subsequent, fully-built `Sequence`, but ONLY through the message-thread-to-audio-thread publish/adopt handoff defined by `generation-live-control`. The audio thread MUST NOT observe a partially-built, torn, or half-written `Sequence` at any point during a replacement.

(Previously: stated the Sequence "is never modified afterward"; regeneration now makes replacement possible, gated by the handoff contract.)

#### Scenario: Sequence exists and is immutable before audio starts

- GIVEN `MainComponent`'s constructor runs
- WHEN `setAudioChannels` is invoked
- THEN a complete, fully-built `Sequence` already exists

#### Scenario: Initial sequence is never replaced without an explicit handoff

- GIVEN audio is running with the initially constructed `Sequence`
- WHEN no Generate/Randomize action has occurred
- THEN the audio thread continues reading that same `Sequence` unchanged indefinitely

#### Scenario: Replacement is always a complete Sequence, never a mix

- GIVEN a Generate or Randomize action has built a new `Sequence` on the message thread
- WHEN the audio thread adopts it via the published handoff
- THEN the audio thread observes either the complete previous `Sequence` or the complete new one, never partially-constructed step data from both (the `SequencePlayer` publish/adopt handoff: `pendingSequence` staging slot + `std::atomic<bool> sequencePending`, adopted via `Sequence::swap` at the top of `process()`)

#### Scenario: No torn read during concurrent build and playback

- GIVEN the message thread is constructing a new `Sequence` while the audio thread renders the previous one
- WHEN the audio thread reads sequence data for the current block
- THEN it reads only fields belonging to one complete, previously-published `Sequence`, never a torn combination (same handoff mechanism: the audio thread only ever reads `sequence`, which it alone replaces via `swap`; the message thread never touches `sequence` directly, only `pendingSequence`)

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

### Requirement: Audio Output Governed By Synth Enable State

The system MUST produce audible audio output in `getNextAudioBlock` when the internal synth is enabled and a note is sounding: the internal synth (oscillator, filter, envelope, LFO, and optional delay/reverb) renders into the audio output buffer. When the internal synth is disabled, or no note is sounding, the audio output buffer MUST contain only silence (zero samples) — no oscillator, filter, envelope, or voice outside the internal synth path may introduce sound. This requirement governs the audio output path only; the callback continues to emit MIDI output as a distinct dispatch path.

#### Scenario: Audio buffer carries synth output when enabled and a note sounds

- GIVEN the internal synth is enabled and a `StepEvent` note-on has been dispatched
- WHEN the block is rendered
- THEN the audio output buffer contains the synth's rendered signal, not silence

#### Scenario: Audio buffer stays silent when the synth is disabled

- GIVEN the internal synth enable toggle is off
- WHEN `getNextAudioBlock` processes a block, regardless of any `StepEvent` note-on
- THEN the audio output buffer remains at its cleared (silent) value

#### Scenario: Audio buffer stays silent when no note is sounding

- GIVEN the internal synth is enabled but no note-on has occurred since the last full envelope release
- WHEN a block is rendered
- THEN the audio output buffer remains silent

#### Scenario: MIDI output does not violate audio silence when the synth is disabled

- GIVEN a block in which MIDI events are dispatched to an output device and the internal synth is disabled
- WHEN the audio output buffer contents are inspected
- THEN the audio buffer remains cleared, because MIDI dispatch is a distinct output path from the audio buffer

### Requirement: Single Atomic Playhead Observability Seam

The system MUST expose exactly one `std::atomic<int>` playhead, written by the audio thread to reflect the current step position, safely readable from another thread without locks. Nothing in this change consumes it; it exists for later phases.

#### Scenario: Playhead advances with the transport

- GIVEN audio is running and steps are advancing
- WHEN the atomic playhead is read from a non-audio thread after several blocks
- THEN its value reflects a current or very recently current step position, obtained without any lock

### Requirement: Standalone App Still Launches And Runs Without Dropouts

The system MUST still allow the standalone app to launch, open its audio device, and run the transport continuously without dropouts or asserts, whether or not the internal synth is currently producing audible output. This is a manual smoke gate, not automatable in the headless test harness for the audio-device-touching path.

#### Scenario: Manual launch gate passes with the synth producing audible sound

- GIVEN the built standalone app with the internal synth enabled by default
- WHEN it is launched manually
- THEN it opens its audio device and runs the transport continuously with no dropouts, asserts, or crashes, until manually closed, whether or not sound is currently audible

#### Scenario: Manual launch gate passes with the synth disabled

- GIVEN the built standalone app with the internal synth toggled off
- WHEN it is launched manually
- THEN it opens its audio device and runs the transport continuously, silently, with no dropouts, asserts, or crashes, until manually closed
