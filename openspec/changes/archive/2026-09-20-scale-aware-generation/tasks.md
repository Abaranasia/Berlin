# Tasks: Scale-Aware Generation

## Review Workload Forecast

| Field | Value |
|-------|-------|
| Estimated changed lines | ~450–600 (additions+deletions) |
| 400-line budget risk | Medium |
| Chained PRs recommended | No |
| Suggested split | Single PR, size:exception (session's pre-approved 800-line budget) |
| Delivery strategy | single-pr-default |
| Chain strategy | size-exception |

Decision needed before apply: No
Chained PRs recommended: No
Chain strategy: size-exception
400-line budget risk: Medium

Estimate exceeds the default 400-line guard (8 production files + 6 test files: catalog table, signature migration, `normalizePitchRange`, 4 editor widgets, schema v2 serialize/deserialize) but stays well under the 800-line budget already approved for this session. No decision needed before apply; flagging here per convention. If apply-time diff approaches 800, stop and re-forecast before continuing.

### Suggested Work Units

| Unit | Goal | Likely PR | Focused test command | Runtime harness | Rollback boundary |
|------|------|-----------|----------------------|-----------------|-------------------|
| 1 | Scale catalog + params + signature migration (Phases 1-4) | PR 1 (single) | `SequenceBuilderTests`, `PitchGeneratorTests`, `ReproducibilityTests`, `ScaleTests` | Manual: Generate with default settings, confirm unchanged output | Revert Scale/GenerationParams/SequenceBuilder + their tests |
| 2 | Preset schema v2 (Phases 5-6) | PR 1 (single) | `PresetSerializationTests`, `PresetManagerFileTests` | Manual: save/load a preset, load a pre-change preset file | Revert Preset.h/PresetManager + processor save/load wiring |
| 3 | Editor controls (Phase 7) | PR 1 (single) | N/A — no automated editor-level harness in this project (precedent: Pulses/Rotation staged widgets are also untested) | Manual: verify all 4 generation-live-control scenarios by hand | Revert editor .h/.cpp widget additions |

## Phase 1: Foundation — Scale Catalog & Params

- [x] 1.1 RED: `ScaleTests` — `fromPitchClass(minor, 0)` degree-equal to `Scale::minor(48)`; interval sets for all 6 `ScaleType`s.
- [x] 1.2 GREEN: `Source/core/Scale.h/.cpp` — add `ScaleType`, `kPitchClassAnchor=48`, `fromPitchClass`, 6-entry interval table.
- [x] 1.3 GREEN: `Source/generation/GenerationParams.h` — add `scaleType`, `rootPitchClass`, `rangeLow`, `rangeHigh` + `kMinPitch/kMaxPitch/kMinPitchRangeSpan/kDefaultRangeLow/kDefaultRangeHigh`; amend D4 comment to state the 4-field persistence exception.

## Phase 2: Migrate Existing Call Sites (mandatory pre-step)

- [x] 2.1 RED: Update `SequenceBuilderTests.cpp` call sites to `buildSeededSequence(seed, const GenerationParams&)` — expected to fail compilation until 2.2. (`PitchGeneratorTests.cpp`/`ReproducibilityTests.cpp` do not call `buildSeededSequence` directly — verified via grep; no migration needed in those two files, see Deviations.)
- [x] 2.2 GREEN: `Source/generation/SequenceBuilder.h/.cpp` — change signature; build `PitchGenerator` from `Scale::fromPitchClass(params.scaleType, params.rootPitchClass)` + normalized range.
- [x] 2.3 GREEN: `Source/plugin/BerlinAudioProcessor.cpp` — update ctor init-list (:28) and `regenerate()` (:161) call sites to pass `generationParams`.

## Phase 3: Range Normalization Invariant

- [x] 3.1 RED: `SequenceBuilderTests.cpp` — `normalizePitchRange`: inverted pair, out-of-[0,127] clamp, sub-octave span widened to ≥12, span at the 127 ceiling widens downward.
- [x] 3.2 GREEN: `SequenceBuilder.h/.cpp` — implement `normalizePitchRange(int&, int&) noexcept`; call at the single chokepoint before constructing `PitchGenerator`.

## Phase 4: Deterministic-Generation Acceptance

- [x] 4.1 RED: `PitchGeneratorTests.cpp` — generated notes satisfy `scale.contains()` + `[rangeLow, rangeHigh]`; dorian/D/[40,64] scenario.
- [x] 4.2 RED: `ReproducibilityTests.cpp` — default params byte-identical to pre-change golden.
- [x] 4.3 RED: `ReproducibilityTests.cpp` — same seed+scale/root/range reproducible; changing them alters notes only, `active` unchanged.
- [x] 4.4 GREEN: No gap surfaced — 2.2/3.2's implementation satisfied 4.1-4.3 with no additional production code.

## Phase 5: Preset Schema v2

- [x] 5.1 RED: `PresetSerializationTests.cpp` — v2 round trip of 4 fields; v1 fixture defaults to minor/C/36-72 (`ok`); v2 missing a field → `parseFailed`; v3 → `unsupportedVersion`.
- [x] 5.2 GREEN: `Source/preset/Preset.h` — add 4 fields (defaults minor/0/36/72).
- [x] 5.3 GREEN: `Source/preset/PresetManager.h/.cpp` — `kSchemaVersion=2`; `scaleNames()` table (mirrors `waveformNames()`).
- [x] 5.4 GREEN: `PresetManager.cpp` — extend `toValueTree`/`fromValueTree`: write 4 fields into `Generation` node; v≥2 requires all 4; v==1 defaults (normalize/wrap is a no-op for these exact defaults — see Deviations).

## Phase 6: Preset Integration

- [x] 6.1 RED: `BerlinAudioProcessorTests.cpp` — load applies 4 fields into `generationParams`, regenerates, without clobbering staged `mode`/`pulses`/`rotation`/`stepProbability`. (Relocated from `PresetManagerFileTests.cpp` — see Deviations: `generationParams` is owned by `BerlinAudioProcessor`, not `PresetManager`.)
- [x] 6.2 GREEN: `BerlinAudioProcessor.cpp` — `save`/`loadPreset`/`setStateInformation` carry the 4 fields through `Preset`.

## Phase 7: Editor Controls

- [x] 7.1 `BerlinAudioProcessorEditor.h` — add `scaleBox`, `rootBox`, `rangeLowSlider`, `rangeHighSlider` + labels.
- [x] 7.2 `BerlinAudioProcessorEditor.cpp` ctor — populate scaleBox (6 items) / rootBox (12 classes); add 2 `resized()` rows; bump `setSize` height.
- [x] 7.3 Extend `pushGenerationParamsFromWidgets()` to stage the 4 values (applied only on Generate/Randomize, per staged-apply pattern).
- [x] 7.4 Add `applyGenerationParamsToWidgets()`, called from `refreshFromProcessor()`, writing back only the 4 persisted fields.
- [x] 7.5 Manual verification against `generation-live-control` spec's 4 scenarios (no automated editor harness exists in this project) — verified by code trace, see apply-progress Work Unit Evidence.

## Phase 8: Cleanup

- [x] 8.1 Resolve design.md's open questions (editor height value; confirm v2-rollback wording) in-place.
- [x] 8.2 Full suite run: `Scale`, `SequenceBuilder`, `PitchGenerator`, `Reproducibility`, `PresetSerialization`, `PresetManagerFile` — confirm no regressions. (269/269 tests pass, 0 failures.)
