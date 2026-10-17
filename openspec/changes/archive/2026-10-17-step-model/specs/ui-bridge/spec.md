# Delta for UI Bridge

## MODIFIED Requirements

### Requirement: setStep Command

`setStep {index: int, note: int, active: bool, skipped?: bool}` MUST set one step to the absolute note and gate given and, when `skipped` is present, to the given skipped flag; when `skipped` is absent the step's skipped flag MUST be unchanged. No new bridge command is introduced (the command count and `kCommands` stay unchanged). Validation is authoritative in the engine: `index` outside `[0, size)` (the current sequence size, max 64) returns `ok:false, error:"step-out-of-range"`; `note` not in the snapshot's `scaleNotes` returns `"note-out-of-scale"`, unless it equals the step's current note, which is always accepted; a publish still pending adoption returns `"busy"`. A missing required arg, wrong type (including a non-boolean `skipped`) or non-finite number MUST return the existing `missing arg: <name>`, `invalid type: <name>` or `non-finite: <name>` token. On any error no state changes (including `edited`). On success the result is `{ok:true, error:"", snapshot}`. Relative deltas and toggles are not supported.
(Previously: `{index, note, active}` only, with index range `[0, min(size, 16))`.)

#### Scenario: Valid edit succeeds [T]
- GIVEN a valid note in `scaleNotes` and no pending publish
- WHEN `dispatch("setStep", {index: 2, note: 50, active: true})` is called
- THEN `ok` is true and `snapshot.steps[2]` is `{note: 50, active: true, skipped: false}` and `snapshot.edited` is true

#### Scenario: Skip edit succeeds [T]
- GIVEN a step that is not skipped
- WHEN `dispatch("setStep", {index: 2, note: <its note>, active: true, skipped: true})` is called
- THEN `ok` is true and `snapshot.steps[2].skipped` is true

#### Scenario: Omitted skipped keeps the flag [T]
- GIVEN step 2 is skipped
- WHEN `dispatch("setStep", {index: 2, note: 50, active: true})` is called
- THEN `snapshot.steps[2].skipped` is still true

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

`patch` field names MUST be the preset XML attribute names (`waveform`, `cutoffHz`, `resonance`, `pulseWidth`, `attack`, `decay`, `sustain`, `release`, `lfoRateHz`, `lfoDepth`, `lfoDestination`, `delayTimeSeconds`, `delayFeedback`, `delayMix`, `reverbRoomSize`, `reverbDamping`, `reverbWetLevel`, `reverbDryLevel`, `outputLevel`, `delaySynced`, `delayDivision`). `generationParams` field names MUST be `mode`, `pulses`, `rotation`, `stepProbability`, `lockSeed`, `scaleType`, `rootPitchClass`, `rangeLow`, `rangeHigh`, `length`. `length` MUST be a JSON number and finite out-of-range values MUST be clamped to `[1, 64]` (not rejected). Each entry of the snapshot `steps` array MUST be encoded as `{note, active, skipped}` with `skipped` a JSON boolean. Enum fields MUST be encoded as the preset XML table names: waveform `saw|square|pulse|triangle`; lfoDestination `pitch|cutoff|amplitude|pulseWidth`; scaleType `minor|major|dorian|phrygian|mixolydian|harmonicMinor`; delayDivision `half|quarter|dottedEighth|eighth|eighthTriplet|sixteenth`. Generation mode MUST be encoded as `"random" | "euclidean" | "probability"`. Seed MUST be encoded as a decimal string in both directions, since int64 exceeds JS safe-integer range. All other numbers MUST be encoded as JSON numbers, and booleans (including `delaySynced` and `lockSeed`) as JSON booleans.
(Previously: `generationParams` had 9 fields without `length`; step entries were `{note, active}`.)

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

### Requirement: Snapshot Contents

`snapshot()` MUST contain these top-level keys: `patch` (all 21 patch fields including `outputLevel`), `generationParams` (all 10 generation params including `length`), `seed`, `bpm`, `playing`, `playheadStep`, `loopCount`, `synthEnabled`, `effectsEnabled`, `masterLevel`, `autoEvolveEnabled`, `autoEvolveRate`, `mutationCount`, `edited` (bool), `scaleNotes` (int array: ascending in-scale MIDI notes within `[rangeLow, rangeHigh]` for the current scale and root), `steps` (one `{note, active, skipped}` entry per step of the current sequence, 1 to 64 entries; the sequence length is carried implicitly as `steps.length`), and `presetNames`. `playheadStep` MUST be a valid index into `steps` or the documented "no current step" value when every step is skipped.
(Previously: `generationParams` had 9 fields, `steps` was 16 entries of `{note, active}`.)

#### Scenario: Snapshot includes every documented field
- GIVEN any valid engine state
- WHEN `snapshot()` is called
- THEN the result contains all of: patch, generation params (including `length`), seed, bpm, playing, playheadStep, loopCount, synthEnabled, effectsEnabled, masterLevel, autoEvolveEnabled, autoEvolveRate, mutationCount, edited, scaleNotes, steps, presetNames

#### Scenario: Steps length follows the sequence [T]
- GIVEN a generated 8-step sequence, and separately a mutated 40-step sequence
- WHEN the snapshot is read
- THEN `steps.length` is 8 and 40 respectively, each entry carrying `note`, `active` and `skipped`

#### Scenario: scaleNotes tracks generation params [T]
- GIVEN `setGenerationParams {scaleType:"major", rootPitchClass:0, rangeLow:48, rangeHigh:60}` succeeded
- WHEN the snapshot is read
- THEN `scaleNotes` is `[48,50,52,53,55,57,59,60]`

### Requirement: Message-Thread-Only Dispatch

All 16 commands MUST be dispatched and handled on the message thread only. No bridge command MUST be callable from, or have any effect on, the audio thread directly. The skip and length features add no command, so the count remains 16.
(Previously: unchanged count of 16, with no statement that skip and length add no command.)

#### Scenario: Dispatch is a message-thread operation
- GIVEN the bridge's implementation
- WHEN its call path is inspected during code review
- THEN `dispatch` and all command handlers execute entirely on the message thread, with no direct audio-thread entry point

#### Scenario: Command surface is unchanged [T]
- GIVEN the bridge command list
- WHEN inspected
- THEN it contains exactly the same 16 commands as before this change
