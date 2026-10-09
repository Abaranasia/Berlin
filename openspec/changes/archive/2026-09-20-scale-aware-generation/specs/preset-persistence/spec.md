# Delta for Preset Persistence

## ADDED Requirements

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

## MODIFIED Requirements

### Requirement: Preset Scope Is Exactly The 11 Live Parameters Plus Seed

A preset MUST capture exactly the 11 live-adjustable synth parameters (waveform, cutoff, resonance, pulse width, attack, decay, sustain, release, LFO destination/rate/depth), the current generation seed, and the 4 generation pitch fields `scaleType`, `rootPitchClass`, `rangeLow`, `rangeHigh`. A preset MUST NOT capture or alter the 8 non-live `SynthPatch` effects fields, which remain pinned to `kDefaultPatch` regardless of any preset operation. A preset MUST NOT capture the remaining `GenerationParams` fields (`mode`, `pulses`, `rotation`, `stepProbability`, `lockSeed`) — these stay unpersisted per the existing rule; only `scaleType`/`rootPitchClass`/`rangeLow`/`rangeHigh` are a deliberate, scoped exception.
(Previously: scope was exactly the 11 live parameters plus seed, with no `GenerationParams` fields persisted at all.)

#### Scenario: Saving captures the 11 live parameters, the seed, and scale/root/range
- GIVEN the user has set the 11 live parameters, a seed, and a non-default scale/root/range
- WHEN the user saves a preset
- THEN the saved unit contains exactly those values, the scale/root/range, and no effects-field values, and no rhythm-mode fields (`mode`/`pulses`/`rotation`/`stepProbability`)

#### Scenario: Effects fields are unaffected by save or load
- GIVEN a preset has been saved and later loaded
- WHEN the loaded synth is inspected
- THEN the 8 effects fields still match `kDefaultPatch`

### Requirement: Load Preset By Name

Loading a preset MUST restore its 11 live parameters, seed, and 4 generation pitch fields (`scaleType`, `rootPitchClass`, `rangeLow`, `rangeHigh`) as the current live state and update any displayed control values to match — no stale UI vs. live audio drift.
(Previously: restored only the 11 live parameters and seed; scale/root/range did not exist as persisted fields.)

#### Scenario: Loading restores all 11 parameters, the seed, scale/root/range, and displayed controls
- GIVEN a saved preset with known values, including a non-default scale/root/range
- WHEN the user loads it
- THEN the 11 live parameters, the seed, and the scale/root/range match the saved values, and displayed controls match the loaded values
