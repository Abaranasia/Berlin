# Delta for Preset Persistence

## MODIFIED Requirements

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

### Requirement: Load Preset By Name

Loading a preset MUST restore its 11 live parameters, seed, 4 generation pitch fields, BPM, sync division/mode, and 8 delay/reverb/output-level fields as the current live state, and update any displayed control values to match - no stale UI vs. live audio drift.
(Previously: restored the 11 live parameters, seed, and 4 pitch fields; BPM and effects fields did not yet exist as persisted or live-adjustable fields.)

#### Scenario: Loading restores all persisted fields and displayed controls
- GIVEN a saved preset with known values, including non-default scale/root/range, BPM, sync state, and effects values
- WHEN the user loads it
- THEN the 11 live parameters, seed, scale/root/range, BPM, sync division/mode, and effects fields all match the saved values, and displayed controls match the loaded values

## ADDED Requirements

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

## Note for Archive (non-canonical)

The main spec's Purpose sentence - "The 8 non-live `SynthPatch` effects fields (delay/reverb/outputLevel) are out of scope and remain pinned to `kDefaultPatch`." - is now false and MUST be updated by hand at archive time alongside the Requirements merge; it is prose, not a delta-mergeable Requirement block.
