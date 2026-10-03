# UI Bridge Specification

## Purpose

A single validated command surface (`dispatch(command, args)` / `snapshot()`) that lets a UI (present or future, e.g. a WebView) drive the engine without touching engine internals directly. Exactly 15 commands. All dispatch runs on the message thread only.

## Requirements

### Requirement: Uniform Result Shape

Every `dispatch` call MUST return an object with `ok: bool` and `error: string`. When `ok` is true, `error` MUST be `""` and the result MUST also contain `snapshot`, the fresh post-command snapshot. When `ok` is false, `error` MUST be a non-empty error token and the result MUST NOT contain a `snapshot` key.

#### Scenario: Successful command returns ok and a fresh snapshot
- GIVEN the engine is idle with bpm 120
- WHEN `dispatch("setBpm", {bpm: 140})` is called
- THEN the result is `{ok: true, error: "", snapshot: {...bpm: 140, ...}}`

#### Scenario: Failed command returns ok:false with an error token and no snapshot
- GIVEN any valid engine state
- WHEN `dispatch("nonexistentCommand", {})` is called
- THEN the result is `{ok: false, error: "unknown command: nonexistentCommand"}` with no `snapshot` key

### Requirement: Fixed Error Tokens

`error` MUST be one of these machine-matchable tokens, where `<name>` is the offending command or argument/field name: `unknown command: <name>`, `missing arg: <name>`, `invalid type: <name>`, `non-finite: <name>`, `unknown field: <name>`, `invalid enum: <name>`, `busy`, `exists`, `path not absolute`, a `PresetResult` enum name (`nameInvalid`, `directoryUnavailable`, `writeFailed`, `fileNotFound`, `parseFailed`, `unsupportedVersion`), or a `MidiFileWriteResult` enum name (`invalidTimeline`, `pathUnavailable`, `writeFailed`).

#### Scenario: Error token names the offending argument
- GIVEN any state
- WHEN `dispatch("setBpm", {})` is called
- THEN `error` is exactly `"missing arg: bpm"`

### Requirement: Rejected Input Leaves State Unchanged

Dispatch MUST return `ok:false` with the matching error token and MUST NOT change any engine state for: an unknown command name; a missing required argument; an argument of the wrong type; a `NaN` or infinite number; an unknown field name inside `setPatch`/`setGenerationParams` (protocol typos are rejected, not silently ignored); an invalid enum name.

#### Scenario: Unknown command leaves bpm unchanged
- GIVEN bpm is 120
- WHEN `dispatch("doesNotExist", {})` is called
- THEN `ok` is false, `error` is `"unknown command: doesNotExist"`, and a subsequent snapshot still shows bpm 120

#### Scenario: Missing required argument is rejected
- GIVEN any state
- WHEN `dispatch("setBpm", {})` is called (missing `bpm`)
- THEN `ok` is false, `error` is `"missing arg: bpm"`, and bpm is unchanged

#### Scenario: Wrong argument type is rejected
- GIVEN any state
- WHEN `dispatch("setBpm", {bpm: "fast"})` is called
- THEN `ok` is false, `error` is `"invalid type: bpm"`, and bpm is unchanged

#### Scenario: NaN/infinite number is rejected
- GIVEN any state
- WHEN `dispatch("setMasterLevel", {level: Infinity})` is called
- THEN `ok` is false, `error` is `"non-finite: level"`, and master level is unchanged

#### Scenario: Unknown field name inside setPatch is rejected, not ignored
- GIVEN a valid patch state
- WHEN `dispatch("setPatch", {oscilatorWaveform: "square"})` is called (typo'd field name)
- THEN `ok` is false, `error` is `"unknown field: oscilatorWaveform"`, and the patch is unchanged

#### Scenario: Invalid enum name is rejected
- GIVEN a valid patch state
- WHEN `dispatch("setPatch", {waveform: "sawtooth"})` is called (not a preset XML waveform name; the valid name is `"saw"`)
- THEN `ok` is false, `error` is `"invalid enum: waveform"`, and the patch is unchanged

### Requirement: Finite Out-Of-Range Numbers Are Clamped, Not Rejected

A finite numeric value outside an existing bound (e.g. master level, BPM, auto-evolve rate, any `SynthPatch` or generation field) MUST be clamped to that bound and accepted (`ok:true`), distinct from `NaN`/infinite values which are rejected per the prior requirement.

#### Scenario: Out-of-range finite level clamps instead of failing
- GIVEN master level is 0.8
- WHEN `dispatch("setMasterLevel", {level: 2.5})` is called
- THEN `ok` is true, the snapshot shows master level clamped to 1.0

### Requirement: Encoding Conventions

`patch` field names MUST be the preset XML attribute names (`waveform`, `cutoffHz`, `resonance`, `pulseWidth`, `attack`, `decay`, `sustain`, `release`, `lfoRateHz`, `lfoDepth`, `lfoDestination`, `delayTimeSeconds`, `delayFeedback`, `delayMix`, `reverbRoomSize`, `reverbDamping`, `reverbWetLevel`, `reverbDryLevel`, `outputLevel`, `delaySynced`, `delayDivision`). `generationParams` field names MUST be `mode`, `pulses`, `rotation`, `stepProbability`, `lockSeed`, `scaleType`, `rootPitchClass`, `rangeLow`, `rangeHigh`. Enum fields MUST be encoded as the preset XML table names: waveform `saw|square|pulse|triangle`; lfoDestination `pitch|cutoff|amplitude|pulseWidth`; scaleType `minor|major|dorian|phrygian|mixolydian|harmonicMinor`; delayDivision `half|quarter|dottedEighth|eighth|eighthTriplet|sixteenth`. Generation mode MUST be encoded as `"random" | "euclidean" | "probability"`. Seed MUST be encoded as a decimal string in both directions (request and snapshot), since int64 exceeds JS safe-integer range. All other numbers MUST be encoded as JSON numbers, and booleans (including `delaySynced` and `lockSeed`) as JSON booleans.

#### Scenario: Seed round-trips as a string
- GIVEN a 19-digit seed value
- WHEN `dispatch("setSeed", {seed: "9223372036854770000"})` is called
- THEN `ok` is true and the snapshot's `seed` field is the same decimal string, not a JSON number

#### Scenario: Waveform round-trips using the preset XML name
- GIVEN `dispatch("setPatch", {waveform: "saw"})` has succeeded
- WHEN the snapshot is read
- THEN `patch.waveform` equals `"saw"`, matching the name used in preset XML

### Requirement: setPatch And setGenerationParams Are Partial Merges

`setPatch` and `setGenerationParams` MUST merge only the supplied fields into current state; any field omitted from the args object MUST keep its current value. `setGenerationParams` commits immediately (no staging).

#### Scenario: Omitted fields are preserved
- GIVEN a patch with `cutoffHz: 800` and `resonance: 2.0`
- WHEN `dispatch("setPatch", {cutoffHz: 1200})` is called (resonance omitted)
- THEN the snapshot shows `cutoffHz: 1200` and `resonance` still 2.0

### Requirement: Synced Delay Time Is Engine-Owned

When delay sync is on, `delayTimeSeconds` MUST be derived from BPM and the selected division whenever BPM or the patch changes via any dispatch path, so every snapshot reflects a consistent, current value rather than a stale one.

#### Scenario: BPM change recomputes synced delay time
- GIVEN `delaySynced: true` with `delayDivision: "quarter"` and bpm 120 (`delayTimeSeconds` 0.5)
- WHEN `dispatch("setBpm", {bpm: 150})` is called
- THEN the snapshot's `delayTimeSeconds` is 0.4 (one quarter note at 150 bpm), not the 0.5 computed at 120

### Requirement: Transport And Generation Commands

`setPlaying {playing: bool}` requests transport state (adopted on the audio thread; see `playback-transport`). `setBpm {bpm}`, `setSeed {seed}` (decimal string), `regenerate {randomize: bool}`, and `mutate {}` act on sequencing/generation state. `regenerate {randomize: false}` regenerates with the current seed. `regenerate {randomize: true}` draws a new seed only when `generationParams.lockSeed` is false; when `lockSeed` is true the current seed is kept. While a previously published sequence has not yet been adopted by the audio thread, `regenerate` and `mutate` MUST return `ok:false, error:"busy"` and change no state; in particular a busy `regenerate {randomize: true}` MUST NOT draw a new seed.

#### Scenario: regenerate false keeps the seed
- GIVEN seed "42"
- WHEN `dispatch("regenerate", {randomize: false})` is called
- THEN `ok` is true and the snapshot's seed is still "42"

#### Scenario: regenerate true changes the seed when lockSeed is false
- GIVEN seed "42" and `generationParams.lockSeed` false
- WHEN `dispatch("regenerate", {randomize: true})` is called
- THEN `ok` is true and the snapshot's seed differs from "42"

#### Scenario: regenerate true keeps the seed when lockSeed is true
- GIVEN seed "42" and `dispatch("setGenerationParams", {lockSeed: true})` has succeeded
- WHEN `dispatch("regenerate", {randomize: true})` is called
- THEN `ok` is true and the snapshot's seed is still "42"

#### Scenario: regenerate while busy fails without state change
- GIVEN a sequence was published by a prior `regenerate` and no audio block has adopted it yet
- WHEN `dispatch("regenerate", {randomize: false})` is called
- THEN `ok` is false, `error` is `"busy"`, and the sequence is unchanged

#### Scenario: Busy randomize leaves seed and snapshot unchanged
- GIVEN a sequence was published by a prior `regenerate` and no audio block has adopted it yet, and `lockSeed` is false
- WHEN `dispatch("regenerate", {randomize: true})` is called
- THEN `ok` is false, `error` is `"busy"`, and a snapshot taken afterwards (seed included) is identical to one taken before the call

### Requirement: Toggle And Level Commands

`setSynthEnabled {enabled}`, `setEffectsEnabled {enabled}`, `setMasterLevel {level}`, `setAutoEvolveEnabled {enabled}`, and `setAutoEvolveRate {rate}` each accept one argument of the documented type and clamp numeric values to existing bounds.

#### Scenario: Wrong-type toggle argument is rejected
- GIVEN synth is enabled
- WHEN `dispatch("setSynthEnabled", {enabled: "yes"})` is called (string, not boolean)
- THEN `ok` is false, `error` is `"invalid type: enabled"`, and the enabled state is unchanged

### Requirement: Preset And Export Commands

`savePreset {name, overwrite}` MUST return `ok:false, error:"exists"` and write nothing when a preset with `name` already exists and `overwrite` is false. `loadPreset {name}` MUST return `ok:false` with the `PresetResult` name (e.g. `fileNotFound`) when `name` does not resolve to an existing preset. `loadPreset` MUST return `ok:false, error:"busy"` (exactly that token, from the new `PresetResult::busy`) when a previously published sequence is still pending adoption by the audio thread; this check MUST happen before the preset is resolved or anything is applied, so on `busy` the current state (patch, seed, bpm, generation params, sequence) is unchanged. `loadPreset` MUST also return `ok:false, error:"busy"` if the post-load `regenerate(false)` reports failure. `exportMidi {path}` MUST require an absolute `path`, returning `ok:false, error:"path not absolute"` for a relative one, and MUST return `ok:false` with the `MidiFileWriteResult` name on a write failure.
(Previously: `loadPreset` reported ok even when the post-load regeneration failed or was busy.)

#### Scenario: savePreset without overwrite on an existing name fails cleanly
- GIVEN a saved preset named "Lead A"
- WHEN `dispatch("savePreset", {name: "Lead A", overwrite: false})` is called
- THEN `ok` is false, `error` is `"exists"`, and no file is written

#### Scenario: savePreset with overwrite true replaces the existing preset
- GIVEN a saved preset named "Lead A"
- WHEN `dispatch("savePreset", {name: "Lead A", overwrite: true})` is called
- THEN `ok` is true and the preset is replaced

#### Scenario: loadPreset on a missing name fails with a reason
- GIVEN no preset named "Ghost" exists
- WHEN `dispatch("loadPreset", {name: "Ghost"})` is called
- THEN `ok` is false, `error` is `"fileNotFound"`, and current state is unchanged

#### Scenario: loadPreset while regeneration is busy
- GIVEN a valid preset "Lead A" and a previously published sequence not yet adopted by the audio thread
- WHEN `dispatch("loadPreset", {name: "Lead A"})` is called
- THEN `ok` is false, `error` is exactly `"busy"`, no `snapshot` key is present, and the current state (patch, seed, bpm, generation params) is unchanged

#### Scenario: loadPreset success is unchanged
- GIVEN a valid preset and no pending sequence
- WHEN `dispatch("loadPreset", {name: "Lead A"})` is called
- THEN `ok` is true with a fresh snapshot

#### Scenario: exportMidi rejects a relative path
- GIVEN any valid state
- WHEN `dispatch("exportMidi", {path: "output.mid"})` is called (relative, not absolute)
- THEN `ok` is false, `error` is `"path not absolute"`, and no file is written

### Requirement: Snapshot Contents

`snapshot()` MUST contain these top-level keys: `patch` (all 21 patch fields including `outputLevel`), `generationParams` (all 9 generation params), `seed`, `bpm`, `playing`, `playheadStep`, `loopCount`, `synthEnabled`, `effectsEnabled`, `masterLevel`, `autoEvolveEnabled`, `autoEvolveRate`, `mutationCount`, `steps` (16 entries of `{note, active}`), and `presetNames`.

#### Scenario: Snapshot includes every documented field
- GIVEN any valid engine state
- WHEN `snapshot()` is called
- THEN the result contains all of: patch, generation params, seed, bpm, playing, playheadStep, loopCount, synthEnabled, effectsEnabled, masterLevel, autoEvolveEnabled, autoEvolveRate, mutationCount, 16-entry steps, presetNames

### Requirement: Snapshot presetNames Is Always Current

`snapshot().presetNames` MUST always equal the current contents of the preset directory, including files added, changed or removed outside the app, regardless of any internal caching.

#### Scenario: External add and remove are visible
- GIVEN a snapshot listed preset "A"
- WHEN a valid preset file "B" is added and "A" removed externally
- THEN the next snapshot's `presetNames` contains "B" and not "A"

### Requirement: Message-Thread-Only Dispatch

All 15 commands MUST be dispatched and handled on the message thread only. No bridge command MUST be callable from, or have any effect on, the audio thread directly.

#### Scenario: Dispatch is a message-thread operation
- GIVEN the bridge's implementation
- WHEN its call path is inspected during code review
- THEN `dispatch` and all command handlers execute entirely on the message thread, with no direct audio-thread entry point
