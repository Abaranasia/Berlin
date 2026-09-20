# Proposal: Scale-Aware Generation

## Intent

Pitch generation is already scale-constrained, but the scale is hardcoded: `SequenceBuilder.cpp:49` builds `PitchGenerator pitch (Scale::minor (48), 36, 72)`. Every generated sequence is therefore C minor across 36–72; scale, root, and range are absent from `GenerationParams` and from the UI. Slice 1 of the Berlin School authenticity initiative (obs #286), and its lowest-risk step: generation-time plumbing, no real-time path.

## Scope

### In Scope
- Add `scaleType`, `rootPitchClass`, `rangeLow`, `rangeHigh` to `GenerationParams`.
- Extend `Scale` with a named scale catalog (see D1).
- Refactor `buildSeededSequence` to take `(seed, const GenerationParams&)` instead of 5 positional args.
- Editor controls for scale, root, and range, following the existing staged-apply-on-Generate pattern.
- Defaults reproduce today's output byte-identically for an unchanged seed.

### In Scope (added at proposal gate)
- Persist `scaleType`, `rootPitchClass`, `rangeLow`, `rangeHigh` in the `Preset` ValueTree via `PresetManager`. This reverses design.md D4 for these 4 fields specifically; `mode`/`pulses`/`rotation`/`probability` remain unpersisted per the existing rule.
- Loading a preset saved before this change must default the 4 new fields to today's hardcoded values (minor/C/36-72), not fail or leave them uninitialized.

### Out of Scope
- Per-step pitch editing, chords, transposition, tempo/delay work (later slices).
- Any claim that a scale list is historically "Berlin School" (see Risks).

## Capabilities

### New Capabilities
- None.

### Modified Capabilities
- `deterministic-generation`: PitchGenerator inputs come from `GenerationParams`; determinism is now per-(seed, scale, root, range).
- `generation-live-control`: new scale/root/range controls and their apply semantics.
- `preset-persistence`: `Preset` schema gains 4 new fields; `PresetManager` load path must default them for pre-change preset files. (Corrected from `preset-system` at spec gate — `preset-persistence` is the real on-disk capability name.)

## Approach

| # | Decision (confirmed at proposal gate) |
|---|---|
| D1 | Catalog: Minor (default), Major, Dorian, Phrygian, Mixolydian, Harmonic Minor. Each is one interval array. |
| D2 | Root is a 12-entry pitch class, not a MIDI note — `Scale::contains` is mod-12, so root octave is behaviorally inert. |
| D3 | Range exposed now as low/high note controls; `PitchGenerator` already swaps an inverted pair. |
| D4 | Persist scale/root/range in presets, reversing design.md D4 for these 4 fields only. Old preset files without them load with today's defaults (minor/C/36-72). |

## Affected Areas

| Area | Impact | Description |
|------|--------|-------------|
| `Source/core/Scale.h/.cpp` | Modified | Named scale catalog |
| `Source/generation/GenerationParams.h` | Modified | 4 new fields |
| `Source/generation/SequenceBuilder.h/.cpp` | Modified | Signature + wiring |
| `Source/plugin/BerlinAudioProcessor.cpp` | Modified | 2 call sites |
| `Source/plugin/BerlinAudioProcessorEditor.h/.cpp` | Modified | 3 new controls |
| `Source/preset/Preset.h` | Modified | 4 new fields |
| `Source/preset/PresetManager.cpp/.h` | Modified | Serialize/deserialize new fields, default on load for old files |
| `Tests/Source/{SequenceBuilder,PitchGenerator,Reproducibility,PresetManager}Tests.cpp` | Modified | Signature + new coverage, old-preset-file load test |

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| Misrepresenting scales as genre-authentic | Med | No catalog is documented Berlin School history (obs #285). Label as musical options; only "minor predominates" is sourced. |
| Empty candidate set changes RNG draw count (fallback draws 0) | Low | Constrain range controls to >= one octave; assert existing clamp behaviour in tests. |
| Golden reproducibility tests break | Med | Defaults pinned to minor/C/36–72; a golden diff means a real regression. |
| Editor widget sprawl (~40 already) | Low | Reuse existing section layout; no new layout framework. |
| Old preset files lack the new fields | Med | `PresetManager` load path defaults missing fields to minor/C/36-72; add a regression test loading a pre-change preset file. |
| Preset schema now diverges from design.md D4's "GenerationParams is unpersisted" invariant | Low | Document the exception explicitly in design.md for this change; other GenerationParams fields (mode/pulses/rotation/probability) stay unpersisted. |

## Rollback Plan

Single PR; revert the commit. Corrected at design gate: this change bumps `PresetManager`'s schema version 1→2 (not a silent ValueTree passthrough). A rolled-back (pre-change) build reading a v2 preset file sees `version > kSchemaVersion` and rejects it with an existing user-facing string ("Preset was saved by a newer version of Berlin.") rather than silently ignoring the new properties. Non-destructive (the file itself is untouched) but not silent. This was chosen over keeping `kSchemaVersion = 1` with optional fields because it distinguishes a genuinely old preset from a truncated/corrupt new one — see design.md's schema-version decision.

## Dependencies

- None. Independent of the internal synth, MIDI routing, and tempo slices.

## Success Criteria

- [x] Default params reproduce pre-change output byte-identically for the same seed.
- [x] Every note satisfies `scale.contains(note)` and sits inside the chosen range.
- [x] Same seed + same scale/root/range reproduces identically; changing them alters pitch only, not rhythm.
- [x] No allocation or locking added to any audio-callback path.
- [x] A preset saved with scale/root/range round-trips through save/load unchanged.
- [x] Loading a pre-change preset file (missing the 4 new fields) defaults to minor/C/36-72 without error.

## Proposal question round — confirmed at proposal gate

1. Catalog: 6 entries (D1) — confirmed.
2. Range controls: in scope now (D3) — confirmed.
3. Persistence: scale/root/range DO persist in presets (D4 reversed) — confirmed. This is a deliberate, scoped exception to design.md's "GenerationParams is unpersisted" rule; it applies only to these 4 fields.
