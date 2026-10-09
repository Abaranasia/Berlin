# Delta for UI Bridge

## ADDED Requirements

### Requirement: setStep Command

`setStep {index: int, note: int, active: bool}` MUST set one step to the absolute note and gate given. Validation is authoritative in the engine: `index` outside `[0, min(size, 16))` returns `ok:false, error:"step-out-of-range"`; `note` not in the snapshot's `scaleNotes` returns `"note-out-of-scale"`; a publish still pending adoption returns `"busy"`. A missing arg, wrong type or non-finite number MUST return the existing `missing arg: <name>`, `invalid type: <name>` or `non-finite: <name>` token. On any error no state changes (including `edited`). On success the result is `{ok:true, error:"", snapshot}`. Relative deltas and toggles are not supported.

#### Scenario: Valid edit succeeds [T]
- GIVEN a valid note in `scaleNotes` and no pending publish
- WHEN `dispatch("setStep", {index: 2, note: 50, active: true})` is called
- THEN `ok` is true and `snapshot.steps[2]` is `{note: 50, active: true}` and `snapshot.edited` is true

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
- WHEN `setStep` is dispatched with `{index: 1, note: 50}`, and separately `{index: "1", note: 50, active: true}`
- THEN `error` is `"missing arg: active"`, and `"invalid type: index"`, with state unchanged

## MODIFIED Requirements

### Requirement: Fixed Error Tokens

`error` MUST be one of these machine-matchable tokens, where `<name>` is the offending command or argument/field name: `unknown command: <name>`, `missing arg: <name>`, `invalid type: <name>`, `non-finite: <name>`, `unknown field: <name>`, `invalid enum: <name>`, `busy`, `exists`, `path not absolute`, `step-out-of-range`, `note-out-of-scale`, a `PresetResult` enum name (`nameInvalid`, `directoryUnavailable`, `writeFailed`, `fileNotFound`, `parseFailed`, `unsupportedVersion`), or a `MidiFileWriteResult` enum name (`invalidTimeline`, `pathUnavailable`, `writeFailed`).
(Previously: no `step-out-of-range` or `note-out-of-scale`.)

#### Scenario: Error token names the offending argument
- GIVEN any state
- WHEN `dispatch("setBpm", {})` is called
- THEN `error` is exactly `"missing arg: bpm"`

### Requirement: Snapshot Contents

`snapshot()` MUST contain these top-level keys: `patch` (all 21 patch fields including `outputLevel`), `generationParams` (all 9 generation params), `seed`, `bpm`, `playing`, `playheadStep`, `loopCount`, `synthEnabled`, `effectsEnabled`, `masterLevel`, `autoEvolveEnabled`, `autoEvolveRate`, `mutationCount`, `edited` (bool), `scaleNotes` (int array: ascending in-scale MIDI notes within `[rangeLow, rangeHigh]` for the current scale and root), `steps` (16 entries of `{note, active}`), and `presetNames`.
(Previously: no `edited` or `scaleNotes`.)

#### Scenario: Snapshot includes every documented field
- GIVEN any valid engine state
- WHEN `snapshot()` is called
- THEN the result contains all of: patch, generation params, seed, bpm, playing, playheadStep, loopCount, synthEnabled, effectsEnabled, masterLevel, autoEvolveEnabled, autoEvolveRate, mutationCount, edited, scaleNotes, 16-entry steps, presetNames

#### Scenario: scaleNotes tracks generation params [T]
- GIVEN `setGenerationParams {scaleType:"major", rootPitchClass:0, rangeLow:48, rangeHigh:60}` succeeded
- WHEN the snapshot is read
- THEN `scaleNotes` is `[48,50,52,53,55,57,59,60]`

### Requirement: Message-Thread-Only Dispatch

All 16 commands MUST be dispatched and handled on the message thread only. No bridge command MUST be callable from, or have any effect on, the audio thread directly.
(Previously: 15 commands.)

#### Scenario: Dispatch is a message-thread operation
- GIVEN the bridge's implementation
- WHEN its call path is inspected during code review
- THEN `dispatch` and all command handlers execute entirely on the message thread, with no direct audio-thread entry point

### Requirement: Standalone-Only Settings Functions

When the editor is created with a non-null standalone host, the bridge MUST register two additional native functions, `getAudioSettings` and `applyAudioSettings`; otherwise it MUST NOT register either. They are not `dispatch` commands: the 16-command `dispatch` surface, its result shape and its tokens are unchanged by them. Both MUST run on the message thread only and MUST NOT call back into JS after the editor is destroyed.

`getAudioSettings` MUST return the current setup and lists: audio device types and devices, sample rates and buffer sizes for the selected device, MIDI output ports (including a None entry), the selected port and the channel 1-16. It MUST NOT return output volume. It MAY take an optional `{ deviceType, device }` query; when given, the sample rates and buffer sizes MUST be those offered by that device, so the dialog can show options for a device that is selected but not yet applied. The None port MUST be represented by the id `""` and listed first as `{ id: "", name: "None" }`.

`applyAudioSettings(request)` MUST return `{ ok: true }` or `{ ok: false, error: <token> }` with the token one of `invalid-request`, `device-unavailable`, `sample-rate-unsupported`, `buffer-size-unsupported`, `midi-port-unavailable`, `busy`, applying transactionally as defined in `standalone-settings`. The request MUST NOT carry output level; level changes use `setMasterLevel`.
(Previously: "15-command" dispatch surface.)

#### Scenario: Registered only with a host [T]
- GIVEN an editor built with a non-null standalone host, and another built with null
- WHEN registered functions are listed
- THEN the first includes `getAudioSettings` and `applyAudioSettings`; the second includes neither

#### Scenario: Get returns lists and no volume [V]
- GIVEN the mocked host returns a settings payload
- WHEN `getAudioSettings` is called
- THEN the payload has devices, rates, buffer sizes, ports with None, selected port and channel, and no volume field

#### Scenario: Apply success shape [V]
- GIVEN a valid request
- WHEN `applyAudioSettings` resolves
- THEN the result is `{ ok: true }` with no `snapshot` key

#### Scenario: Apply failure shape [V]
- GIVEN an invalid request
- WHEN `applyAudioSettings` resolves with `{ ok: false, error: "invalid-request" }`
- THEN the JS wrapper exposes that token unchanged

#### Scenario: Dispatch surface is 16 commands [V]
- GIVEN the bridge command list
- WHEN inspected
- THEN it contains exactly the 15 prior commands plus `setStep`, and no settings command

#### Scenario: Callback after teardown [M]
- GIVEN an apply is in flight
- WHEN the editor is destroyed
- THEN no JS callback is made and no crash occurs
