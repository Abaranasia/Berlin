# Delta for Generation Live Control

## ADDED Requirements

### Requirement: Scale, Root, and Range Controls Apply On Generate

The UI MUST expose three additional controls: a scale selector (the 6-entry catalog: Minor, Major, Dorian, Phrygian, Mixolydian, Harmonic Minor), a root selector (12 pitch classes), and low/high range controls (MIDI note bounds). Changing any of these controls MUST NOT affect the currently playing `Sequence` until Generate (or Randomize/New Seed, or a preset load) is pressed — the same staged-apply-on-Generate pattern already used for Pulses/Rotation in Euclidean mode. Defaults MUST be Minor, root C, range `[36, 72]`, reproducing pre-change behavior exactly when left untouched.

#### Scenario: Changing scale/root/range before Generate does not affect the live Sequence

- GIVEN a Sequence is currently playing
- WHEN the user changes the scale selector, root selector, or a range bound without pressing Generate
- THEN the currently playing `Sequence` is unaffected

#### Scenario: Generate applies the staged scale/root/range values

- GIVEN the user has changed the scale to Dorian, root to D, and range to `[40, 64]` without yet pressing Generate
- WHEN Generate is pressed
- THEN the rebuilt `Sequence`'s notes are drawn from D Dorian within `[40, 64]`

#### Scenario: Untouched defaults reproduce pre-change output

- GIVEN the scale, root, and range controls are left at their defaults (Minor, C, `[36, 72]`)
- WHEN Generate is pressed with an unchanged seed
- THEN the resulting `Sequence` is byte-identical to the pre-change hardcoded-scale output

### Requirement: Randomize/New Seed Does Not Alter Staged Scale, Root, or Range

Randomize/New Seed MUST draw a fresh seed and regenerate, but MUST NOT change the currently staged `scaleType`, `rootPitchClass`, `rangeLow`, or `rangeHigh` values — only the seed (and therefore pitch draws within the same scale/range) changes, analogous to how Randomize holds `pulses`/`rotation` fixed in Euclidean mode.

#### Scenario: Randomize keeps scale/root/range fixed while pitch values vary

- GIVEN scale = Harmonic Minor, root = A, range = `[45, 69]`
- WHEN Randomize/New Seed is pressed
- THEN a new seed is drawn, scale/root/range remain Harmonic Minor/A/`[45, 69]`, and the resulting notes differ from before but still satisfy scale membership and range

## MODIFIED Requirements

### Requirement: Generate Rebuilds the Sequence From the Current Seed and Restarts Playback

Pressing Generate MUST rebuild a `Sequence` by composing the currently selected rhythm source (`SkipMaskGenerator` in Random mode, `EuclideanRhythmGenerator` in Euclidean mode) and `PitchGenerator` against a `DeterministicRandom` constructed from the current seed, publish it through the handoff, and restart the transport from step 1 — not a mid-loop swap that keeps the previous playhead running. `PitchGenerator`'s scale, root, and range MUST come from the current (staged-then-applied) `GenerationParams` values (`scaleType`, `rootPitchClass`, `rangeLow`, `rangeHigh`), not a hardcoded scale.
(Previously: rhythm source was always `SkipMaskGenerator`, no mode branch existed; `PitchGenerator` was always constructed from a hardcoded `Scale::minor(48)` and range `[36, 72]`.)

#### Scenario: Generate with an unchanged seed and mode reproduces an identical Sequence

- GIVEN the current seed, rhythm mode, and scale/root/range are unchanged since the last Generate
- WHEN Generate is pressed again
- THEN the resulting `Sequence` is byte-identical to the previous one

#### Scenario: Generate mid-playback restarts from step 1

- GIVEN audio is playing partway through the current `Sequence`
- WHEN Generate is pressed
- THEN playback restarts from step 1 of the newly adopted `Sequence`, not from the prior playhead position
