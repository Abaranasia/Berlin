# Delta for Sequencing Core

Built on top of the pending `step-velocity` delta. Archive `step-velocity` first, then this change.

Archive note: the capability Purpose line (after the velocity archive) says velocity exists and "gate, duration, probability and accent remain future work". After this change it MUST read that velocity and per-step gate length exist and duration, probability and accent remain future work.

## MODIFIED Requirements

### Requirement: Step Value Type

The system MUST provide a `Step` value type with exactly five fields: `note` (integer pitch), `active` (boolean gate flag), `skipped` (boolean, default `false`), `velocity` (integer MIDI velocity in `[1, 127]`, default `kDefaultVelocity` = 100) and `gatePercent` (integer gate length as a percentage of the step, in `[1, 100]`, default `kDefaultGatePercent` = 100). `skipped` MUST be declared after `active`, `velocity` after `skipped`, and `gatePercent` MUST be declared last, so `Step` stays an aggregate and `Step{note, active}`, `Step{note, active, skipped}` and `Step{note, active, skipped, velocity}` still compile with `gatePercent == 100`. The system MUST expose `kMinGatePercent` (1), `kMaxGatePercent` (100) and an `isValidGatePercent` predicate that is true exactly for integers in `[1, 100]`. A skipped step is removed from the playback/export cycle (takes no time); `active == false` on a non-skipped step remains a timed rest. `Step` MUST NOT expose duration, probability, timing-offset, or accent fields. The `active` flag keeps its meaning (the on/off flag); `gatePercent` is a separate length, not a replacement for it.
(Previously: exactly four fields, no gate length.)

#### Scenario: Construct a step

- GIVEN a `note` value and an `active` flag
- WHEN a `Step` is constructed with them
- THEN `step.note` equals the given note
- AND `step.active` equals the given flag
- AND `step.skipped` is false, `step.velocity` is 100 and `step.gatePercent` is 100

#### Scenario: Construct a step with velocity

- GIVEN a note, active, skipped and `velocity = 127`
- WHEN a `Step` is aggregate-initialised with all four
- THEN each field reads back the given value and `gatePercent` is 100

#### Scenario: Construct a step with gate length

- GIVEN a note, active, skipped, velocity and `gatePercent = 25`
- WHEN a `Step` is aggregate-initialised with all five
- THEN each field reads back the given value

#### Scenario: Gate percent validity predicate

- GIVEN the values 0, 1, 50, 100, 101 and -1
- WHEN `isValidGatePercent` is evaluated on each
- THEN it is false, true, true, true, false and false

#### Scenario: Step has no extra fields

- GIVEN the `Step` type definition
- WHEN inspecting its public members
- THEN only `note`, `active`, `skipped`, `velocity` and `gatePercent` are present

## ADDED Requirements

### Requirement: Generated Steps Use the Default Gate Length

Every step produced by generation or mutation MUST have `gatePercent == kDefaultGatePercent` (100). Gate length MUST NOT consume any random draw, so a given seed produces the same notes, active flags, skip flags and velocities as before this change and existing golden sequences stay valid.

#### Scenario: Generated steps have gate 100

- GIVEN any generation mode, seed and length
- WHEN a sequence is generated
- THEN every step has `gatePercent == 100`

#### Scenario: Goldens unchanged

- GIVEN the existing golden-sequence and reproducibility tests
- WHEN they run without modification
- THEN they pass
