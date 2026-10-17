# Delta for Web UI Controls

## MODIFIED Requirements

### Requirement: Typed Protocol

`protocol.ts` MUST type the 16 commands (including `setStep {index, note, active, skipped?}`) with their args, the snapshot (including `edited`, `scaleNotes`, `generationParams.length` and steps typed `{note, active, skipped}`), the events (`playhead`, `snapshot`), the error tokens (including `step-out-of-range` and `note-out-of-scale`), the enum lists (waveform, lfoDestination, scaleType, delayDivision, mode) and hand-written parameter limits including the `length` range `[1, 64]`. Seed MUST be a string.
(Previously: `setStep {index, note, active}`, steps `{note, active}`, no `length`.)

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
- AND a call omitting `skipped` type-checks

#### Scenario: Length limits are exposed [V]
- GIVEN the protocol limits
- WHEN `length` limits are read
- THEN min is 1, max is 64 and the default is 16

### Requirement: Per-Step Coalescing With Busy Retry

Step edits MUST use the store key `step.<i>` per step, with latest-wins coalescing as in "Optimistic Updates And Latest-Wins Coalescing". Because `setStep` values are absolute, the pending value for a key MUST replace any earlier one; the pending value MUST carry `note`, `active` and `skipped` together so a note drag, a mute toggle and a skip toggle on the same step coalesce into one absolute state. On a `busy` result with no newer pending value, the store MUST retry that key so the last requested value lands; it MUST NOT drop the optimistic value on `busy`. Non-busy failures MUST revert as usual. Tokens `step-out-of-range` and `note-out-of-scale` MUST show a human-readable status message; `busy` keeps "Busy, try again" only if the retry is exhausted.
(Previously: the pending value carried note and gate only.)

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

#### Scenario: Non-busy failure reverts [V]
- GIVEN `setStep` returns `note-out-of-scale`
- WHEN handled
- THEN the step shows the last confirmed value and the status shows a mapped message

### Requirement: Transport, Tempo And Level Controls

The UI MUST provide Play/Stop (`setPlaying`), BPM (−/+ and entry, `setBpm`), master level (`setMasterLevel`), and a step display with one cell per step of the current sequence (`steps.length`, wrapping into rows of 16) with the playhead highlight driven by `playhead` events and the initial snapshot. When the playhead reports no current step (all steps skipped) no cell MUST be highlighted. Consecutive BPM presses MUST accumulate (no stale-closure loss).
(Previously: a 16-step display.)

#### Scenario: Play/Stop
- GIVEN stopped
- WHEN Play is pressed
- THEN `setPlaying {playing:true}` is dispatched and the control shows playing after the response

#### Scenario: Rapid BPM presses
- GIVEN BPM 120
- WHEN + is pressed three times quickly
- THEN the final dispatched BPM equals 120 plus three increments and none are lost

#### Scenario: Master level
- GIVEN the level slider
- WHEN moved
- THEN `setMasterLevel {level}` is dispatched with the slider value

#### Scenario: Variable-length display [V]
- GIVEN snapshots with 8, 16 and 64 steps
- WHEN rendered
- THEN 8, 16 and 64 cells are shown, the last arranged in four rows of 16

#### Scenario: No highlight when everything is skipped [V]
- GIVEN a snapshot where every step is skipped and the playhead has no current step
- WHEN rendered
- THEN no cell is highlighted and nothing throws

#### Scenario: Playhead follows (MANUAL)
- GIVEN playback is running
- WHEN steps advance
- THEN the highlighted step follows and skips the skipped cells, in standalone and Cakewalk Sonar

### Requirement: Live Generation Params

Generation param edits (mode, pulses, rotation, chance, lock seed, scale, root, range, length) MUST be sent immediately via `setGenerationParams` (not staged until Generate). `rangeLow` and `rangeHigh` MUST always be sent together in one command. The length control MUST accept integers in `[1, 64]` and send `length` alone.
(Previously: no length.)

#### Scenario: Live send
- GIVEN the mode is euclidean
- WHEN pulses changes to 5
- THEN `setGenerationParams {pulses:5}` is dispatched without pressing Generate

#### Scenario: Range sent together
- GIVEN range 36..72
- WHEN Range Low is changed to 40
- THEN the command contains both `rangeLow:40` and `rangeHigh:72`

#### Scenario: Length sent live [V]
- GIVEN length 16
- WHEN the length control is changed to 24
- THEN `setGenerationParams {length:24}` is dispatched without pressing Generate

#### Scenario: Length bounds [V]
- GIVEN the length control
- WHEN driven to below 1 or above 64
- THEN the value sent is clamped to the range `[1, 64]`

### Requirement: Generation, Seed And Rhythm Controls

The UI MUST provide: Seed field (`setSeed`), Generate (`regenerate {randomize:false}`), Randomize (`regenerate {randomize:true}`), Mutate (`mutate`), Lock Seed, Rhythm mode (random|euclidean|probability), Pulses, Rotation, Chance % and Length. The Length control MUST show the value from the snapshot's `generationParams.length`.
(Previously: no Length control.)

#### Scenario: Generate and Randomize
- GIVEN Lock Seed off
- WHEN Generate then Randomize are pressed
- THEN `regenerate` is dispatched with `randomize:false` then `randomize:true`

#### Scenario: Mutate
- GIVEN a sequence
- WHEN Mutate is pressed
- THEN `mutate` is dispatched

#### Scenario: Rhythm fields reflect snapshot
- GIVEN a snapshot with mode `probability`, chance 0.5 and length 32
- WHEN rendered
- THEN mode, chance and length controls show those values

#### Scenario: Evolve updates UI via snapshot event
- GIVEN auto-evolve is running
- WHEN a `snapshot` event arrives with a new `mutationCount` and a different number of steps
- THEN the step display shows the new step count
