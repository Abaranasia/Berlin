# Design: Scale-Aware Generation

## Technical Approach

Move the hardcoded `Scale::minor (48), 36, 72` out of `SequenceBuilder.cpp:49` into four new `GenerationParams` fields, fed by a `ScaleType` catalog in `Source/core/Scale.h`, staged from three editor controls, and persisted in the `Preset` ValueTree. All work is message-thread only: `buildSeededSequence` is already called from the processor's constructor and `regenerate()`; nothing new is read by `processBlock`. `PitchGenerator`'s allocation stays where it is today (construction, message thread) — no allocation, lock, or new atomic enters the audio callback.

## Architecture Decisions

### Decision: `ScaleType` enum + table lookup in `Scale`

**Choice**: `enum class ScaleType { minor, major, dorian, phrygian, mixolydian, harmonicMinor }` in `Source/core/Scale.h` (JUCE-free); a static interval table in `Scale.cpp`; new factory `Scale::fromPitchClass (ScaleType, int pitchClass)`. `Scale::major/minor(int)` stay for existing callers/tests.
**Alternatives considered**: public interval-array constructor; a `std::map` catalog; enum in `GenerationParams.h`.
**Rationale**: The private-constructor invariant ("no caller can smuggle an unsanctioned scale") is the documented reason `Scale` looks the way it does — a factory keeps it. The enum belongs with the type it names; `GenerationParams.h` includes `core/Scale.h` (both JUCE-free).

### Decision: pitch class anchored at MIDI 48

**Choice**: `rootPitchClass` is `0..11` (0 = C), floored-modulo wrapped. `fromPitchClass` builds the scale with root `kPitchClassAnchor + pitchClass`, `kPitchClassAnchor = 48`.
**Alternatives considered**: use the pitch class itself as the root note.
**Rationale**: `contains()` is mod-12, so the octave is inert for `PitchGenerator` (proposal D2) — but anchoring at 48 makes the default construction *literally* `Scale::minor (48)`, preserving byte-identical output, and keeps `getDegree()` in a musical octave for future callers.

### Decision: minimum one-octave range as a design invariant

**Choice**: `normalizePitchRange (int& low, int& high)` — declared in `SequenceBuilder.h`, defined in `SequenceBuilder.cpp` — swaps an inverted pair, clamps to `[0,127]`, then widens until `high - low >= kMinPitchRangeSpan (12)`. Called at the single chokepoint inside `buildSeededSequence`, and again by the editor to constrain the two sliders interactively.
**Alternatives considered**: enforce in the editor only; add a new `GenerationParams.cpp`.
**Rationale**: `PitchGenerator`'s empty-candidate fallback consumes **0** RNG draws vs 1 for the normal path (`PitchGenerator.cpp:55-59`). Any 12 consecutive semitones contain all 12 pitch classes, so a >= 12 span makes the candidate set provably non-empty for every catalog scale and pins the draw count at exactly one per active step. Enforcing inside `buildSeededSequence` also covers preset-loaded and test-constructed params, not just the UI. `SequenceBuilder.cpp` is already registered in both `.jucer` projects — a new `.cpp` would need two build-file edits.

### Decision: four explicit `Preset` fields, not a `GenerationParams` member

**Choice**: `Preset` gains `scaleType`, `rootPitchClass`, `rangeLow`, `rangeHigh` as scalars.
**Alternatives considered**: embed `GenerationParams generation;` in `Preset` (the `SynthPatch` 11-of-19 precedent).
**Rationale**: The `SynthPatch` precedent works because a patch is applied wholesale (`setPatch(preset.patch)`). A `GenerationParams` member invites `generationParams = preset.generation`, which would clobber the deliberately-unpersisted `mode`/`pulses`/`rotation`/`stepProbability` with defaults. Explicit fields make the partial scope type-enforced.

### Decision: schema version 1 → 2, version-gated field requirement

**Choice**: `kSchemaVersion = 2`. `toValueTree` writes the 4 fields into the existing `Generation` node (`scale` as a **name string** via a `scaleNames()` table, matching the `waveformNames()` convention; the three ints via `juce::String(int)`, matching the explicit-formatting rule). `fromValueTree`: for `version >= 2` the 4 properties are required (missing → `parseFailed`); for `version == 1` they are absent by definition and filled with minor/C/36–72, then `normalizePitchRange`d and pitch-class-wrapped (the existing clamp-continuous-values policy).
**Alternatives considered**: keep `kSchemaVersion = 1` and treat the fields as optional-if-present.
**Rationale**: This is the first real exercise of the migration path `PresetManager.h` already documents as "policy only — unreachable at v1". Version gating distinguishes a genuinely old file from a truncated new one. **Consequence, correcting the proposal's Rollback Plan**: a rolled-back pre-change build reads a v2 file as `version > kSchemaVersion` and returns `unsupportedVersion` — it does *not* silently ignore the unknown properties. Non-destructive (the file is untouched) and already has a user-facing string ("Preset was saved by a newer version of Berlin."), but it is not silent. See Open Questions.

### Decision: scoped exception to `GenerationParams` "not persisted" (prior `parameter-controls` design.md D4)

**Choice**: Only these 4 new fields persist. `mode`, `pulses`, `rotation`, `stepProbability`, `lockSeed` remain unpersisted.
**Rationale**: No contradiction with the prior D4 — that rule's fields are untouched and keep their behavior. The 4 new fields are *musical identity* (what key the patch is in), which belongs to a saved preset, whereas the rhythm-staging fields are transient GUI configuration. The `GenerationParams.h` header comment must be amended in place to state the exception rather than left asserting a now-false blanket rule.

## Data Flow

    Editor widgets (staged)                    Preset XML (v2)
    scaleBox/rootBox/rangeLow/rangeHigh              │
            │ Generate | Randomize click             │ loadPreset
            ▼                                        ▼
    pushGenerationParamsFromWidgets()   BerlinAudioProcessor::loadPreset()
            │ setGenerationParams()        │ writes ONLY the 4 fields into
            ▼                              ▼ generationParams, then regenerate(false)
      BerlinAudioProcessor::generationParams  (message thread, owner)
            │
            ▼
    buildSeededSequence (seed, const GenerationParams&)
            │ normalizePitchRange() → Scale::fromPitchClass() → PitchGenerator
            ▼
    Sequence ──→ player.publishSequence()  ──→ audio thread (existing lock-free handoff)

`refreshFromProcessor()` gains `applyGenerationParamsToWidgets()`, which writes back **only** the 4 persisted fields — the rhythm widgets stay the staging source of truth.

## File Changes

| File | Action | Description |
|------|--------|-------------|
| `Source/core/Scale.h/.cpp` | Modify | `ScaleType`, `kPitchClassAnchor = 48`, `fromPitchClass`, 6-entry interval table |
| `Source/generation/GenerationParams.h` | Modify | 4 fields; `kMinPitch/kMaxPitch/kMinPitchRangeSpan/kDefaultRangeLow/kDefaultRangeHigh`; amend the D4 header comment |
| `Source/generation/SequenceBuilder.h/.cpp` | Modify | Signature `(seed, const GenerationParams&)`; `normalizePitchRange`; wire scale/range into `PitchGenerator` |
| `Source/plugin/BerlinAudioProcessor.cpp` | Modify | 2 call sites (ctor init-list :28, `regenerate` :161); `save`/`loadPreset` carry the 4 fields |
| `Source/plugin/BerlinAudioProcessorEditor.h/.cpp` | Modify | `scaleBox`, `rootBox`, `rangeLowSlider`, `rangeHighSlider` + labels; 2 new `resized()` rows; `pushGenerationParamsFromWidgets`; `applyGenerationParamsToWidgets`; `setSize` height +2 rows |
| `Source/preset/Preset.h` | Modify | 4 fields with defaults minor/0/36/72 |
| `Source/preset/PresetManager.h/.cpp` | Modify | `kSchemaVersion = 2`; `scaleNames()` table; serialize/deserialize + v1 defaulting |
| `Tests/Source/{SequenceBuilder,PitchGenerator,Reproducibility,PresetSerialization,PresetManagerFile}Tests.cpp` | Modify | Signature migration + new coverage |

## Interfaces / Contracts

```cpp
// Source/core/Scale.h
enum class ScaleType { minor, major, dorian, phrygian, mixolydian, harmonicMinor };
// minor {0,2,3,5,7,8,10}  major {0,2,4,5,7,9,11}  dorian {0,2,3,5,7,9,10}
// phrygian {0,1,3,5,7,8,10}  mixolydian {0,2,4,5,7,9,10}  harmonicMinor {0,2,3,5,7,8,11}
static constexpr int kPitchClassAnchor = 48;              // C3; keeps defaults == Scale::minor(48)
static Scale fromPitchClass (ScaleType, int pitchClass);  // pitchClass floored-mod 12

// Source/generation/GenerationParams.h  (defaults reproduce today's output)
ScaleType scaleType = ScaleType::minor;
int rootPitchClass = 0, rangeLow = 36, rangeHigh = 72;

// Source/generation/SequenceBuilder.h
void normalizePitchRange (int& low, int& high) noexcept;   // swap, clamp [0,127], widen to span >= 12
Sequence buildSeededSequence (juce::int64 seed, const GenerationParams& params);
```

**Invariants**
1. `generationParams` must stay declared **before** `currentSequence` in `BerlinAudioProcessor.h` (:150 / :154) — the ctor init-list reads it.
2. `ScaleType` ordinals are UI/`setSelectedId` inputs only; persistence uses name strings, so ordinals may be reordered only with the same care already applied to `RhythmMode`.
3. Rhythm RNG draws always precede pitch draws; with invariant (4) each active step consumes exactly one pitch draw, so changing scale/root/range alters pitch only, never rhythm.
4. Candidate-set-non-empty: `normalizePitchRange` guarantees `high - low >= 12`, so `PitchGenerator`'s 0-draw fallback is unreachable from UI and preset paths (kept, and still directly unit-tested).
5. Nothing added here is read by `processBlock`; persisted state remains one versioned `ValueTree`.

## Testing Strategy

| Layer | What to Test | Approach |
|-------|-------------|----------|
| Unit | `Scale::fromPitchClass` for all 6 types; `fromPitchClass(minor, 0)` degree-for-degree equal to `Scale::minor(48)` | `ScaleTests` |
| Unit | `normalizePitchRange`: inverted pair, out-of-bounds, sub-octave span, span at the 127 ceiling widens downward | `SequenceBuilderTests` |
| Unit | Every generated note satisfies `scale.contains()` and sits in `[rangeLow, rangeHigh]` | `PitchGeneratorTests` |
| Unit | Default params byte-identical to the pre-change golden for a fixed seed | `ReproducibilityTests` (existing golden is the regression detector) |
| Unit | Same seed + different scale/root → pitches differ, `active` mask identical | `ReproducibilityTests` |
| Unit | v2 round trip of the 4 fields; v1 XML fixture (no new properties) loads as minor/C/36–72 with `PresetResult::ok`; v2 XML missing one new property → `parseFailed`; v3 XML → `unsupportedVersion` | `PresetSerializationTests` |
| Integration | `loadPreset` applies the 4 fields and regenerates without clobbering staged `mode`/`pulses`/`rotation`/`stepProbability` | `PresetManagerFileTests` |

RED first, per the project's strict-TDD rule; the `buildSeededSequence` signature change makes the existing suites fail to compile until migrated.

## Threat Matrix

N/A — no routing, shell, subprocess, VCS/PR automation, executable-file classification, or process-integration boundary. Preset file I/O is unchanged; `fileForName`'s existing sanitization + containment guard is untouched.

## Migration / Rollout

Forward: v1 presets load with minor/C/36–72 defaults. No file rewriting, no destructive migration. Backward: see the schema-version decision — a rolled-back build reports `unsupportedVersion` for v2 files instead of ignoring the new properties.

## Open Questions

- [x] Resolved at apply: the corrected rollback semantics are implemented as designed — `kSchemaVersion = 2`; a rolled-back (pre-change) build reading a v2 preset file rejects it with `PresetResult::unsupportedVersion` ("Preset was saved by a newer version of Berlin."), not a silent degrade. The optional-fields-at-v1 alternative was NOT taken, preserving the old-vs-truncated distinction this decision was made for.
- [x] Resolved at apply: editor height is `setSize (800, 680 + 2 * (kControlHeight + kMargin / 2))` = `800 x 748` (2 new rows: Scale/Root, Range Lo/Range Hi — each `kControlHeight` (28) + `kMargin / 2` (6) tall), computed from the file's own live layout constants rather than a hardcoded literal.
