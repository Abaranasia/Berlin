# Preset Persistence Specification

## Purpose

Named, browsable storage for a musician's sound: bundles the 11 live synth parameters and the current generation seed into one saved unit, restorable later by name. The 8 non-live `SynthPatch` effects fields (delay/reverb/outputLevel) are out of scope and remain pinned to `kDefaultPatch`.

## Requirements

### Requirement: Preset Scope Is Exactly The 11 Live Parameters Plus Seed

A preset MUST capture exactly the 11 live-adjustable synth parameters (waveform, cutoff, resonance, pulse width, attack, decay, sustain, release, LFO destination/rate/depth) and the current generation seed. A preset MUST NOT capture or alter the 8 non-live `SynthPatch` effects fields, which remain pinned to `kDefaultPatch` regardless of any preset operation.

#### Scenario: Saving captures only the 11 live parameters and the seed
- GIVEN the user has set the 11 live parameters and a seed
- WHEN the user saves a preset
- THEN the saved unit contains exactly those values and no effects-field values

#### Scenario: Effects fields are unaffected by save or load
- GIVEN a preset has been saved and later loaded
- WHEN the loaded synth is inspected
- THEN the 8 effects fields still match `kDefaultPatch`

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

Loading a preset MUST restore its 11 live parameters and seed as the current live state and update any displayed control values to match — no stale UI vs. live audio drift.

#### Scenario: Loading restores all 11 parameters, the seed, and displayed controls
- GIVEN a saved preset with known values
- WHEN the user loads it
- THEN the 11 live parameters and the seed match the saved values, and displayed controls match the loaded values

### Requirement: Loading Applies The Saved Seed Even When Lock Seed Is Enabled

Loading a preset MUST apply its saved seed and regenerate the sequence even if Lock Seed is currently enabled. Loading is an explicit user action distinct from Randomize/New Seed, which Lock Seed continues to correctly suppress (see `generation-live-control`).

#### Scenario: Load overrides Lock Seed
- GIVEN Lock Seed is enabled
- WHEN the user loads a preset with a different saved seed
- THEN the current seed becomes the preset's saved seed and a new `Sequence` is generated from it

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
