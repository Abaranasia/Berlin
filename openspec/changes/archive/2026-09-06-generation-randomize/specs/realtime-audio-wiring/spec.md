# Delta for Realtime Audio Wiring

## MODIFIED Requirements

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
- THEN the audio thread observes either the complete previous `Sequence` or the complete new one, never partially-constructed step data from both [mechanism pinned by design]

#### Scenario: No torn read during concurrent build and playback

- GIVEN the message thread is constructing a new `Sequence` while the audio thread renders the previous one
- WHEN the audio thread reads sequence data for the current block
- THEN it reads only fields belonging to one complete, previously-published `Sequence`, never a torn combination [mechanism pinned by design]
