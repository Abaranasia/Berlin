# Generation Live Control Specification

## Purpose

Runtime generation control for the live app: the Generate/Randomize/Lock-Seed UI actions, seed entry, and the audio-thread-safe contract for replacing the playing `Sequence` without the audio thread ever observing a torn or partially-built `Sequence` (the underlying handoff mechanism is defined jointly with `realtime-audio-wiring`'s modified requirement).

A brief audible discontinuity at the moment of a user-triggered regeneration is acceptable and out of scope to eliminate; this capability does not require sample-accurate or glitch-free swapping. The no-torn-read guarantee (never observing incomplete data) is a distinct, separately-guaranteed concern from perceived audio continuity.

## Requirements

### Requirement: Generate Rebuilds the Sequence From the Current Seed and Restarts Playback

Pressing Generate MUST rebuild a `Sequence` by composing the currently selected rhythm source (`SkipMaskGenerator` in Random mode, `EuclideanRhythmGenerator` in Euclidean mode) and `PitchGenerator` against a `DeterministicRandom` constructed from the current seed, publish it through the handoff, and restart the transport from step 1 — not a mid-loop swap that keeps the previous playhead running.
(Previously: rhythm source was always `SkipMaskGenerator`, no mode branch existed.)

#### Scenario: Generate with an unchanged seed and mode reproduces an identical Sequence

- GIVEN the current seed and rhythm mode (and mode parameters) are unchanged since the last Generate
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

While Lock Seed is enabled, Randomize/New Seed MUST NOT draw or apply a new seed value. Generate remains available and continues to reproduce the same `Sequence` for the same (locked) seed. Loading a preset is a distinct, explicit user action and MUST NOT be suppressed by Lock Seed — the preset's saved seed is applied and a new `Sequence` generated regardless of Lock Seed's state (see `preset-persistence`'s "Loading Applies The Saved Seed Even When Lock Seed Is Enabled").

#### Scenario: Randomize is suppressed while Lock Seed is enabled

- GIVEN Lock Seed is enabled
- WHEN Randomize/New Seed is pressed
- THEN the seed value does not change, and no new `Sequence` is generated from a different seed

#### Scenario: Loading a preset is not suppressed by Lock Seed

- GIVEN Lock Seed is enabled
- WHEN the user loads a preset with a different saved seed
- THEN the seed value changes to the preset's saved seed, and a new `Sequence` is generated from it

### Requirement: Rhythm-Mode Selection Controls

The UI MUST expose one row of controls for rhythm mode: a mode selector (Random | Euclidean), a Pulses control, and a Rotation control. Pulses and Rotation MUST be inert (not affect generation) while Random mode is selected. Switching to Euclidean mode for the first time MUST default `pulses` to 5, `rotation` to 0.

#### Scenario: Switching to Euclidean mode uses the default pulses value

- GIVEN the mode has never been switched to Euclidean before
- WHEN the user selects Euclidean mode
- THEN Pulses defaults to 5 and Rotation defaults to 0

#### Scenario: Pulses/Rotation controls are inert in Random mode

- GIVEN Random mode is selected
- WHEN the user changes Pulses or Rotation
- THEN the generated Sequence is unaffected until the mode is switched to Euclidean

### Requirement: Randomize Rerolls Pitch Only in Euclidean Mode

In Euclidean mode, Randomize/New Seed MUST draw a fresh seed and regenerate pitches, but MUST NOT alter the current `pulses` or `rotation` values — the rhythm pattern stays fixed while pitch varies. This differs from Random mode, where a new seed changes both rhythm and pitch.
(Previously: Randomize/New Seed drew a fresh seed and regenerated both rhythm and pitch uniformly, with no mode distinction.)

#### Scenario: Randomize in Euclidean mode holds rhythm, varies pitch

- GIVEN Euclidean mode is active with `pulses=5, rotation=2`
- WHEN Randomize/New Seed is pressed
- THEN a new seed is drawn, the active-step pattern (still `pulses=5, rotation=2`) is unchanged, and the resulting notes differ from before

#### Scenario: Randomize in Random mode still varies rhythm and pitch

- GIVEN Random mode is active
- WHEN Randomize/New Seed is pressed
- THEN a new seed is drawn and both the active-step pattern and the notes may differ from before

### Requirement: Rhythm Mode Is Not Persisted This Slice — Live UI State Wins on Preset Load

Loading a preset MUST NOT change the current rhythm mode, `pulses`, or `rotation` — these remain at whatever the UI currently holds, and that live state (not any prior-saved rhythm, since `Preset` has no rhythm field to begin with) is what gets applied together with the preset's seed when the preset load rebuilds the Sequence. This is a known, explicit gap (Preset schema/version change is out of scope for this slice), not a defect: the user's currently-selected rhythm mode/parameters persist across the load exactly as if Generate had been pressed with the preset's seed.

#### Scenario: Preset load leaves rhythm mode untouched and applies it with the preset's seed

- GIVEN Euclidean mode is active with `pulses=7, rotation=3`
- WHEN the user loads a preset saved while Random mode was active
- THEN rhythm mode remains Euclidean with `pulses=7, rotation=3` after load, and the resulting Sequence is built from the preset's seed combined with THIS live Euclidean configuration — not any rhythm the preset was originally saved with

### Requirement: pulses=0 Silently Disables Mutate and Auto-Evolve (Known Limitation)

Because `MutationEngine`'s transforms that require at least one active step (e.g. `addNote`) early-return unchanged on an all-inactive `Sequence`, setting Euclidean `pulses` to 0 produces an all-inactive Sequence on which manual Mutate clicks and Auto-Evolve's automatic triggers become permanent no-ops (no crash, no error, no status feedback) until `pulses` is raised above 0 again. This is an accepted known limitation for this slice, not a defect to fix here — future slices MAY add UI feedback or disable Mutate/Auto-Evolve controls when the active-step count is 0.

#### Scenario: pulses=0 makes Mutate a silent no-op

- GIVEN Euclidean mode with `pulses=0` (an all-inactive Sequence)
- WHEN the user clicks Mutate
- THEN the Sequence remains all-inactive and unchanged, with no error or status message distinguishing this from a normal successful mutation

### Requirement: Loading A Preset Applies Its Saved Seed, Then Generates

Loading a preset MUST set the current seed to the preset's saved seed value, update the seed field to display it, then perform the same rebuild-and-restart behavior as Generate — including, if audio is playing, restarting from step 1 via the existing audio-thread-safe regeneration handoff.

#### Scenario: Preset load updates the visible seed and produces its Sequence

- GIVEN a preset saved with a specific seed
- WHEN the user loads that preset
- THEN the seed field shows the preset's saved seed, and the resulting `Sequence` matches generating directly from that seed value

#### Scenario: Preset load mid-playback restarts from step 1

- GIVEN audio is playing partway through the current `Sequence`
- WHEN the user loads a preset
- THEN playback restarts from step 1 of the `Sequence` generated from the preset's seed, not from the prior playhead position

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
