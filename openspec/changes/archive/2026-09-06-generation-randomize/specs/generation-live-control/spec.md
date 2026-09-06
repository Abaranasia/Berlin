# Generation Live Control Specification

## Purpose

Runtime generation control for the live app: the Generate/Randomize/Lock-Seed UI actions, seed entry, and the audio-thread-safe contract for replacing the playing `Sequence` without the audio thread ever observing a torn or partially-built `Sequence` (the underlying handoff mechanism is defined jointly with `realtime-audio-wiring`'s modified requirement).

A brief audible discontinuity at the moment of a user-triggered regeneration is acceptable and out of scope to eliminate; this capability does not require sample-accurate or glitch-free swapping. The no-torn-read guarantee (never observing incomplete data) is a distinct, separately-guaranteed concern from perceived audio continuity.

## Requirements

### Requirement: Generate Rebuilds the Sequence From the Current Seed and Restarts Playback

Pressing Generate MUST rebuild a `Sequence` by composing `SkipMaskGenerator` and `PitchGenerator` against a `DeterministicRandom` constructed from the current seed, publish it through the handoff, and restart the transport from step 1 — not a mid-loop swap that keeps the previous playhead running.

#### Scenario: Generate with an unchanged seed reproduces an identical Sequence

- GIVEN the current seed is unchanged since the last Generate
- WHEN Generate is pressed again
- THEN the resulting `Sequence` is byte-identical to the previous one

#### Scenario: Generate mid-playback restarts from step 1

- GIVEN audio is playing partway through the current `Sequence`
- WHEN Generate is pressed
- THEN playback restarts from step 1 of the newly adopted `Sequence`, not from the prior playhead position

### Requirement: Randomize/New Seed Draws a Fresh Seed, Then Generates

Randomize/New Seed MUST draw a fresh seed value from outside `DeterministicRandom`, update the seed field to display it, then perform the same rebuild-and-restart behavior as Generate.

#### Scenario: Randomize updates the visible seed and produces a new Sequence

- GIVEN an existing seed and `Sequence`
- WHEN Randomize/New Seed is pressed
- THEN the seed field shows a new value, and the resulting `Sequence` differs from the prior one

### Requirement: Lock Seed Suppresses Reseeding

While Lock Seed is enabled, Randomize/New Seed MUST NOT draw or apply a new seed value. Generate remains available and continues to reproduce the same `Sequence` for the same (locked) seed.

#### Scenario: Randomize is suppressed while Lock Seed is enabled

- GIVEN Lock Seed is enabled
- WHEN Randomize/New Seed is pressed
- THEN the seed value does not change, and no new `Sequence` is generated from a different seed

### Requirement: Seed Field Is Directly Editable

The seed control MUST be an editable text field, not a display-only label. A user MUST be able to type or paste a seed value and press Generate to produce that seed's `Sequence`.

#### Scenario: Typed seed reproduces its deterministic Sequence

- GIVEN a user types a specific seed value into the seed field
- WHEN Generate is pressed
- THEN the resulting `Sequence` matches generating directly from that seed value

### Requirement: Audio-Thread-Safe Regeneration Handoff

Regeneration MUST use a message-thread-to-audio-thread publish/adopt handoff: the message thread builds a complete `Sequence` and publishes it into a staging slot (`SequencePlayer::publishSequence`); the audio thread, at the top of its own `process()`, adopts it only as a whole — emitting a note-off for any currently-sounding note first (the same note-off contract `flushPendingNoteOff` provides, applied inline as part of adoption rather than via a second call to that method), swapping the sequence in without allocation (`Sequence::swap`), and resetting its playhead to step 1. Adoption on the audio thread MUST NOT allocate, MUST NOT use a blocking lock, and MUST NOT introduce logging.

#### Scenario: Regeneration while audio is running causes no hung note, dropout, assert, or crash

- GIVEN audio is running and a note may be sounding
- WHEN Generate or Randomize is triggered
- THEN the previously sounding note (if any) receives a note-off, the new `Sequence` is adopted, and no hung note, dropout, assert, or crash occurs

#### Scenario: Audio thread never observes a partially-built or torn Sequence during adoption

- GIVEN the message thread is mid-build of a new `Sequence`
- WHEN the audio thread is simultaneously rendering
- THEN the audio thread reads either the complete old `Sequence` or the complete new one, never a torn mixture (the audio thread only ever reads `sequence`, which it alone replaces via `swap`; the message thread never touches `sequence` directly, only the `pendingSequence` staging slot behind the `sequencePending` atomic flag)
