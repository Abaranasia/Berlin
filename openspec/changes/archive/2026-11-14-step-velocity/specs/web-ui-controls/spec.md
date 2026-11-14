# Delta for Web UI Controls

## MODIFIED Requirements

### Requirement: Per-Step Coalescing With Busy Retry

Step edits MUST use the store key `step.<i>` per step, with latest-wins coalescing as in "Optimistic Updates And Latest-Wins Coalescing". Because `setStep` values are absolute, the pending value for a key MUST replace any earlier one; the pending value MUST carry `note`, `active`, `skipped` and `velocity` together so a note drag, a mute toggle, a skip toggle and a velocity cycle on the same step coalesce into one absolute state. `sendStep` MUST take one step object `{note, active, skipped, velocity}` and send all four absolute values. On a `busy` result with no newer pending value, the store MUST retry that key so the last requested value lands; it MUST NOT drop the optimistic value on `busy`. Non-busy failures MUST revert as usual. Tokens `step-out-of-range`, `note-out-of-scale` and `velocity-out-of-range` MUST show a human-readable status message; `busy` keeps "Busy, try again" only if the retry is exhausted.
(Previously: the pending value carried note, active and skipped; `sendStep` took positional args; no `velocity-out-of-range` message.)

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

#### Scenario: sendStep sends the absolute step [V]
- GIVEN a velocity cycle on a step with note 48, active, not skipped, velocity 100
- WHEN `sendStep` dispatches
- THEN the command is `setStep {index, note:48, active:true, skipped:false, velocity:127}`

#### Scenario: Non-busy failure reverts [V]
- GIVEN `setStep` returns `note-out-of-scale`, or `velocity-out-of-range`
- WHEN handled
- THEN the step shows the last confirmed value and the status shows a mapped message

### Requirement: Typed Protocol

`protocol.ts` MUST type the 16 commands (including `setStep {index, note, active, skipped?, velocity?}`) with their args, the snapshot (including `edited`, `scaleNotes`, `generationParams.length` and steps typed `{note, active, skipped, velocity}`), the events (`playhead`, `snapshot`), the error tokens (including `step-out-of-range`, `note-out-of-scale` and `velocity-out-of-range`), the enum lists (waveform, lfoDestination, scaleType, delayDivision, mode) and hand-written parameter limits including the `length` range `[1, 64]`. Seed MUST be a string.
(Previously: `setStep {index, note, active, skipped?}`, steps `{note, active, skipped}`, no `velocity-out-of-range`.)

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
- AND a call omitting `skipped` and `velocity` type-checks

#### Scenario: Length limits are exposed [V]
- GIVEN the protocol limits
- WHEN `length` limits are read
- THEN min is 1, max is 64 and the default is 16

## ADDED Requirements

### Requirement: Velocity Levels And Snapping

`ui/src/lib/dynamics.ts` MUST define the four levels ghost 40, soft 70, normal 100, accent 127. It MUST map any velocity in `[1, 127]` to its nearest level, with ties going to the higher level, and MUST provide the next level strictly above a given level, wrapping accent to ghost. Level names MUST be the strings `ghost`, `soft`, `normal`, `accent`.

#### Scenario: Nearest level [V]
- GIVEN velocities 1, 55, 56, 85, 113, 114 and 127
- WHEN snapped
- THEN the levels are ghost, soft, soft, normal, normal, accent and accent

#### Scenario: Next level cycles upward [V]
- GIVEN each level in turn
- WHEN the next level is requested
- THEN ghost gives soft, soft gives normal, normal gives accent and accent gives ghost

#### Scenario: Cycle from an off-level velocity [V]
- GIVEN velocity 85 (displayed normal)
- WHEN the next level is requested
- THEN the result is accent (127)

### Requirement: Mock Mirrors setStep Validation

The dev mock bridge MUST mirror the engine's `setStep` validation and encoding for velocity: `steps` entries carry `velocity` (default 100); an absent `velocity` keeps the current value; an integer outside `[1, 127]` returns `velocity-out-of-range`; non-integer or non-finite values return `invalid type: velocity` / `non-finite: velocity`; validation order is index, note, velocity, then busy.

#### Scenario: Mock rejects out-of-range velocity [V]
- GIVEN the mock bridge
- WHEN `setStep` is dispatched with `velocity: 128`
- THEN it returns `{ok:false, error:"velocity-out-of-range"}` and the step is unchanged

#### Scenario: Mock keeps velocity when absent [V]
- GIVEN a mock step with velocity 40
- WHEN `setStep` is dispatched without `velocity`
- THEN the snapshot step still has velocity 40
