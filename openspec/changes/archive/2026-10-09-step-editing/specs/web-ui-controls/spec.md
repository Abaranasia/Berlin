# Delta for Web UI Controls

## ADDED Requirements

### Requirement: Per-Step Coalescing With Busy Retry

Step edits MUST use the store key `step.<i>` per step, with latest-wins coalescing as in "Optimistic Updates And Latest-Wins Coalescing". Because `setStep` values are absolute, the pending value for a key MUST replace any earlier one. On a `busy` result with no newer pending value, the store MUST retry that key so the last requested value lands; it MUST NOT drop the optimistic value on `busy`. Non-busy failures MUST revert as usual. Tokens `step-out-of-range` and `note-out-of-scale` MUST show a human-readable status message; `busy` keeps "Busy, try again" only if the retry is exhausted.

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

#### Scenario: Non-busy failure reverts [V]
- GIVEN `setStep` returns `note-out-of-scale`
- WHEN handled
- THEN the step shows the last confirmed value and the status shows a mapped message

## MODIFIED Requirements

### Requirement: Typed Protocol

`protocol.ts` MUST type the 16 commands (including `setStep {index, note, active}`) with their args, the snapshot (including `edited` and `scaleNotes`), the events (`playhead`, `snapshot`), the error tokens (including `step-out-of-range` and `note-out-of-scale`), the enum lists (waveform, lfoDestination, scaleType, delayDivision, mode) and hand-written parameter limits. Seed MUST be a string.
(Previously: 15 commands, no `edited` or `scaleNotes`, no step error tokens.)

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
