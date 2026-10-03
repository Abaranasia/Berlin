# Delta for UI Bridge

## MODIFIED Requirements

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

## ADDED Requirements

### Requirement: Snapshot presetNames Is Always Current

`snapshot().presetNames` MUST always equal the current contents of the preset directory, including files added, changed or removed outside the app, regardless of any internal caching.

#### Scenario: External add and remove are visible
- GIVEN a snapshot listed preset "A"
- WHEN a valid preset file "B" is added and "A" removed externally
- THEN the next snapshot's `presetNames` contains "B" and not "A"
