# Delta for Generation Live Control

## MODIFIED Requirements

### Requirement: Lock Seed Suppresses Reseeding

While Lock Seed is enabled, Randomize/New Seed MUST NOT draw or apply a new seed value. Generate remains available and continues to reproduce the same `Sequence` for the same (locked) seed. Loading a preset is a distinct, explicit user action and MUST NOT be suppressed by Lock Seed — the preset's saved seed is applied and a new `Sequence` generated regardless of Lock Seed's state (see `preset-persistence`'s "Loading Applies The Saved Seed Even When Lock Seed Is Enabled").

(Previously: only described Randomize/New Seed being suppressed by Lock Seed; did not address preset loading, a new seed-mutating action introduced by the preset system.)

#### Scenario: Randomize is suppressed while Lock Seed is enabled

- GIVEN Lock Seed is enabled
- WHEN Randomize/New Seed is pressed
- THEN the seed value does not change, and no new `Sequence` is generated from a different seed

#### Scenario: Loading a preset is not suppressed by Lock Seed

- GIVEN Lock Seed is enabled
- WHEN the user loads a preset with a different saved seed
- THEN the seed value changes to the preset's saved seed, and a new `Sequence` is generated from it

## ADDED Requirements

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
