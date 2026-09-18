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

Every generator in this capability MUST produce byte-identical output given the same seed and the same input parameters, within a single build. For the live app's composed Generate/Randomize path (which now has a selectable rhythm mode with three options), this contract is scoped to: same seed + same rhythm mode + same mode parameters (`kActiveSteps`; or `pulses`/`rotation`; or the Probability-mode uniform probability value) -> identical resulting Sequence. It does NOT guarantee identical output across different modes for the same seed.
(Previously: scoped to two modes' parameters — `kActiveSteps` or `pulses`/`rotation` — with no Probability-mode parameter.)

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

### Requirement: RhythmGenerator Configured via Per-Step Probability Vector

The system MUST provide a `RhythmGenerator` configured with `{numSteps, std::vector<float> probabilities}` that produces a `Sequence` via `generate(DeterministicRandom&)`, populating only the `active` field of each step. Each step `i`'s `active` flag MUST be determined by an independent draw against `probabilities[i]` (clamped `[0,1]`), NOT by a guaranteed/exact active-step count. The existing scalar-density constructor `(numSteps, float density)` MUST remain available and MUST delegate to the vector constructor by expanding `density` into a uniform vector of length `numSteps`, so it is a thin wrapper, not a parallel implementation.
(Previously: only a scalar `{numSteps, density}` constructor existed; this generalizes it to per-step vector configuration while keeping the scalar form as a delegating overload.)

#### Scenario: Output size matches numSteps

- GIVEN `numSteps = 16` and any valid probability configuration (scalar or vector)
- WHEN `RhythmGenerator::generate` is called
- THEN the resulting `Sequence` has exactly 16 steps

#### Scenario: Density approximates average active-step count, not an exact count

- GIVEN `numSteps = 16` and a uniform vector with every element `0.5`
- WHEN `RhythmGenerator::generate` is run across many distinct seeds
- THEN the mean active-step count approximates 8, with individual runs varying (e.g. 7, 8, 9)

#### Scenario: Per-step probability endpoints are exact, not statistical

- GIVEN a vector where step 3 has probability `0.0` and step 7 has probability `1.0`
- WHEN `generate` is called across many distinct seeds
- THEN step 3 is never active and step 7 is always active in every run

#### Scenario: Scalar constructor is behavior-preserving delegation

- GIVEN a `RhythmGenerator` built via the scalar ctor `(numSteps, density)` and another built via the vector ctor with a uniform vector of that same `density`
- WHEN both are driven by `DeterministicRandom` instances seeded identically
- THEN both produce byte-identical `Sequence` output, proving the scalar path is a pure delegation

#### Scenario: Exactly one RNG draw per step regardless of per-step probability

- GIVEN any valid probability vector (including elements of `0.0` or `1.0`)
- WHEN `generate` runs
- THEN exactly one `nextFloat()` draw is consumed per step, in ascending step order, never short-circuited by an endpoint value

#### Scenario: Vector shorter or longer than numSteps is padded/truncated at construction

- GIVEN a probability vector with fewer or more elements than `numSteps`
- WHEN the `RhythmGenerator` is constructed
- THEN the vector is padded (with `0.0`) or truncated to exactly `numSteps` elements at construction time, immutably, before any `generate` call

#### Scenario: RhythmGenerator only touches active

- GIVEN a `RhythmGenerator` configured with any valid `numSteps` and probability configuration
- WHEN `generate` produces a `Sequence`
- THEN every step's `note` field is left at its default/unset value, and only `active` is populated

#### Scenario: All-zero probability vector no-ops downstream MutationEngine — accepted, not a defect

- GIVEN a probability vector of all `0.0` (or near-zero) elements
- WHEN `generate` produces an all-inactive `Sequence` and that `Sequence` is later passed through `MutationEngine` transforms (e.g. transpose)
- THEN the transform's `hasActive` guard makes it a silent no-op, matching the existing accepted `EuclideanRhythmGenerator pulses=0` precedent — this is expected, documented behavior, not a bug to fix in this slice

### Requirement: Production Composition of a Rhythm Source and PitchGenerator

The system MUST compose a rhythm-producing generator with `PitchGenerator` together in production code, driven by a single `DeterministicRandom` per generation, callable at runtime (not only during construction). For the live app's Generate/Randomize path, the rhythm source is selectable per a rhythm-mode switch with three branches: `SkipMaskGenerator` (Random mode), `EuclideanRhythmGenerator` (Euclidean mode), and `RhythmGenerator` (Probability mode, this change) — `buildSeededSequence` (static, ctor-member-init-list-safe) dispatches on `RhythmMode`. `EuclideanRhythmGenerator::generate()` consumes zero RNG draws before the `PitchGenerator` pass. `SkipMaskGenerator` consumes exactly `kNumSteps - kActiveSteps` draws (5 with current production constants). `RhythmGenerator` in Probability mode consumes exactly `kNumSteps` draws (16, one per step, per the stream-length invariant), regardless of the per-step probability values used. The same seed therefore produces different pitches depending on which of the three modes is active, because `PitchGenerator` begins drawing from a different `DeterministicRandom` stream position in each mode.
(Previously: only two modes — Random and Euclidean — existed in the switch; Probability mode and its 16-draw stream-position contribution did not exist.)

#### Scenario: Production build composes all three rhythm sources at runtime

- GIVEN a running app with an existing `Sequence`
- WHEN a Generate or Randomize action is triggered
- THEN the currently selected rhythm source (`SkipMaskGenerator`, `EuclideanRhythmGenerator`, or `RhythmGenerator` per the active `RhythmMode`) and `PitchGenerator` are recomposed against a fresh `DeterministicRandom`, not only during `MainComponent` construction

#### Scenario: Same seed diverges in pitch across all three modes — expected, not a bug

- GIVEN the same seed value in Random mode (5 draws before the pitch pass), Euclidean mode (0 draws), and Probability mode (16 draws, one per step)
- WHEN a Sequence is generated in each mode
- THEN the three resulting Sequences' pitches differ from each other, because the pitch pass starts from a different `DeterministicRandom` stream position in each mode — expected, documented behavior

#### Scenario: Probability mode uses a single staged-apply uniform-probability control

- GIVEN Probability mode is selected and a user adjusts the single scalar probability control (analogous to Euclidean's pulses/rotation sliders)
- WHEN Generate is pressed
- THEN the control's value is expanded into a uniform 16-element probability vector via the vector constructor and used for that generation — no per-step UI exists this slice

### Requirement: Per-Step Editing and Preset Persistence Are Explicit Non-Goals This Slice

Per-step probability editing UI and persistence of rhythm-mode configuration (mode, pulses, rotation, probability value) in `Preset.h`/`PresetManager.cpp` MUST NOT be implemented in this slice. The single staged-apply scalar control is the entire UI surface for Probability mode; presets continue persisting only `seed`, unchanged from the prior two-mode state.

#### Scenario: No per-step editor exists

- GIVEN Probability mode is active
- WHEN inspecting the UI
- THEN no per-step (16-cell) probability editor exists; only the single scalar staged-apply control is present

#### Scenario: Saving a preset does not persist mode, pulses, rotation, or probability

- GIVEN Probability mode is active with a non-default probability value
- WHEN a preset is saved and later loaded
- THEN the loaded preset does not restore rhythm mode, pulses, rotation, or the probability value — only `seed` round-trips, an unchanged pre-existing gap

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
