# Delta for Realtime Audio Wiring

## MODIFIED Requirements

### Requirement: Sequence Built Before Audio Starts, Replaceable Only Via Published Handoff

The system MUST generate/construct the initial `Sequence` on the message thread before the host or OS ever invokes the audio callback — `setAudioChannels`/`getNextAudioBlock` on the standalone path, `prepareToPlay`/`processBlock` on the plugin path — so the audio thread only ever reads a complete, fully-built `Sequence`. After construction, the live `Sequence` MAY be replaced at runtime by a subsequent, fully-built `Sequence`, but ONLY through the message-thread-to-audio-thread publish/adopt handoff defined by `generation-live-control`. The audio thread MUST NOT observe a partially-built, torn, or half-written `Sequence` at any point during a replacement, on either path.

(Previously: scoped to `MainComponent`/`setAudioChannels` only; now covers the shared processor callback on the plugin path too.)

#### Scenario: Sequence exists and is immutable before audio starts (either path)

- GIVEN the owning component's (`MainComponent` or `BerlinAudioProcessor`) constructor has run
- WHEN `setAudioChannels` (standalone) or the host's first `prepareToPlay` (plugin) is invoked
- THEN a complete, fully-built `Sequence` already exists

#### Scenario: Initial sequence is never replaced without an explicit handoff

- GIVEN audio is running with the initially constructed `Sequence`
- WHEN no Generate/Randomize action has occurred
- THEN the audio thread continues reading that same `Sequence` unchanged indefinitely

#### Scenario: Replacement is always a complete Sequence, never a mix

- GIVEN a Generate or Randomize action has built a new `Sequence` on the message thread
- WHEN the audio thread adopts it via the published handoff
- THEN the audio thread observes either the complete previous `Sequence` or the complete new one, never partially-constructed step data from both

#### Scenario: No torn read during concurrent build and playback

- GIVEN the message thread is constructing a new `Sequence` while the audio thread renders the previous one
- WHEN the audio thread reads sequence data for the current block
- THEN it reads only fields belonging to one complete, previously-published `Sequence`, never a torn combination

### Requirement: Timing Derived Once Per Prepare Call, Regardless Of Host

The system MUST derive `Transport`'s samples-per-step from the `sampleRate` supplied by the host/OS's prepare callback — `prepareToPlay(samplesPerBlockExpected, sampleRate)` on the standalone `AudioAppComponent` path, `prepareToPlay(sampleRate, samplesPerBlock)` on the plugin `AudioProcessor` path — not compute or recompute it inside `getNextAudioBlock`/`processBlock`.

(Previously: named only the `AudioAppComponent` signature; now covers both prepare signatures and their argument-order difference.)

#### Scenario: prepareToPlay configures the transport on either path

- GIVEN the host/OS calls the platform's `prepareToPlay` with a given `sampleRate`
- WHEN the owning component handles this callback
- THEN `Transport::prepare(sampleRate)` is invoked and its resulting `samplesPerStep` reflects that `sampleRate`, regardless of argument order

#### Scenario: Repeated prepareToPlay updates timing consistently

- GIVEN `prepareToPlay` is called again with a different `sampleRate` (e.g. device change or host re-prepare)
- WHEN the transport is re-prepared
- THEN `samplesPerStep` is recomputed from the new `sampleRate` and no stale value from the previous rate is used

### Requirement: Allocation-Free, Lock-Free, Log-Free Audio Callback

The system MUST NOT allocate heap memory or call any logging/`juce::String`-formatting function inside the audio callback (`getNextAudioBlock` standalone, `processBlock` plugin), for any block size. On the standalone path only, the callback MAY acquire the one named, documented lock exception below; the plugin path MUST NOT acquire any lock at all, because it has no `MidiOutputSink`/`sendBlockOfMessages` call in its path.

**Named exception (standalone path only)**: `juce::MidiOutput::sendBlockOfMessages`, invoked from the MIDI dispatch step, takes a short, bounded, internal lock to append to its own pending-message list. No I/O, no unbounded-size allocation, and no driver syscall occurs on the calling (audio) thread as part of this lock.

(Previously: applied only to `getNextAudioBlock`; now scopes the lock exception to the standalone path and requires zero locks on `processBlock`.)

#### Scenario: Callback body contains no allocation or logging call

- GIVEN the implementation of `getNextAudioBlock` or `processBlock`
- WHEN its body is inspected during code review
- THEN no heap allocation and no `juce::String`/`Logger` call is present anywhere in the call path executed per block

#### Scenario: Standalone path has exactly one documented lock

- GIVEN `getNextAudioBlock`'s full call path
- WHEN it is inspected during code review
- THEN the only lock acquisition found is the documented `sendBlockOfMessages` internal lock

#### Scenario: Plugin path has zero locks

- GIVEN `processBlock`'s full call path
- WHEN it is inspected during code review
- THEN no lock acquisition of any kind is present, because the plugin path has no `MidiOutputSink`

### Requirement: Audio Output Governed By Synth Enable State

The system MUST produce audible audio output in the audio callback (`getNextAudioBlock` standalone, `processBlock` plugin) when the internal synth is enabled and a note is sounding: the internal synth renders into the audio output buffer. When the internal synth is disabled, or no note is sounding, the audio output buffer MUST contain only silence — no oscillator, filter, envelope, or voice outside the internal synth path may introduce sound. This requirement governs the audio output path only; MIDI output is a distinct dispatch path (OS device on the standalone path, host `MidiBuffer` on the plugin path).

(Previously: named only `getNextAudioBlock`; now covers `processBlock` and names the plugin's distinct MIDI path.)

#### Scenario: Audio buffer carries synth output when enabled and a note sounds

- GIVEN the internal synth is enabled and a `StepEvent` note-on has been dispatched
- WHEN the block is rendered on either path
- THEN the audio output buffer contains the synth's rendered signal, not silence

#### Scenario: Audio buffer stays silent when the synth is disabled

- GIVEN the internal synth enable toggle is off
- WHEN a block is processed, regardless of any `StepEvent` note-on
- THEN the audio output buffer remains at its cleared (silent) value

#### Scenario: Audio buffer stays silent when no note is sounding

- GIVEN the internal synth is enabled but no note-on has occurred since the last full envelope release
- WHEN a block is rendered
- THEN the audio output buffer remains silent

#### Scenario: MIDI output does not violate audio silence when the synth is disabled

- GIVEN a block in which MIDI events are dispatched (device or host buffer) and the internal synth is disabled
- WHEN the audio output buffer contents are inspected
- THEN the audio buffer remains cleared, because MIDI dispatch is a distinct output path from the audio buffer

### Requirement: Single Atomic Playhead Observability Seam

The system MUST expose exactly one `std::atomic<int>` playhead, owned by whichever component holds `Transport` (`MainComponent` standalone, `BerlinAudioProcessor` plugin), written by the audio thread to reflect the current step position, safely readable from another thread without locks.

(Previously: implicitly owned by `MainComponent` only; now names the plugin's owner too.)

#### Scenario: Playhead advances with the transport on either path

- GIVEN audio is running and steps are advancing
- WHEN the atomic playhead is read from a non-audio thread after several blocks
- THEN its value reflects a current or very recently current step position, obtained without any lock
