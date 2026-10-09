# Euclidean Rhythm Specification

## Purpose

Bjorklund's algorithm as a pure, deterministic rhythm generator: given `(steps, pulses, rotation)`, produces the maximally-even Euclidean placement of `pulses` active steps among `steps` steps, as a `Sequence` — a drop-in alternative to `SkipMaskGenerator` at `MainComponent::buildSeededSequence`'s call site.

## Requirements

### Requirement: Bjorklund Pulse Placement Matches Published Patterns

`EuclideanRhythmGenerator` MUST implement Bjorklund's algorithm, producing the maximally-even distribution of `pulses` active steps among `steps` steps, matching canonical published Euclidean rhythms exactly.

#### Scenario: E(3,8) — tresillo

- GIVEN `steps=8, pulses=3, rotation=0`
- WHEN `generate()` is called
- THEN active steps are exactly {0, 3, 6}; all others inactive

#### Scenario: E(5,8) — cinquillo

- GIVEN `steps=8, pulses=5, rotation=0`
- WHEN `generate()` is called
- THEN active steps are exactly {0, 2, 3, 5, 6}; all others inactive

#### Scenario: E(5,16)

- GIVEN `steps=16, pulses=5, rotation=0`
- WHEN `generate()` is called
- THEN active steps are exactly {0, 3, 6, 9, 12}; all others inactive

#### Scenario: E(4,16) — evenly divides, maximal spacing

- GIVEN `steps=16, pulses=4, rotation=0`
- WHEN `generate()` is called
- THEN active steps are exactly {0, 4, 8, 12}; all others inactive

### Requirement: No RNG Parameter — Deliberate API-Shape Deviation

`EuclideanRhythmGenerator::generate()` MUST NOT accept a `DeterministicRandom` parameter. Bjorklund is a pure function of `(steps, pulses, rotation)`; every other generator in `deterministic-generation` (`RhythmGenerator`, `SkipMaskGenerator`, `PitchGenerator`) takes one. This asymmetry is intentional and MUST be documented at the call site, not treated as an oversight or made consistent by adding an unused parameter. This is a signature-level (compile-time) contract — "consumes zero RNG draws" is guaranteed by the absence of the parameter, not something to be asserted at runtime.

#### Scenario: Signature omits DeterministicRandom

- GIVEN the `EuclideanRhythmGenerator` public interface
- WHEN inspecting `generate()`
- THEN its signature takes no `DeterministicRandom&` argument, unlike every sibling generator

### Requirement: Fixed Step Count This Slice

`steps` MUST be fixed at `kNumSteps` (16) at the production call site in `MainComponent`. The generator's own constructor MAY accept any `numSteps` (needed so its unit tests can exercise the 8-step canonical oracles E(3,8)/E(5,8)) — the "fixed at 16" constraint is a call-site/UI constraint only, not a generator-level limitation. Variable-length sequences in the live app are out of scope for this slice.

#### Scenario: Steps parameter is not exposed as user-variable in the live app

- GIVEN the live app's Euclidean mode
- WHEN a Sequence is generated
- THEN `steps` is always 16, regardless of Pulses/Rotation UI values

### Requirement: Pulses and Rotation Are Clamped/Wrapped, Never Crash

`pulses` MUST be clamped to `[0, steps]` before generation — this floor of 0 (not 1) is a deliberate deviation from `SkipMaskGenerator`'s `[1, numSteps]` clamp (which exists because SkipMaskGenerator's step-0 phase anchor cannot be silent); Euclidean has no such anchor, and `pulses == 0` MUST be total, mirroring the `RhythmGenerator` `density == 0` precedent, not SkipMaskGenerator's. `rotation` MUST wrap via a true mathematical modulo of `steps` (always yielding a non-negative result in `[0, steps)`, including for negative input) — NOT C++'s `%`, whose result follows the dividend's sign. `pulses == steps` MUST produce all steps active.

#### Scenario: pulses=0 is total, all-inactive

- GIVEN `steps=16, pulses=0` (any rotation)
- WHEN `generate()` is called
- THEN the resulting Sequence has 16 steps, all inactive, no crash

#### Scenario: pulses clamped above steps

- GIVEN `steps=16, pulses=20`
- WHEN `generate()` is called
- THEN the clamped value 16 is used; all 16 steps are active

#### Scenario: pulses clamped below zero

- GIVEN `steps=16, pulses=-3`
- WHEN `generate()` is called
- THEN the clamped value 0 is used; all 16 steps are inactive

#### Scenario: negative rotation wraps into valid range

- GIVEN `steps=8, pulses=3, rotation=-1`
- WHEN `generate()` is called
- THEN the effective rotation used is 7 (i.e. `-1 mod 8 = 7`), not a negative index and not a crash

### Requirement: Rotation Circularly Shifts the Base Pattern

For a given `rotation` R, the output active-step set MUST equal the `rotation=0` base pattern's active-step set shifted circularly, rotating LEFT: `output[i].active == base[(i + R) mod steps].active` for every step `i`. `rotation == steps` (or any multiple of `steps`) MUST be equivalent to `rotation == 0`.

#### Scenario: Rotating E(3,8) by 1 shifts the pattern left

- GIVEN `steps=8, pulses=3` with base (rotation=0) active steps {0,3,6} (pattern `10010010`)
- WHEN `rotation=1` is applied
- THEN the pattern becomes `00100101` — active steps {2,5,7}

#### Scenario: Full-cycle rotation is a no-op

- GIVEN `steps=8, pulses=3, rotation=8`
- WHEN `generate()` is called
- THEN the result is identical to `rotation=0`

### Requirement: Rotation Reorders Onsets, Not the Melody Riding on Them

Because `PitchGenerator`'s pitch pass assigns drawn notes to active steps in ascending index order (unchanged by this feature), rotating the Euclidean pattern changes WHICH onset each already-drawn pitch lands on — it does NOT carry a fixed melodic phrase along with the rhythm. Increasing `pulses` similarly keeps the earlier-drawn pitch values but redistributes them onto a different set of active steps. This is expected, not a defect: rhythm and pitch are independent axes in this generator's contract.

#### Scenario: Rotating the rhythm reassigns pitches to the new onsets, not the reverse

- GIVEN a generated Sequence in Euclidean mode with pitches P0..P(n-1) assigned in onset order to base-pattern active steps
- WHEN the rotation parameter changes (same seed, same pulses) and the Sequence is regenerated
- THEN the same seed's pitch draws P0..P(n-1) are reassigned in ascending order to the NEW rotated set of active steps — no pitch "follows" its original onset position

### Requirement: Produces a Sequence, Drop-In Replacement Shape

`generate()` MUST return a `Sequence` of size `steps` with only `.active` populated per step (`.note` left at default), identical output shape to `SkipMaskGenerator::generate`, so it is substitutable at `MainComponent::buildSeededSequence`'s call site without changes to the downstream pitch pass, Mutate, Auto-Evolve, or Export.

#### Scenario: Only active is populated

- GIVEN any valid `(steps, pulses, rotation)`
- WHEN `generate()` produces a Sequence
- THEN every step's `note` field is left at its default/unset value, and only `active` is populated
