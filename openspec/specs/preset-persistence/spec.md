# Preset Persistence Specification

## Purpose

Named, browsable storage for a musician's sound: bundles the 11 live synth parameters, the current generation seed, the current BPM, the current sync division and sync mode, and the 8 delay/reverb/output-level effects fields into one saved unit, restorable later by name. All previously out-of-scope effects fields are now live-adjustable and persisted.

## Requirements

### Requirement: Old-Format Presets Default Missing Scale, Root, and Range Fields

Loading a preset file saved before this change (one that has no `scaleType`, `rootPitchClass`, `rangeLow`, or `rangeHigh` properties) MUST NOT fail and MUST NOT leave those fields uninitialized. `PresetManager`'s load path MUST default any missing one of these 4 fields to today's hardcoded values (`scaleType = minor`, `rootPitchClass = C`, `rangeLow = 36`, `rangeHigh = 72`) individually, per field — this is the same "older schema, missing field defaults" policy already applied to `SynthPatch` fields via `kDefaultPatch`, scoped to these 4 new fields.

#### Scenario: Loading a pre-change preset file defaults scale/root/range without error

- GIVEN a preset file written before this change, containing no `scaleType`/`rootPitchClass`/`rangeLow`/`rangeHigh` properties
- WHEN the user loads it
- THEN loading succeeds, and the live scale/root/range become Minor, C, `[36, 72]` — matching today's hardcoded pre-change behavior — with no error and no crash

#### Scenario: A preset saved with scale/root/range round-trips unchanged

- GIVEN a preset saved with `scaleType = phrygian`, `rootPitchClass = E`, `rangeLow = 43`, `rangeHigh = 67`
- WHEN the preset is loaded again
- THEN the recovered scale, root, and range exactly match the saved values

### Requirement: Preset Scope Is Exactly The 11 Live Parameters Plus Seed

A preset MUST capture: the 11 original live-adjustable synth parameters (waveform, cutoff, resonance, pulse width, attack, decay, sustain, release, LFO destination/rate/depth); the current generation seed; the 4 generation pitch fields (`scaleType`, `rootPitchClass`, `rangeLow`, `rangeHigh`); the current BPM; the current sync division and Sync/Free mode; and the 8 delay/reverb/output-level fields (`delayTimeSeconds`, `delayFeedback`, `delayMix`, `reverbRoomSize`, `reverbDamping`, `reverbWetLevel`, `reverbDryLevel`, `outputLevel`), all of which are now live-adjustable and MUST NOT remain pinned to `kDefaultPatch`. A preset MUST NOT capture the remaining `GenerationParams` fields (`mode`, `pulses`, `rotation`, `stepProbability`, `lockSeed`) - these stay unpersisted per the existing rule.
(Previously: scope was the 11 live parameters, seed, and the 4 pitch fields; the 8 effects fields were explicitly out of scope and pinned to `kDefaultPatch` because they had no UI.)

#### Scenario: Saving captures the 11 parameters, seed, scale/root/range, BPM, sync state, and effects fields
- GIVEN the user has set the 11 live parameters, a seed, a non-default scale/root/range, a non-default BPM, sync division/mode, and non-default delay/reverb values
- WHEN the user saves a preset
- THEN the saved unit contains exactly those values and no rhythm-mode fields (`mode`/`pulses`/`rotation`/`stepProbability`)

#### Scenario: Effects fields round-trip like any other live parameter
- GIVEN a preset saved with non-default delay/reverb values
- WHEN it is loaded
- THEN the loaded synth's delay/reverb values match the saved ones, not `kDefaultPatch`

### Requirement: Save Preset By Name

The system MUST let the user save the current 11 live parameters and seed under a user-supplied name, and MUST prompt for confirmation before overwriting a name that already exists — neither silent overwrite nor silent rejection is acceptable.

#### Scenario: Save under a new name
- GIVEN a name matching no existing preset
- WHEN the user saves
- THEN a new preset is created and appears in the browsable list

#### Scenario: Save under an existing name prompts before overwrite
- GIVEN a preset named "Lead" already exists
- WHEN the user saves another preset also named "Lead"
- THEN the user is prompted to confirm, and "Lead"'s stored content is unchanged until confirmed

#### Scenario: Declining the confirmation preserves the existing preset
- GIVEN the overwrite confirmation prompt is shown
- WHEN the user declines
- THEN the existing preset's stored content is unchanged

### Requirement: Browse Available Presets

The system MUST let the user list all saved presets by name, including when none exist.

#### Scenario: Saved presets appear in the browsable list
- GIVEN one or more presets have been saved
- WHEN the user opens the preset selector
- THEN every saved preset's name appears and is selectable for loading

#### Scenario: Empty preset store is browsable without error
- GIVEN no preset has ever been saved
- WHEN the user opens the preset selector
- THEN the list is empty and no error occurs

### Requirement: Load Preset By Name

Loading a preset MUST restore its 11 live parameters, seed, 4 generation pitch fields, BPM, sync division/mode, and 8 delay/reverb/output-level fields as the current live state, and update any displayed control values to match - no stale UI vs. live audio drift.
(Previously: restored the 11 live parameters, seed, and 4 pitch fields; BPM and effects fields did not yet exist as persisted or live-adjustable fields.)

#### Scenario: Loading restores all persisted fields and displayed controls
- GIVEN a saved preset with known values, including non-default scale/root/range, BPM, sync state, and effects values
- WHEN the user loads it
- THEN the 11 live parameters, seed, scale/root/range, BPM, sync division/mode, and effects fields all match the saved values, and displayed controls match the loaded values

### Requirement: Loading Applies The Saved Seed Even When Lock Seed Is Enabled

Loading a preset MUST apply its saved seed and regenerate the sequence even if Lock Seed is currently enabled. Loading is an explicit user action distinct from Randomize/New Seed, which Lock Seed continues to correctly suppress (see `generation-live-control`).

#### Scenario: Load overrides Lock Seed
- GIVEN Lock Seed is enabled
- WHEN the user loads a preset with a different saved seed
- THEN the current seed becomes the preset's saved seed and a new `Sequence` is generated from it

### Requirement: Old-Format Presets Default Missing BPM, Sync Division, Sync Mode, and Effects Fields

Loading a preset file saved before this change (one with no `bpm`, sync-division, sync-mode, or effects-field properties) MUST NOT fail and MUST NOT leave those fields uninitialized. `PresetManager`'s load path MUST default each missing field individually: `bpm` to 120, sync mode to Free, sync division to quarter (unused while Free), and each of the 8 effects fields to its `kDefaultPatch` value - matching today's pre-change pinned behavior.

#### Scenario: Loading a pre-change preset file defaults BPM, sync, and effects fields without error
- GIVEN a preset file written before this change, containing no `bpm`/sync/effects properties
- WHEN the user loads it
- THEN loading succeeds, BPM becomes 120, sync mode becomes Free, and every effects field equals its `kDefaultPatch` value, with no error and no crash

#### Scenario: A preset saved with BPM and effects fields round-trips unchanged
- GIVEN a preset saved with BPM = 95, Sync mode active on the dotted-eighth division, and non-default delay/reverb values
- WHEN the preset is loaded again
- THEN the recovered BPM, sync division/mode, and effects values exactly match the saved values

### Requirement: Loading While Playing Restarts Playback From Step 1

Loading a preset while the sequence is playing MUST restart playback from step 1 of the newly generated sequence, reusing the existing Generate/Randomize regeneration-and-handoff behavior.

#### Scenario: Load mid-playback restarts from step 1
- GIVEN audio is playing partway through the current `Sequence`
- WHEN the user loads a preset
- THEN playback restarts from step 1 of the `Sequence` generated from the preset's seed, with no hung note, dropout, assert, or crash

### Requirement: Persisted Format Is Human-Readable And Version-Tagged

Presets MUST be persisted as one versioned `juce::ValueTree` serialized to human-readable XML text, satisfying both the `juce-app-dev` skill's ValueTree convention and the proposal's human-readable requirement. Every property MUST be written as an explicitly formatted string, never a native `double`/`int64` value directly stringified by a generic serializer — this is a correctness requirement, not a style preference: the seed's full `int64` range (including its extremes) and floating-point parameters MUST round-trip exactly, with no precision loss and no parse failure at any valid value.

The persisted format MUST carry a schema version. A preset whose version is newer than the app supports MUST be rejected (its future fields cannot be safely guessed). A preset whose version is older MUST still be accepted, with any fields the older version didn't have filled from `kDefaultPatch`.

#### Scenario: A saved preset file is human-readable
- GIVEN a preset has been saved to disk
- WHEN the persisted file is opened in a text editor
- THEN its structure and values are readable as text

#### Scenario: Seed round-trips exactly at its extremes
- GIVEN a preset saved with the seed at its minimum or maximum representable value
- WHEN the preset is loaded again
- THEN the recovered seed is bit-for-bit identical to the saved value, with no parse failure or overflow

#### Scenario: Floating-point parameters round-trip without precision loss
- GIVEN a preset saved with a parameter value that is not exactly representable in a small number of decimal digits
- WHEN the preset is loaded again
- THEN the recovered value is identical to the original, not silently rounded to fewer significant digits

#### Scenario: A preset from a newer, unsupported schema version is rejected
- GIVEN a preset file whose schema version is newer than this build supports
- WHEN the user attempts to load it
- THEN loading fails cleanly, the synth is left untouched, and the user is told why

#### Scenario: A preset from an older schema version still loads
- GIVEN a preset file whose schema version is older than the current one, missing a field the current version has
- WHEN the user loads it
- THEN it loads successfully, with the missing field taking its value from `kDefaultPatch`

### Requirement: Malformed Preset Files Are Handled Safely, With Different Policies For Structural And Continuous-Value Defects

Loading a preset file MUST NOT crash, MUST NOT trigger an assertion failure, and MUST NOT let an out-of-range value reach the live synth atomics, regardless of what the file contains. Two distinct failure classes are handled differently:

- **Structural defects** (not valid XML, wrong root element, a missing required section or attribute, an unrecognized enum name for waveform or LFO destination, or a missing/unparseable schema version) MUST cause the whole preset to be rejected: nothing is applied, the synth is left exactly as it was, and the user is told why. Partial application would leave the instrument in a state attributed to a preset name that doesn't actually represent it.
- **A continuous numeric value outside its documented range** (e.g. a cutoff frequency beyond the slider's bounds) MUST be clamped to that parameter's documented range and the preset still loads successfully. Rejecting the whole preset over one out-of-range continuous value would be stricter than the live UI itself, where a slider physically cannot leave its range.

#### Scenario: A structurally invalid preset file is rejected outright
- GIVEN a preset file that is not valid XML, or is missing a required section, or names an unrecognized waveform/LFO destination
- WHEN the user attempts to load it
- THEN nothing is applied, the synth and sequence are left unchanged, and the user is told loading failed

#### Scenario: An out-of-range continuous value is clamped, not rejected
- GIVEN a preset file whose stored cutoff frequency (or any other continuous parameter) is numerically valid but outside its documented range
- WHEN the user loads it
- THEN the preset loads successfully with that value clamped to its documented range, and no out-of-range value ever reaches the live synth atomics
