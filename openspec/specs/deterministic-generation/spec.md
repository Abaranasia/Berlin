# Deterministic Generation Specification

## Purpose

Seeded, reproducible generation of pitch and rhythm content from the `sequencing-core` value types. Reproducibility (same seed -> identical output) is the central contract this capability exists to guarantee, valid within a given build (same JUCE version, same binary).

## Requirements

### Requirement: DeterministicRandom Explicit-Seed Wrapper

The system MUST provide a `DeterministicRandom` type wrapping `juce::Random(int64 seed)`, constructible ONLY with an explicit seed (no default-seeded or time-seeded constructor). Its public surface MUST be limited to `nextInt`, `nextFloat`, and `getSeed`.

#### Scenario: Explicit seed construction

- GIVEN an int64 seed value
- WHEN a `DeterministicRandom` is constructed with it
- THEN `getSeed()` returns that same seed

#### Scenario: No implicit-seed construction

- GIVEN the `DeterministicRandom` type definition
- WHEN inspecting its constructors
- THEN no default constructor or time-seeded constructor is available

#### Scenario: Same seed produces same sequence of draws

- GIVEN two `DeterministicRandom` instances constructed with the same seed
- WHEN the same sequence of `nextInt`/`nextFloat` calls is made on each
- THEN both instances return identical values at every step

### Requirement: Seed-In, Identical-Out Reproducibility Contract

Every generator in this capability MUST produce byte-identical output given the same seed and the same input parameters, within a single build. For the live app's composed Generate/Randomize path (which now has a selectable rhythm mode), this contract is scoped to: same seed + same rhythm mode + same mode parameters (`kActiveSteps`, or `pulses`/`rotation`) -> identical resulting Sequence. It does NOT guarantee identical output across different modes for the same seed (see the divergence scenario above).
(Previously: stated as "same seed and same input parameters -> identical output" with no mode dimension, since only one rhythm source existed.)

#### Scenario: Same seed, identical Sequence output

- GIVEN a fixed `numSteps`, a fixed `density`, and a fixed seed
- WHEN `RhythmGenerator::generate` is called twice with fresh `DeterministicRandom` instances constructed from that same seed
- THEN both resulting `Sequence` objects have identical `active` flags at every index

#### Scenario: Same seed, same mode, same params — reproducible end-to-end

- GIVEN a fixed seed, a fixed rhythm mode, and fixed mode parameters
- WHEN the composed Sequence (rhythm source + `PitchGenerator`) is built twice from that same seed
- THEN both resulting Sequences are byte-identical (same active flags, same notes)

### Requirement: PitchGenerator Scale-Based Note Generation

The system MUST provide a `PitchGenerator` configured with `{Scale, rangeLow, rangeHigh}` that generates the next note via `generateNextNote(DeterministicRandom&)`, returning only notes within `[rangeLow, rangeHigh]` that belong to the configured `Scale`.

#### Scenario: Generated note is in scale and in range

- GIVEN a `PitchGenerator` configured with a `Scale` and a range containing at least one in-scale note
- WHEN `generateNextNote` is called
- THEN the returned note is within `[rangeLow, rangeHigh]`
- AND the returned note is a member of the configured `Scale`

#### Scenario: Range with no in-scale note clamps to nearest

- GIVEN a `PitchGenerator` configured with a range `[rangeLow, rangeHigh]` that contains no note belonging to the configured `Scale`
- WHEN `generateNextNote` is called
- THEN the returned note is the nearest in-scale note to the configured range (outside the range if necessary)
- AND no exception is thrown and no out-of-scale note is ever returned

### Requirement: RhythmGenerator Density as Per-Step Probability

The system MUST provide a `RhythmGenerator` configured with `{numSteps, density}` that produces a `Sequence` via `generate(DeterministicRandom&)`, populating only the `active` field of each step. Each step's `active` flag MUST be determined by an independent probability equal to `density`, NOT by a guaranteed/exact active-step count.

#### Scenario: Output size matches numSteps

- GIVEN `numSteps = 16` and any valid `density`
- WHEN `RhythmGenerator::generate` is called
- THEN the resulting `Sequence` has exactly 16 steps

#### Scenario: Density approximates average active-step count, not an exact count

- GIVEN `numSteps = 16` and `density = 0.5`
- WHEN `RhythmGenerator::generate` is run across many distinct seeds
- THEN the mean active-step count across runs approximates 8 (within statistical tolerance)
- AND individual runs are permitted to report differing exact active-step counts (e.g. 7, 8, or 9)

#### Scenario: RhythmGenerator only touches active

- GIVEN a `RhythmGenerator` configured with any valid `numSteps` and `density`
- WHEN `generate` produces a `Sequence`
- THEN every step's `note` field is left at its default/unset value
- AND only `active` is populated by the generator

### Requirement: Production Composition of a Rhythm Source and PitchGenerator

The system MUST compose a rhythm-producing generator with `PitchGenerator` together in production code, driven by a single `DeterministicRandom` per generation, and this composition MUST be callable at runtime (not only during construction). For the live app's Generate/Randomize path, the rhythm source is selectable per a rhythm-mode switch: `SkipMaskGenerator` (Random mode) or `EuclideanRhythmGenerator` (Euclidean mode, this change) — `RhythmGenerator` remains a separate, fully valid, independently-tested capability with no production call site of its own. Because `EuclideanRhythmGenerator::generate()` takes no `DeterministicRandom`, it consumes zero RNG draws before the `PitchGenerator` pass runs. `SkipMaskGenerator`, by contrast, consumes exactly `kNumSteps - kActiveSteps` draws (one per skipped step, per its existing "Deterministic Displacement Pattern" requirement) — with the current production constants (`kNumSteps=16`, `kActiveSteps=11`), that is exactly 5 draws, a fixed constant that does NOT vary with the Euclidean-mode `pulses` parameter (the two are unrelated values). The same seed therefore produces different pitches depending on which rhythm mode is active, because `PitchGenerator` begins drawing from a different `DeterministicRandom` stream position in each mode (index 5 in Random mode, index 0 in Euclidean mode).
(Previously: composition used only `SkipMaskGenerator`; no mode switch, no draw-count divergence existed.)

#### Scenario: Production build composes both generators at runtime

- GIVEN a running app with an existing `Sequence`
- WHEN a Generate or Randomize action is triggered
- THEN the currently selected rhythm source (`SkipMaskGenerator` in Random mode, `EuclideanRhythmGenerator` in Euclidean mode) and `PitchGenerator` are recomposed against a fresh `DeterministicRandom`, not only during `MainComponent` construction

#### Scenario: Same seed diverges in pitch across modes — expected, not a bug

- GIVEN the same seed value, Random mode active (draws exactly 5 RNG values — `kNumSteps - kActiveSteps` — before the pitch pass)
- AND the same seed value, Euclidean mode active with any `(pulses, rotation)` (draws 0 RNG values before the pitch pass)
- WHEN a Sequence is generated in each mode
- THEN the two resulting Sequences' pitches differ (the pitch pass starts from different `DeterministicRandom` stream positions), and this divergence is expected, documented behavior, not a defect

### Requirement: Seed Is User-Editable, Not Display-Only

The system MUST expose the active seed via an editable text field. Typing or pasting a valid seed value and triggering Generate MUST reproduce that exact seed's `Sequence`, per the existing Seed-In, Identical-Out Reproducibility Contract.

#### Scenario: Typed seed reproduces its deterministic Sequence

- GIVEN a user types a specific seed value into the seed field
- WHEN Generate is pressed
- THEN the resulting `Sequence` is identical to generating with that seed value directly

#### Scenario: Re-entering the same seed reproduces the same Sequence

- GIVEN a seed value was used to generate a `Sequence`
- WHEN the same seed value is entered again and Generate is pressed
- THEN the resulting `Sequence` is byte-identical to the first

### Requirement: Randomize/New Seed Draws a Fresh External Seed

Randomize/New Seed MUST draw a fresh seed value from a source external to `DeterministicRandom` (e.g. system RNG), then generate using that seed. `DeterministicRandom` MUST remain explicit-seed-only; it gains no default or time-seeded constructor.

#### Scenario: Randomize yields a different seed and Sequence

- GIVEN an existing seed and `Sequence`
- WHEN Randomize/New Seed is triggered
- THEN a different seed value is drawn and a correspondingly different `Sequence` is generated

### Requirement: Lock Seed Suppresses Reseeding

While Lock Seed is enabled, the system MUST NOT draw a new seed value in response to Randomize/New Seed actions, so repeated Generate presses remain reproducible (same seed -> identical `Sequence`).

#### Scenario: Repeated Generate under Lock Seed stays reproducible

- GIVEN Lock Seed is enabled
- WHEN Generate is pressed multiple times
- THEN every resulting `Sequence` is byte-identical

#### Scenario: Randomize does not change the seed while locked

- GIVEN Lock Seed is enabled
- WHEN Randomize/New Seed is triggered
- THEN the seed value is unchanged

### Requirement: SkipMaskGenerator Produces a Deterministic Displacement Pattern

The system MUST provide a `SkipMaskGenerator` configured with `{numSteps, activeSteps}` that produces a `Sequence` via `generate(DeterministicRandom&)` by starting from a fully-active step pattern and deterministically skipping exactly `numSteps - activeSteps` steps (step 0 is never skipped, anchoring the pattern's phase). This is a distinct capability from `RhythmGenerator`: `RhythmGenerator`'s existing "Density as Per-Step Probability" requirement (in this same spec, unaffected by this addition) is unchanged and remains fully valid — `SkipMaskGenerator` is additive, not a replacement of that contract, even though the live app's Generate/Randomize path uses `SkipMaskGenerator` rather than `RhythmGenerator`.

#### Scenario: Active-step count is exact, not probabilistic

- GIVEN `numSteps` and `activeSteps` (clamped to `[1, numSteps]`)
- WHEN `SkipMaskGenerator::generate` is called
- THEN the resulting `Sequence` has exactly `numSteps` steps and exactly `activeSteps` of them are active — never a differing count across runs with the same parameters

#### Scenario: Step 0 is never skipped

- GIVEN any valid `numSteps` and `activeSteps < numSteps`
- WHEN `SkipMaskGenerator::generate` is called
- THEN step 0's `active` flag is always true

#### Scenario: Same seed produces an identical mask

- GIVEN a fixed `numSteps` and `activeSteps`
- WHEN `SkipMaskGenerator::generate` is called twice with fresh `DeterministicRandom` instances constructed from the same seed
- THEN both resulting `Sequence` objects have identical `active` flags at every index

#### Scenario: Only active is populated

- GIVEN a `SkipMaskGenerator` configured with any valid `numSteps` and `activeSteps`
- WHEN `generate` produces a `Sequence`
- THEN every step's `note` field is left at its default/unset value, and only `active` is populated

#### Scenario: activeSteps clamped at both ends

- GIVEN `activeSteps` of 0 (below valid range)
- WHEN `SkipMaskGenerator::generate` is called
- THEN the clamped value of 1 is used, producing exactly 1 active step

- GIVEN `activeSteps` greater than `numSteps`
- WHEN `SkipMaskGenerator::generate` is called
- THEN the clamped value of `numSteps` is used, producing all steps active
