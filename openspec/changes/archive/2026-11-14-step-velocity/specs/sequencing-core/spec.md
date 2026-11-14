# Delta for Sequencing Core

Archive note: the capability Purpose line says "No timing, velocity, gate, ... fields yet"; after this change it MUST read that velocity exists and gate, duration, probability and accent remain future work.

## MODIFIED Requirements

### Requirement: Step Value Type

The system MUST provide a `Step` value type with exactly four fields: `note` (integer pitch), `active` (boolean gate flag), `skipped` (boolean, default `false`) and `velocity` (integer MIDI velocity in `[1, 127]`, default `kDefaultVelocity` = 100). `skipped` MUST be declared after `active` and `velocity` MUST be declared last, so `Step` stays an aggregate and `Step{note, active}` and `Step{note, active, skipped}` still compile with `velocity == 100`. A skipped step is removed from the playback/export cycle (takes no time); `active == false` on a non-skipped step remains a timed rest. `Step` MUST NOT expose gate length, duration, probability, timing-offset, or accent fields.
(Previously: exactly three fields, no velocity.)

#### Scenario: Construct a step

- GIVEN a `note` value and an `active` flag
- WHEN a `Step` is constructed with them
- THEN `step.note` equals the given note
- AND `step.active` equals the given flag
- AND `step.skipped` is false and `step.velocity` is 100

#### Scenario: Construct a skipped step

- GIVEN a `note`, an `active` flag and `skipped = true`
- WHEN a `Step` is aggregate-initialised with all three
- THEN each field reads back the given value and `velocity` is 100

#### Scenario: Construct a step with velocity

- GIVEN a note, active, skipped and `velocity = 127`
- WHEN a `Step` is aggregate-initialised with all four
- THEN each field reads back the given value

#### Scenario: Step has no extra fields

- GIVEN the `Step` type definition
- WHEN inspecting its public members
- THEN only `note`, `active`, `skipped` and `velocity` are present

## ADDED Requirements

### Requirement: Generated Steps Use the Default Velocity

Every step produced by generation MUST have `velocity == kDefaultVelocity` (100). Velocity MUST NOT consume any random draw, so a given seed produces the same notes, gates and skip flags as before this change and existing golden sequences stay valid.

#### Scenario: Generated steps have velocity 100

- GIVEN any generation mode, seed and length
- WHEN a sequence is generated
- THEN every step has `velocity == 100`

#### Scenario: Goldens unchanged

- GIVEN the existing golden-sequence and reproducibility tests
- WHEN they run without modification
- THEN they pass
