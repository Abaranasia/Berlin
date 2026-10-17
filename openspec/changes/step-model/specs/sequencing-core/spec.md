# Delta for Sequencing Core

## MODIFIED Requirements

### Requirement: Step Value Type

The system MUST provide a `Step` value type with exactly three fields: `note` (integer pitch), `active` (boolean gate flag) and `skipped` (boolean, default `false`). `skipped` MUST be declared after `active` so `Step` stays an aggregate and `Step{note, active}` still compiles with `skipped == false`. A skipped step is removed from the playback/export cycle (takes no time); `active == false` on a non-skipped step remains a timed rest. `Step` MUST NOT expose velocity, gate length, duration, probability, timing-offset, or accent fields.
(Previously: exactly two fields, `note` and `active`.)

#### Scenario: Construct a step

- GIVEN a `note` value and an `active` flag
- WHEN a `Step` is constructed with them
- THEN `step.note` equals the given note
- AND `step.active` equals the given flag
- AND `step.skipped` is false

#### Scenario: Construct a skipped step

- GIVEN a `note`, an `active` flag and `skipped = true`
- WHEN a `Step` is aggregate-initialised with all three
- THEN each field reads back the given value

#### Scenario: Step has no extra fields

- GIVEN the `Step` type definition
- WHEN inspecting its public members
- THEN only `note`, `active` and `skipped` are present

### Requirement: Sequence Capacity Constant

The system MUST define a single `kMaxSteps = 64` constant as the maximum sequence length for generation, editing and display. `Sequence` itself remains resizable and MUST NOT be hard-limited by this constant.

#### Scenario: Constant value

- GIVEN the core headers
- WHEN `kMaxSteps` is read
- THEN it equals 64
