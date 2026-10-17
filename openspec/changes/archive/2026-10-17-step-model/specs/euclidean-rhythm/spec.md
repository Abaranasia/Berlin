# Delta for Euclidean Rhythm

## MODIFIED Requirements

### Requirement: Fixed Step Count This Slice

In the live app, `steps` MUST equal `GenerationParams.length` (default 16, range `[1, 64]`) at the production call site. The generator's own constructor MAY accept any `numSteps`. At `length == 16` the output MUST be identical to the pre-change fixed-16 output. `pulses` MUST be clamped to `[0, length]` and `rotation` MUST wrap into `[0, length - 1]` using the existing mathematical-modulo rule.
(Previously: `steps` was fixed at `kNumSteps` (16) at the call site and variable-length sequences were out of scope.)

#### Scenario: Steps follows the length parameter

- GIVEN the live app's Euclidean mode with `length = 12`
- WHEN a Sequence is generated
- THEN it has 12 steps, built by Bjorklund over 12 steps

#### Scenario: Default length is unchanged behavior

- GIVEN Euclidean mode with `length = 16`
- WHEN a Sequence is generated
- THEN `steps` is 16 and the output equals the pre-change output for the same pulses/rotation

#### Scenario: Pulses clamp to the length

- GIVEN `length = 8` and `pulses = 12`
- WHEN a Sequence is generated
- THEN pulses is clamped to 8 and all 8 steps are active

#### Scenario: Rotation wraps within the length

- GIVEN `length = 8` and `rotation = 8` (or `-1`)
- WHEN a Sequence is generated
- THEN the effective rotation is 0 (or 7 respectively)

#### Scenario: Length 1

- GIVEN `length = 1` with `pulses = 1`, and separately `pulses = 0`
- WHEN a Sequence is generated
- THEN the sequence has 1 step, active in the first case and inactive in the second, with no crash
