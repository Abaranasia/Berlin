# Delta for Deterministic Generation

## ADDED Requirements

### Requirement: Production Composition of a Rhythm Source and PitchGenerator

The system MUST compose a rhythm-producing generator with `PitchGenerator` together in production code, driven by a single `DeterministicRandom` per generation, and this composition MUST be callable at runtime (not only during construction). For the live app's Generate/Randomize path, the rhythm source is `SkipMaskGenerator` (see "SkipMaskGenerator Produces a Deterministic Displacement Pattern" below) — `RhythmGenerator` remains a separate, fully valid, independently-tested capability with no production call site of its own after this change.

#### Scenario: Production build composes both generators at runtime

- GIVEN a running app with an existing `Sequence`
- WHEN a Generate or Randomize action is triggered
- THEN the rhythm source (`SkipMaskGenerator`) and `PitchGenerator` are recomposed against a fresh `DeterministicRandom`, not only during `MainComponent` construction

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

The system MUST provide a `SkipMaskGenerator` configured with `{numSteps, activeSteps}` that produces a `Sequence` via `generate(DeterministicRandom&)` by starting from a fully-active step pattern and deterministically skipping exactly `numSteps - activeSteps` steps (step 0 is never skipped, anchoring the pattern's phase). This is a distinct capability from `RhythmGenerator`: `RhythmGenerator`'s existing "Density as Per-Step Probability" requirement (in the main `deterministic-generation` spec, unaffected by this delta) is unchanged and remains fully valid — `SkipMaskGenerator` is additive, not a replacement of that contract, even though the live app's Generate/Randomize path uses `SkipMaskGenerator` rather than `RhythmGenerator`.

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

- GIVEN any `numSteps` and `activeSteps`
- WHEN `SkipMaskGenerator::generate` is called
- THEN each `Step`'s `note` field remains at its default value (uninitialized or zero-initialized per `Step`'s constructor)

#### Scenario: activeSteps clamped at both ends

- GIVEN `activeSteps` of 0 (below valid range)
- WHEN `SkipMaskGenerator::generate` is called
- THEN the clamped value of 1 is used, producing exactly 1 active step

- GIVEN `activeSteps` greater than `numSteps`
- WHEN `SkipMaskGenerator::generate` is called
- THEN the clamped value of `numSteps` is used, producing all steps active
