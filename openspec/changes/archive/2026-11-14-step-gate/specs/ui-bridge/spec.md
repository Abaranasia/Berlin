# Delta for UI Bridge

Built on top of the pending `step-velocity` delta. Archive `step-velocity` first, then this change.

## MODIFIED Requirements

### Requirement: Fixed Error Tokens

`error` MUST be one of these machine-matchable tokens, where `<name>` is the offending command or argument/field name: `unknown command: <name>`, `missing arg: <name>`, `invalid type: <name>`, `non-finite: <name>`, `unknown field: <name>`, `invalid enum: <name>`, `busy`, `exists`, `path not absolute`, `step-out-of-range`, `note-out-of-scale`, `velocity-out-of-range`, `gate-out-of-range`, a `PresetResult` enum name (`nameInvalid`, `directoryUnavailable`, `writeFailed`, `fileNotFound`, `parseFailed`, `unsupportedVersion`), or a `MidiFileWriteResult` enum name (`invalidTimeline`, `pathUnavailable`, `writeFailed`).
(Previously: no `gate-out-of-range`.)

#### Scenario: Error token names the offending argument
- GIVEN any state
- WHEN `dispatch("setBpm", {})` is called
- THEN `error` is exactly `"missing arg: bpm"`

#### Scenario: Velocity token exists
- GIVEN a valid state
- WHEN `setStep` is dispatched with `velocity: 0`
- THEN `error` is exactly `"velocity-out-of-range"`

#### Scenario: Gate token exists
- GIVEN a valid state
- WHEN `setStep` is dispatched with `gatePercent: 0`
- THEN `error` is exactly `"gate-out-of-range"`

### Requirement: setStep Command

`setStep {index: int, note: int, active: bool, skipped?: bool, velocity?: int, gatePercent?: int}` MUST set one step to the absolute note and `active` flag given; when `skipped` is present it sets the skipped flag, when `velocity` is present it sets the step's velocity, and when `gatePercent` is present it sets the step's gate length. When `skipped`, `velocity` or `gatePercent` is absent, that field of the step MUST be unchanged. No new bridge command is introduced (the command count and `kCommands` stay unchanged). Validation is authoritative in the engine, in this order: `index` outside `[0, size)` (max 64) returns `"step-out-of-range"`; `note` not in the snapshot's `scaleNotes` returns `"note-out-of-scale"`, unless it equals the step's current note; an integer `velocity` outside `[1, 127]` returns `"velocity-out-of-range"`; an integer `gatePercent` outside `[1, 100]` returns `"gate-out-of-range"` (never clamped); a publish or state restore still pending returns `"busy"`. A missing required arg, wrong type (including a non-boolean `skipped`, a non-integer `velocity` and a non-integer `gatePercent`) or non-finite number (including a non-finite `velocity` or `gatePercent`) MUST return the existing `missing arg: <name>`, `invalid type: <name>` or `non-finite: <name>` token. On any error no state changes (including `edited`). On success the result is `{ok:true, error:"", snapshot}`. Relative deltas and toggles are not supported.
(Previously: `{index, note, active, skipped?, velocity?}`; no `gatePercent` and no `gate-out-of-range`.)

#### Scenario: Valid edit succeeds [T]
- GIVEN a valid note in `scaleNotes` and no pending publish
- WHEN `dispatch("setStep", {index: 2, note: 50, active: true})` is called
- THEN `ok` is true and `snapshot.steps[2]` is `{note: 50, active: true, skipped: false, velocity: <its previous velocity>, gatePercent: <its previous gatePercent>}` and `snapshot.edited` is true

#### Scenario: Skip edit succeeds [T]
- GIVEN a step that is not skipped
- WHEN `dispatch("setStep", {index: 2, note: <its note>, active: true, skipped: true})` is called
- THEN `ok` is true and `snapshot.steps[2].skipped` is true

#### Scenario: Omitted skipped keeps the flag [T]
- GIVEN step 2 is skipped
- WHEN `dispatch("setStep", {index: 2, note: 50, active: true})` is called
- THEN `snapshot.steps[2].skipped` is still true

#### Scenario: Velocity edit succeeds [T]
- GIVEN step 2 has velocity 100
- WHEN `dispatch("setStep", {index: 2, note: <its note>, active: true, velocity: 127})` is called
- THEN `ok` is true and `snapshot.steps[2].velocity` is 127

#### Scenario: Omitted velocity keeps the value [T]
- GIVEN step 2 has velocity 40
- WHEN `dispatch("setStep", {index: 2, note: 50, active: true})` is called
- THEN `snapshot.steps[2].velocity` is still 40

#### Scenario: Out-of-range velocity is rejected [T]
- GIVEN step 2 has velocity 100
- WHEN `setStep` is dispatched with `velocity` 0, separately 128
- THEN `error` is `"velocity-out-of-range"`, no `snapshot` key, and state is unchanged

#### Scenario: Malformed velocity [T]
- GIVEN any state
- WHEN `setStep` is dispatched with `velocity: "loud"`, separately `velocity: 100.5`, separately `velocity: Infinity`
- THEN `error` is `"invalid type: velocity"`, `"invalid type: velocity"` and `"non-finite: velocity"` respectively, with state unchanged

#### Scenario: Gate edit succeeds [T]
- GIVEN step 2 has `gatePercent` 100
- WHEN `dispatch("setStep", {index: 2, note: <its note>, active: true, gatePercent: 50})` is called
- THEN `ok` is true and `snapshot.steps[2].gatePercent` is 50 and `snapshot.edited` is true

#### Scenario: Omitted gatePercent keeps the value [T]
- GIVEN step 2 has `gatePercent` 25
- WHEN `dispatch("setStep", {index: 2, note: 50, active: true})` is called
- THEN `snapshot.steps[2].gatePercent` is still 25

#### Scenario: Out-of-range gate is rejected [T]
- GIVEN step 2 has `gatePercent` 100
- WHEN `setStep` is dispatched with `gatePercent` 0, separately 101
- THEN `error` is `"gate-out-of-range"`, no `snapshot` key, and state is unchanged

#### Scenario: Malformed gatePercent [T]
- GIVEN any state
- WHEN `setStep` is dispatched with `gatePercent: "short"`, separately `gatePercent: 50.5`, separately `gatePercent: Infinity`
- THEN `error` is `"invalid type: gatePercent"`, `"invalid type: gatePercent"` and `"non-finite: gatePercent"` respectively, with state unchanged

#### Scenario: Validation order [T]
- GIVEN a publish is pending
- WHEN `setStep` is dispatched with an out-of-range index, velocity 0 and gatePercent 0; separately a valid index, out-of-scale note, velocity 0 and gatePercent 0; separately a valid index and note with velocity 0 and gatePercent 0; separately a valid index, note and velocity with gatePercent 0
- THEN the errors are `step-out-of-range`, `note-out-of-scale`, `velocity-out-of-range` and `gate-out-of-range`, never `busy`

#### Scenario: Index beyond 16 on a longer sequence [T]
- GIVEN a 32-step sequence
- WHEN `dispatch("setStep", {index: 20, note: 50, active: true})` is called
- THEN `ok` is true

#### Scenario: Out-of-range index [T]
- GIVEN a 16-step sequence
- WHEN `dispatch("setStep", {index: 16, note: 50, active: true})` is called
- THEN `error` is `"step-out-of-range"` and state is unchanged

#### Scenario: Out-of-scale note [T]
- GIVEN a note not in `scaleNotes`
- WHEN `setStep` is dispatched with it
- THEN `error` is `"note-out-of-scale"` and state is unchanged

#### Scenario: Busy [T]
- GIVEN a published sequence not yet adopted
- WHEN `setStep` is dispatched
- THEN `error` is `"busy"`, no `snapshot` key, state unchanged

#### Scenario: Malformed args [T]
- GIVEN any state
- WHEN `setStep` is dispatched with `{index: 1, note: 50}`, separately `{index: "1", note: 50, active: true}`, and separately `{index: 1, note: 50, active: true, skipped: "yes"}`
- THEN `error` is `"missing arg: active"`, `"invalid type: index"` and `"invalid type: skipped"` respectively, with state unchanged

### Requirement: Encoding Conventions

`patch` field names MUST be the preset XML attribute names (`waveform`, `cutoffHz`, `resonance`, `pulseWidth`, `attack`, `decay`, `sustain`, `release`, `lfoRateHz`, `lfoDepth`, `lfoDestination`, `delayTimeSeconds`, `delayFeedback`, `delayMix`, `reverbRoomSize`, `reverbDamping`, `reverbWetLevel`, `reverbDryLevel`, `outputLevel`, `delaySynced`, `delayDivision`). `generationParams` field names MUST be `mode`, `pulses`, `rotation`, `stepProbability`, `lockSeed`, `scaleType`, `rootPitchClass`, `rangeLow`, `rangeHigh`, `length`. `length` MUST be a JSON number and finite out-of-range values MUST be clamped to `[1, 64]` (not rejected). Each entry of the snapshot `steps` array MUST be encoded as `{note, active, skipped, velocity, gatePercent}` with `skipped` a JSON boolean, `velocity` a JSON integer in `[1, 127]` and `gatePercent` a JSON integer in `[1, 100]`. Enum fields MUST be encoded as the preset XML table names: waveform `saw|square|pulse|triangle`; lfoDestination `pitch|cutoff|amplitude|pulseWidth`; scaleType `minor|major|dorian|phrygian|mixolydian|harmonicMinor`; delayDivision `half|quarter|dottedEighth|eighth|eighthTriplet|sixteenth`. Generation mode MUST be encoded as `"random" | "euclidean" | "probability"`. Seed MUST be encoded as a decimal string in both directions, since int64 exceeds JS safe-integer range. All other numbers MUST be encoded as JSON numbers, and booleans (including `delaySynced` and `lockSeed`) as JSON booleans.
(Previously: step entries were `{note, active, skipped, velocity}`.)

#### Scenario: Seed round-trips as a string
- GIVEN a 19-digit seed value
- WHEN `dispatch("setSeed", {seed: "9223372036854770000"})` is called
- THEN `ok` is true and the snapshot's `seed` field is the same decimal string, not a JSON number

#### Scenario: Waveform round-trips using the preset XML name
- GIVEN `dispatch("setPatch", {waveform: "saw"})` has succeeded
- WHEN the snapshot is read
- THEN `patch.waveform` equals `"saw"`, matching the name used in preset XML

#### Scenario: Length round-trips and clamps [T]
- GIVEN `dispatch("setGenerationParams", {length: 24})` has succeeded
- WHEN the snapshot is read
- THEN `generationParams.length` is 24
- AND `{length: 100}` yields 64 and `{length: 0}` yields 1 with `ok:true`

#### Scenario: Unknown generation field still rejected [T]
- GIVEN any state
- WHEN `dispatch("setGenerationParams", {lenght: 8})` is called
- THEN `error` is `"unknown field: lenght"` and state is unchanged

#### Scenario: encodeSteps carries velocity [T]
- GIVEN a sequence whose steps have velocities 40, 100 and 127
- WHEN the snapshot is read
- THEN `steps` entries carry those velocities, `velocity` a JSON number

#### Scenario: encodeSteps carries gatePercent [T]
- GIVEN a sequence whose steps have `gatePercent` 25, 75 and 100
- WHEN the snapshot is read
- THEN `steps` entries are `{note, active, skipped, velocity, gatePercent}` with those values, `gatePercent` a JSON number

### Requirement: Snapshot Contents

`snapshot()` MUST contain these top-level keys: `patch` (all 21 patch fields including `outputLevel`), `generationParams` (all 10 generation params including `length`), `seed`, `bpm`, `playing`, `playheadStep`, `loopCount`, `synthEnabled`, `effectsEnabled`, `masterLevel`, `autoEvolveEnabled`, `autoEvolveRate`, `mutationCount`, `edited` (bool), `scaleNotes` (int array: ascending in-scale MIDI notes within `[rangeLow, rangeHigh]` for the current scale and root), `steps` (one `{note, active, skipped, velocity, gatePercent}` entry per step of the current sequence, 1 to 64 entries; the sequence length is carried implicitly as `steps.length`), and `presetNames`. `playheadStep` MUST be a valid index into `steps` or the documented "no current step" value when every step is skipped.
(Previously: `steps` entries were `{note, active, skipped, velocity}`.)

#### Scenario: Snapshot includes every documented field
- GIVEN any valid engine state
- WHEN `snapshot()` is called
- THEN the result contains all of: patch, generation params (including `length`), seed, bpm, playing, playheadStep, loopCount, synthEnabled, effectsEnabled, masterLevel, autoEvolveEnabled, autoEvolveRate, mutationCount, edited, scaleNotes, steps, presetNames

#### Scenario: Steps length follows the sequence [T]
- GIVEN a generated 8-step sequence, and separately a mutated 40-step sequence
- WHEN the snapshot is read
- THEN `steps.length` is 8 and 40 respectively, each entry carrying `note`, `active`, `skipped`, `velocity` and `gatePercent`

#### Scenario: scaleNotes tracks generation params [T]
- GIVEN `setGenerationParams {scaleType:"major", rootPitchClass:0, rangeLow:48, rangeHigh:60}` succeeded
- WHEN the snapshot is read
- THEN `scaleNotes` is `[48,50,52,53,55,57,59,60]`

#### Scenario: Generated steps report velocity 100 [T]
- GIVEN a freshly generated sequence
- WHEN the snapshot is read
- THEN every step's `velocity` is 100

#### Scenario: Generated steps report gatePercent 100 [T]
- GIVEN a freshly generated sequence
- WHEN the snapshot is read
- THEN every step's `gatePercent` is 100

### Requirement: Message-Thread-Only Dispatch

All 16 commands MUST be dispatched and handled on the message thread only. No bridge command MUST be callable from, or have any effect on, the audio thread directly. The skip, length, velocity and gate-length features add no command, so the count remains 16.
(Previously: stated only that skip, length and velocity add no command.)

#### Scenario: Dispatch is a message-thread operation
- GIVEN the bridge's implementation
- WHEN its call path is inspected during code review
- THEN `dispatch` and all command handlers execute entirely on the message thread, with no direct audio-thread entry point

#### Scenario: Command surface is unchanged [T]
- GIVEN the bridge command list
- WHEN inspected
- THEN it contains exactly the same 16 commands as before this change
