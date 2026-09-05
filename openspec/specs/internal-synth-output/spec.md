# Internal Synth Output Specification

## Purpose

Wiring the single monophonic voice (`internal-synth-voice`) into the real audio callback: consuming the same `StepEventBuffer` stream MIDI dispatch already reads, mixing the rendered signal (plus optional delay/reverb) into the audio output buffer, an enable/disable toggle independent of MIDI-out, and correct silence on stop/restart. Additive alongside existing MIDI dispatch; `Source/midi/*` and `Source/export/*` behavior is unaffected.

## Requirements

### Requirement: StepEventBuffer Consumption Alongside MIDI Dispatch

The system MUST consume the same per-block `StepEventBuffer` stream (`sampleOffset`, `stepIndex`, `note`, `isNoteOn`) that MIDI dispatch already consumes, as an additive parallel path inside the audio callback. This MUST NOT alter the events delivered to, or the behavior of, the existing MIDI dispatch path.

#### Scenario: Same events reach both the synth and MIDI dispatch
- GIVEN a block containing note-on and note-off `StepEvent`s
- WHEN the block is processed
- THEN both the synth path and the MIDI dispatch path observe the identical set of events with identical field values

#### Scenario: MIDI dispatch is unaffected by synth consumption
- GIVEN the synth reads the `StepEventBuffer`
- WHEN MIDI dispatch also reads it in the same block
- THEN MIDI dispatch's messages, timestamps, and behavior are identical to Phase 7 behavior

### Requirement: Sample-Offset-Accurate Note Rendering

The system MUST render note-on and note-off transitions at their `StepEvent::sampleOffset` within the block, not merely at the block boundary. When multiple events share a sample offset, note-off MUST be applied before note-on at that offset, consistent with the ordering MIDI dispatch already relies on.

#### Scenario: A note-on mid-block starts sounding only from its offset
- GIVEN a note-on `StepEvent` at a non-zero sample offset within a block
- WHEN the block is rendered
- THEN samples before the offset reflect the prior state and samples from the offset onward reflect the new note

#### Scenario: Shared-offset note-off and note-on apply in order
- GIVEN a note-off and a note-on sharing the same sample offset
- WHEN the block is rendered
- THEN the note-off is applied before the note-on at that offset

### Requirement: Synth Enable/Disable Toggle

The system MUST provide a user-facing toggle that enables or disables the internal synth's audio contribution, independent of the external MIDI-out path and independent of the delay/reverb bypass state. When disabled, the synth MUST contribute no audible signal to the output buffer. Toggling MUST NOT affect transport, step scheduling, MIDI dispatch, or MIDI export. The toggle MUST default to enabled.

#### Scenario: Disabling the toggle silences only the synth
- GIVEN the internal synth is enabled and sounding
- WHEN the user disables the toggle
- THEN the audio output buffer stops carrying synth signal, while MIDI dispatch continues unaffected

#### Scenario: Default launch produces audible synth output
- GIVEN a freshly launched application with no user interaction
- WHEN the seeded sequence plays
- THEN the internal synth is enabled by default and produces audible sound with no MIDI device connected

### Requirement: Terminal Delay And Reverb, Bypassed By Default

The system MUST apply delay and reverb as a terminal per-block effects pass over the rendered voice signal. Both MUST be bypassed (dry) by default; the user MUST be able to enable each independently. Disabling either MUST stop introducing new processed tail without producing an audible glitch.

#### Scenario: Default output is dry
- GIVEN a freshly launched application with no user interaction
- WHEN a note sounds
- THEN the output contains no delay or reverb tail

#### Scenario: Enabling then disabling stops new tail without artefacts
- GIVEN delay and/or reverb has been enabled and a tail is audible
- WHEN the user disables it
- THEN no new processed tail is introduced afterward, and no click/glitch is audible at the transition

### Requirement: No Stuck Notes On Stop Or Device Restart

The system MUST hard-silence the voice and clear any effect tail when audio processing stops or the audio device restarts, so no drone or hanging note continues.

#### Scenario: Stopping playback silences the synth
- GIVEN a note is sounding when audio processing stops
- WHEN processing stops
- THEN no further sound is produced, including any delay/reverb tail

#### Scenario: Device restart does not resume a stuck note
- GIVEN a note was sounding before an audio device restart
- WHEN the device restarts and audio processing resumes
- THEN no stuck or hanging note is audible from before the restart

### Requirement: Mixing Without Clipping

The system MUST mix the synth's rendered signal (including any enabled effects) into the audio output buffer with sufficient headroom that a sustained, single-voice note at full level does not clip the output.

#### Scenario: Sustained full-level note does not clip
- GIVEN the voice sounds a note at full envelope level for a sustained duration, with effects enabled
- WHEN the output is inspected
- THEN no sample exceeds the valid output range

### Requirement: Allocation-Free Synth Contribution To The Callback

The synth-related code invoked from `getNextAudioBlock` MUST NOT allocate heap memory, acquire a lock, or call a logging/formatting function, consistent with the audio callback's existing constitution rule.

#### Scenario: Synth contribution introduces no allocation, lock, or logging call
- GIVEN the synth path added to `getNextAudioBlock`
- WHEN its code is inspected during code review
- THEN no heap allocation, no lock acquisition, and no logging/formatting call is present anywhere in its call path
