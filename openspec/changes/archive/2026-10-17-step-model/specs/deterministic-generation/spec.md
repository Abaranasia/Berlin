# Delta for Deterministic Generation

## ADDED Requirements

### Requirement: Variable Sequence Length Parameter

`GenerationParams` MUST gain `length` (int, default 16, valid range `[1, kMaxSteps]` = `[1, 64]`). Generation in all three rhythm modes MUST produce a `Sequence` of exactly `length` steps. In Random mode the active-step count MUST be `(length * 11 + 8) / 16` (integer division, minimum 1 for `length >= 1`), which equals 11 at length 16. Generators MUST NOT produce skipped steps (`skipped` is always false in generated output). At `length == 16`, output MUST be byte-identical to the pre-change output for the same seed and parameters, including RNG draw counts per mode (Random: 5 draws before the pitch pass; Euclidean: 0; Probability: 16), so the existing reproducibility goldens are unchanged. Changing `length` and regenerating from the same seed MUST be reproducible (same seed + same `length` + same other params -> identical Sequence).

#### Scenario: Default length reproduces pre-change output

- GIVEN `GenerationParams` with `length` at its default 16 and an unchanged seed, in each of the three rhythm modes
- WHEN a `Sequence` is generated before and after this change
- THEN the resulting Sequences are byte-identical and the ReproducibilityTests goldens are unchanged

#### Scenario: Output size equals length

- GIVEN any `length` in `[1, 64]` and any rhythm mode
- WHEN a `Sequence` is generated
- THEN it has exactly `length` steps

#### Scenario: Random mode active count scales with length

- GIVEN Random mode
- WHEN generating at lengths 16, 8, 64 and 1
- THEN the active counts are 11, 6, 44 and 1 respectively

#### Scenario: Generated sequences contain no skips

- GIVEN any seed, mode and length
- WHEN a `Sequence` is generated
- THEN every step has `skipped == false`

#### Scenario: Same seed and length reproduce identically

- GIVEN a fixed seed, mode, parameters and `length = 24`
- WHEN the Sequence is built twice
- THEN both are byte-identical

## MODIFIED Requirements

### Requirement: Seed-In, Identical-Out Reproducibility Contract

Every generator in this capability MUST produce byte-identical output given the same seed and the same input parameters, within a single build. For the live app's composed Generate/Randomize path, this contract is scoped to: same seed + same rhythm mode + same mode parameters (`kActiveSteps` derived from `length`; or `pulses`/`rotation`; or the Probability-mode uniform probability value) + same `length` + same pitch parameters (`scaleType`, `rootPitchClass`, `rangeLow`, `rangeHigh`) -> identical resulting Sequence. It does NOT guarantee identical output across different modes, different `length` values, or different scale/root/range combinations, for the same seed.
(Previously: scoped to seed + rhythm mode + mode parameters + pitch parameters, with the step count fixed at 16 and not a variable in the contract.)

#### Scenario: Same seed, identical Sequence output

- GIVEN a fixed `numSteps`, a fixed `density`, and a fixed seed
- WHEN `RhythmGenerator::generate` is called twice with fresh `DeterministicRandom` instances constructed from that same seed
- THEN both resulting `Sequence` objects have identical `active` flags at every index

#### Scenario: Same seed, same mode, same params — reproducible end-to-end

- GIVEN a fixed seed, a fixed rhythm mode, a fixed `length` and fixed mode parameters
- WHEN the composed Sequence (rhythm source + `PitchGenerator`) is built twice from that same seed
- THEN both resulting Sequences are byte-identical (same active flags, same notes)

#### Scenario: Default pitch and length parameters reproduce pre-change output byte-identically

- GIVEN `GenerationParams` with `scaleType`, `rootPitchClass`, `rangeLow`, `rangeHigh` and `length` left at their defaults (minor, C, 36, 72, 16) and an unchanged seed
- WHEN a `Sequence` is generated before and after this change is applied
- THEN the two resulting `Sequence` objects are byte-identical

#### Scenario: Same seed, same scale/root/range reproduces identically; changing them alters pitch only

- GIVEN a fixed seed and a fixed rhythm mode/parameters
- WHEN a `Sequence` is generated twice with the same `scaleType`/`rootPitchClass`/`rangeLow`/`rangeHigh`, then once more after changing only those pitch parameters
- THEN the first two Sequences are byte-identical, and the third differs only in `note` values at active steps — the `active` flags (rhythm) are unchanged across all three

### Requirement: Production Composition of a Rhythm Source and PitchGenerator

The system MUST compose a rhythm-producing generator with `PitchGenerator` together in production code, driven by a single `DeterministicRandom` per generation, callable at runtime (not only during construction). For the live app's Generate/Randomize path, the rhythm source is selectable per a rhythm-mode switch with three branches: `SkipMaskGenerator` (Random mode), `EuclideanRhythmGenerator` (Euclidean mode), and `RhythmGenerator` (Probability mode) — `buildSeededSequence` (static, ctor-member-init-list-safe) dispatches on `RhythmMode` and takes the step count from `GenerationParams.length`. `EuclideanRhythmGenerator::generate()` consumes zero RNG draws before the `PitchGenerator` pass. `SkipMaskGenerator` consumes exactly `length - activeCount` draws (5 at length 16). `RhythmGenerator` in Probability mode consumes exactly `length` draws (16 at length 16, one per step), regardless of the per-step probability values used. The same seed therefore produces different pitches depending on which of the three modes is active, because `PitchGenerator` begins drawing from a different `DeterministicRandom` stream position in each mode.
(Previously: the step count was the constant `kNumSteps` (16) and draw counts were stated only for 16 steps.)

#### Scenario: Production build composes all three rhythm sources at runtime

- GIVEN a running app with an existing `Sequence`
- WHEN a Generate or Randomize action is triggered
- THEN the currently selected rhythm source (`SkipMaskGenerator`, `EuclideanRhythmGenerator`, or `RhythmGenerator` per the active `RhythmMode`) and `PitchGenerator` are recomposed against a fresh `DeterministicRandom` for `GenerationParams.length` steps, not only during `MainComponent` construction

#### Scenario: Same seed diverges in pitch across all three modes — expected, not a bug

- GIVEN the same seed value in Random mode (5 draws before the pitch pass at length 16), Euclidean mode (0 draws), and Probability mode (16 draws, one per step)
- WHEN a Sequence is generated in each mode
- THEN the three resulting Sequences' pitches differ from each other, because the pitch pass starts from a different `DeterministicRandom` stream position in each mode — expected, documented behavior

#### Scenario: Probability mode draw count follows length

- GIVEN Probability mode at `length = 8`
- WHEN a Sequence is generated
- THEN exactly 8 `nextFloat()` draws are consumed before the pitch pass

#### Scenario: Probability mode uses a single staged-apply uniform-probability control

- GIVEN Probability mode is selected and a user adjusts the single scalar probability control
- WHEN Generate is pressed
- THEN the control's value is expanded into a uniform `length`-element probability vector via the vector constructor and used for that generation — no per-step UI exists

### Requirement: Per-Step Editing and Preset Persistence Are Explicit Non-Goals This Slice

Per-step probability editing UI and persistence of rhythm-mode configuration (mode, pulses, rotation, probability value, `length`) in `Preset.h`/`PresetManager.cpp` MUST NOT be implemented in this slice. The single staged-apply scalar control is the entire UI surface for Probability mode; presets continue persisting only `seed`, unchanged. Loading a preset or restoring plugin state MUST regenerate using the current live `GenerationParams`, including the current `length`, exactly as rhythm mode is handled today.
(Previously: `length` did not exist; the non-goal listed mode, pulses, rotation and probability only.)

#### Scenario: No per-step editor exists

- GIVEN Probability mode is active
- WHEN inspecting the UI
- THEN no per-step (16-cell) probability editor exists; only the single scalar staged-apply control is present

#### Scenario: Saving a preset does not persist mode, pulses, rotation, probability or length

- GIVEN Probability mode is active with a non-default probability value and `length = 32`
- WHEN a preset is saved and later loaded
- THEN the loaded preset does not restore rhythm mode, pulses, rotation, the probability value or `length` — only `seed` round-trips

#### Scenario: Preset load regenerates at the live length

- GIVEN `length = 8` is the current generation parameter
- WHEN a preset is loaded
- THEN the resulting Sequence has 8 steps, built from the preset's seed and the live parameters
