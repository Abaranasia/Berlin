# Delta for Web UI Controls

Built on top of the pending `step-velocity` delta. Archive `step-velocity` first, then this change.

## MODIFIED Requirements

### Requirement: Per-Step Coalescing With Busy Retry

Step edits MUST use the store key `step.<i>` per step, with latest-wins coalescing as in "Optimistic Updates And Latest-Wins Coalescing". Because `setStep` values are absolute, the pending value for a key MUST replace any earlier one; the pending value MUST carry `note`, `active`, `skipped`, `velocity` and `gatePercent` together so a note drag, a mute toggle, a skip toggle, a velocity cycle and a gate cycle on the same step coalesce into one absolute state. `sendStep` MUST take one step object `{note, active, skipped, velocity, gatePercent}` and send all five absolute values. On a `busy` result with no newer pending value, the store MUST retry that key so the last requested value lands; it MUST NOT drop the optimistic value on `busy`. Non-busy failures MUST revert as usual. Tokens `step-out-of-range`, `note-out-of-scale`, `velocity-out-of-range` and `gate-out-of-range` MUST show a human-readable status message; `busy` keeps "Busy, try again" only if the retry is exhausted.
(Previously: the pending value carried note, active, skipped and velocity; no `gate-out-of-range` message.)

#### Scenario: Fast wheel ends on the last note [V]
- GIVEN 10 wheel notches on step 4 while the first `setStep` is in flight
- WHEN the first response arrives
- THEN exactly one further `setStep` is sent, with the latest absolute note

#### Scenario: Busy retry lands the last value [V]
- GIVEN `setStep` returns `busy` once, then ok
- WHEN the retry runs
- THEN the final dispatched value equals the last requested value and the display never reverts to the old one

#### Scenario: Different steps do not coalesce [V]
- GIVEN edits to step 1 and step 2 while step 1 is in flight
- WHEN responses arrive
- THEN both steps' final values are dispatched

#### Scenario: Skip and mute on one step coalesce [V]
- GIVEN a skip toggle then a mute toggle on step 6 while the first `setStep` is in flight
- WHEN the first response arrives
- THEN exactly one further `setStep` is sent carrying both the latest `active` and the latest `skipped`

#### Scenario: Velocity and note on one step coalesce [V]
- GIVEN a velocity cycle then a note drag on step 6 while the first `setStep` is in flight
- WHEN the first response arrives
- THEN exactly one further `setStep` is sent carrying the latest note and the latest velocity

#### Scenario: Gate and velocity on one step coalesce [V]
- GIVEN a gate cycle then a velocity cycle on step 6 while the first `setStep` is in flight
- WHEN the first response arrives
- THEN exactly one further `setStep` is sent carrying the latest `gatePercent` and the latest velocity

#### Scenario: sendStep sends the absolute step [V]
- GIVEN a gate cycle on a step with note 48, active, not skipped, velocity 100, `gatePercent` 100
- WHEN `sendStep` dispatches
- THEN the command is `setStep {index, note:48, active:true, skipped:false, velocity:100, gatePercent:25}`

#### Scenario: Non-busy failure reverts [V]
- GIVEN `setStep` returns `note-out-of-scale`, `velocity-out-of-range` or `gate-out-of-range`
- WHEN handled
- THEN the step shows the last confirmed value and the status shows a mapped message

### Requirement: Typed Protocol

`protocol.ts` MUST type the 16 commands (including `setStep {index, note, active, skipped?, velocity?, gatePercent?}`) with their args, the snapshot (including `edited`, `scaleNotes`, `generationParams.length` and steps typed `{note, active, skipped, velocity, gatePercent}`), the events (`playhead`, `snapshot`), the error tokens (including `step-out-of-range`, `note-out-of-scale`, `velocity-out-of-range` and `gate-out-of-range`), the enum lists (waveform, lfoDestination, scaleType, delayDivision, mode) and hand-written parameter limits including the `length` range `[1, 64]` and the `gatePercent` range `[1, 100]`. Seed MUST be a string.
(Previously: `setStep {index, note, active, skipped?, velocity?}`, steps `{note, active, skipped, velocity}`, no `gate-out-of-range`.)

#### Scenario: Enum lists match the bridge encoding
- GIVEN the protocol enum lists
- WHEN compared with the `ui-bridge` encoding conventions
- THEN they are identical (e.g. waveform `saw|square|pulse|triangle`)

#### Scenario: Unknown command is a type error
- GIVEN a TS call with a command name outside the 16
- WHEN type-checked
- THEN compilation fails

#### Scenario: setStep args are typed [V]
- GIVEN a TS call to `setStep` missing `active`
- WHEN type-checked
- THEN compilation fails
- AND a call omitting `skipped`, `velocity` and `gatePercent` type-checks

#### Scenario: Length limits are exposed [V]
- GIVEN the protocol limits
- WHEN `length` limits are read
- THEN min is 1, max is 64 and the default is 16

#### Scenario: Gate limits are exposed [V]
- GIVEN the protocol limits
- WHEN `gatePercent` limits are read
- THEN min is 1, max is 100 and the default is 100

### Requirement: Mock Mirrors setStep Validation

The dev mock bridge MUST mirror the engine's `setStep` validation and encoding for velocity and gate length: `steps` entries carry `velocity` (default 100) and `gatePercent` (default 100); an absent `velocity` or `gatePercent` keeps the current value; an integer `velocity` outside `[1, 127]` returns `velocity-out-of-range`; an integer `gatePercent` outside `[1, 100]` returns `gate-out-of-range`; non-integer or non-finite values return `invalid type: <name>` / `non-finite: <name>`; validation order is index, note, velocity, gate, then busy.
(Previously: covered velocity only, order index, note, velocity, busy.)

#### Scenario: Mock rejects out-of-range velocity [V]
- GIVEN the mock bridge
- WHEN `setStep` is dispatched with `velocity: 128`
- THEN it returns `{ok:false, error:"velocity-out-of-range"}` and the step is unchanged

#### Scenario: Mock keeps velocity when absent [V]
- GIVEN a mock step with velocity 40
- WHEN `setStep` is dispatched without `velocity`
- THEN the snapshot step still has velocity 40

#### Scenario: Mock rejects out-of-range gate [V]
- GIVEN the mock bridge
- WHEN `setStep` is dispatched with `gatePercent: 101`, separately `gatePercent: 0`
- THEN it returns `{ok:false, error:"gate-out-of-range"}` and the step is unchanged

#### Scenario: Mock keeps gate when absent [V]
- GIVEN a mock step with `gatePercent` 25
- WHEN `setStep` is dispatched without `gatePercent`
- THEN the snapshot step still has `gatePercent` 25

#### Scenario: Mock gate validation order [V]
- GIVEN the mock bridge
- WHEN `setStep` is dispatched with velocity 0 and `gatePercent` 0
- THEN the error is `velocity-out-of-range`, and with a valid velocity it is `gate-out-of-range`

### Requirement: Velocity Levels And Snapping

`ui/src/lib/dynamics.ts` MUST define two level tables: velocity levels ghost 40, soft 70, normal 100, accent 127, and gate levels staccato 25, short 50, long 75, full 100. For each table it MUST map any in-range value (velocity `[1, 127]`, gate `[1, 100]`) to its nearest level, with ties going to the higher level, and MUST provide the next level strictly above a given level, wrapping the top level (accent, full) to the bottom level (ghost, staccato). Level names MUST be the strings `ghost`, `soft`, `normal`, `accent` and `staccato`, `short`, `long`, `full`. The nearest/next logic MUST be shared by both tables and the velocity behavior MUST be unchanged.
(Previously: a single velocity level table only.)

#### Scenario: Nearest level [V]
- GIVEN velocities 1, 55, 56, 85, 113, 114 and 127
- WHEN snapped
- THEN the levels are ghost, soft, soft, normal, normal, accent and accent

#### Scenario: Next level cycles upward [V]
- GIVEN each velocity level in turn
- WHEN the next level is requested
- THEN ghost gives soft, soft gives normal, normal gives accent and accent gives ghost

#### Scenario: Cycle from an off-level velocity [V]
- GIVEN velocity 85 (displayed normal)
- WHEN the next level is requested
- THEN the result is accent (127)

#### Scenario: Nearest gate level [V]
- GIVEN gate values 1, 37, 38, 62, 63, 87, 88 and 100
- WHEN snapped
- THEN the levels are staccato, staccato, short, short, long, long, full and full (ties go to the higher level)

#### Scenario: Next gate level cycles upward [V]
- GIVEN each gate level in turn
- WHEN the next level is requested
- THEN staccato gives short, short gives long, long gives full and full gives staccato

#### Scenario: Cycle from an off-level gate [V]
- GIVEN `gatePercent` 90 (displayed full), and separately 38 (displayed short)
- WHEN the next level is requested
- THEN the results are staccato (25) and long (75) respectively

## ADDED Requirements

### Requirement: Step Gate Cycle Button

Each step cell MUST provide a gate-length cycle button, class `.step-gate`, as a sibling of the cell, in the same `step-controls` row as the velocity and skip buttons. Its displayed level MUST be the nearest gate level to the step's `gatePercent`, exposed as `data-gate-length` with the level name (`staccato`, `short`, `long` or `full`); the existing `data-gate` attribute and the cell's `gate` prop keep meaning the active flag and MUST NOT be reused. Activating it MUST dispatch `setStep` with the absolute `gatePercent` of the next level strictly above the displayed one (25 -> 50 -> 75 -> 100 -> 25) together with the step's other current values. The button MUST have an accessible name that includes the level name, MUST be part of the roving tabindex, MUST keep the 44 px minimum target, and MUST be visually distinguishable by level.

#### Scenario: Gate button shows the level [V]
- GIVEN steps with `gatePercent` 25, 50, 75 and 100
- WHEN rendered
- THEN `data-gate-length` is staccato, short, long and full, and each aria-label contains that name

#### Scenario: Gate button cycles 25, 50, 75, 100 [V]
- GIVEN a step with `gatePercent` 25
- WHEN the gate button is activated three times, each response applied
- THEN `setStep` carries `gatePercent` 50, then 75, then 100, then (a fourth activation) 25

#### Scenario: Gate button does not change the active flag [V]
- GIVEN an active step
- WHEN its gate button is activated
- THEN the dispatched `active` is true, `data-gate` is unchanged and the cell is not muted

#### Scenario: Gate button is operable by keyboard [V]
- GIVEN the gate button focused
- WHEN Enter or Space is pressed
- THEN the same `setStep` is dispatched as for a click
