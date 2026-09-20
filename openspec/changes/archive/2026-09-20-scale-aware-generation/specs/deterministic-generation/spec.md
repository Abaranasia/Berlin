# Delta for Deterministic Generation

## MODIFIED Requirements

### Requirement: PitchGenerator Scale-Based Note Generation

The system MUST provide a `PitchGenerator` configured with `{Scale, rangeLow, rangeHigh}` that generates the next note via `generateNextNote(DeterministicRandom&)`, returning only notes within `[rangeLow, rangeHigh]` that belong to the configured `Scale`. `Scale`, `rangeLow`, and `rangeHigh` MUST be sourced from `GenerationParams` (`scaleType`, `rootPitchClass`, `rangeLow`, `rangeHigh`) rather than hardcoded, via a scale-catalog lookup keyed by `scaleType` and transposed to `rootPitchClass`.
(Previously: `PitchGenerator` was always constructed with a hardcoded `Scale::minor(48)` and the fixed range `[36, 72]`, set as constant literals in `SequenceBuilder.cpp`.)

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

#### Scenario: PitchGenerator inputs come from GenerationParams, not literals

- GIVEN a `GenerationParams` with `scaleType = dorian`, `rootPitchClass = 2` (D), `rangeLow = 40`, `rangeHigh = 64`
- WHEN `buildSeededSequence` constructs its `PitchGenerator`
- THEN the `PitchGenerator` is configured with the D Dorian scale and the range `[40, 64]`, not `Scale::minor(48)` / `[36, 72]`

#### Scenario: Every generated note satisfies scale membership and range for any chosen scale/root/range

- GIVEN any valid combination of `scaleType`, `rootPitchClass`, `rangeLow`, and `rangeHigh` from `GenerationParams`
- WHEN a `Sequence` is generated
- THEN every active step's note is a member of the resulting `Scale` (mod-12 against `rootPitchClass`) and lies within `[rangeLow, rangeHigh]`

### Requirement: Seed-In, Identical-Out Reproducibility Contract

Every generator in this capability MUST produce byte-identical output given the same seed and the same input parameters, within a single build. For the live app's composed Generate/Randomize path, this contract is scoped to: same seed + same rhythm mode + same mode parameters (`kActiveSteps`; or `pulses`/`rotation`; or the Probability-mode uniform probability value) + same pitch parameters (`scaleType`, `rootPitchClass`, `rangeLow`, `rangeHigh`) -> identical resulting Sequence. It does NOT guarantee identical output across different modes, or different scale/root/range combinations, for the same seed.
(Previously: scoped to seed + rhythm mode + mode parameters only, with pitch always fixed to hardcoded `Scale::minor(48)`/`[36, 72]` and therefore never a variable in the contract.)

#### Scenario: Same seed, identical Sequence output

- GIVEN a fixed `numSteps`, a fixed `density`, and a fixed seed
- WHEN `RhythmGenerator::generate` is called twice with fresh `DeterministicRandom` instances constructed from that same seed
- THEN both resulting `Sequence` objects have identical `active` flags at every index

#### Scenario: Same seed, same mode, same params — reproducible end-to-end

- GIVEN a fixed seed, a fixed rhythm mode, and fixed mode parameters
- WHEN the composed Sequence (rhythm source + `PitchGenerator`) is built twice from that same seed
- THEN both resulting Sequences are byte-identical (same active flags, same notes)

#### Scenario: Default pitch parameters reproduce pre-change output byte-identically

- GIVEN `GenerationParams` with `scaleType`, `rootPitchClass`, `rangeLow`, `rangeHigh` left at their defaults (minor, C, 36, 72) and an unchanged seed
- WHEN a `Sequence` is generated before and after this change is applied
- THEN the two resulting `Sequence` objects are byte-identical

#### Scenario: Same seed, same scale/root/range reproduces identically; changing them alters pitch only

- GIVEN a fixed seed and a fixed rhythm mode/parameters
- WHEN a `Sequence` is generated twice with the same `scaleType`/`rootPitchClass`/`rangeLow`/`rangeHigh`, then once more after changing only those pitch parameters
- THEN the first two Sequences are byte-identical, and the third differs only in `note` values at active steps — the `active` flags (rhythm) are unchanged across all three
