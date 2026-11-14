/*
  ==============================================================================

   GenerationParams - the mode-agnostic generation configuration that used to
   live as MainComponent widget state (roadmap Phase 11 / vst3-au-plugin,
   design.md Decision 1 / D6).

   JUCE-free: standard library only. `RhythmMode` is lifted verbatim from
   MainComponent (ordinals MUST stay random=0, euclidean=1, probability=2 -
   probability was appended last on purpose, generation-randomize +
   probability-matrices history). `GenerationParams` bundles the values a
   BerlinAudioProcessor needs to build a Sequence: the editor pushes values in,
   the processor holds them, buildSeededSequence() (SequenceBuilder.h) consumes
   them.

   Persistence (scale-aware-generation design.md, scoped exception to prior
   D4): `mode`, `pulses`, `rotation`, `stepProbability`, `length` and `lockSeed` are
   NOT persisted - they are transient GUI-adjacent configuration, not the
   versioned Preset schema. `scaleType`, `rootPitchClass`, `rangeLow`, and
   `rangeHigh` ARE a deliberate, scoped exception: they are musical identity
   (what key the patch is in), so `Preset`/`PresetManager` persist those 4
   fields individually - never via whole-struct assignment, which would
   clobber the still-unpersisted fields above.

  ==============================================================================
*/

#pragma once

#include "core/Scale.h"

namespace berlin
{

enum class RhythmMode { random, euclidean, probability };

// How pitches are chosen for ACTIVE steps (musical-notes). random is the
// original uniform pick (byte-identical to pre-change output); smooth is the
// weighted, proximity/median/tonic-biased pick. Ordinals are fixed.
enum class MelodyStyle { random, smooth };

// Default step count (GenerationParams::length). The live length is a
// GenerationParams member (step-model); kMaxSteps (core/Sequence.h) bounds it.
inline constexpr int kNumSteps = 16;

// Pitch-range bounds (scale-aware-generation design.md). kMinPitchRangeSpan
// is the minimum-one-octave invariant: any 12 consecutive semitones contain
// all 12 pitch classes, so a span >= 12 makes PitchGenerator's candidate set
// provably non-empty for every catalog scale (normalizePitchRange enforces
// this at the single chokepoint inside buildSeededSequence).
inline constexpr int kMinPitch          = 0;
inline constexpr int kMaxPitch          = 127;
inline constexpr int kMinPitchRangeSpan = 12;
inline constexpr int kDefaultRangeLow   = 36;
inline constexpr int kDefaultRangeHigh  = 72;

struct GenerationParams
{
    RhythmMode mode            = RhythmMode::random;
    int        pulses          = 5;
    int        rotation        = 0;
    float      stepProbability = 0.5f;
    bool       lockSeed        = false;
    int        length          = kNumSteps;   // staged: applies on the next generation (clamped 1..kMaxSteps)

    // Persisted fields (see the header comment's exception to prior D4).
    // Defaults reproduce today's pre-change output byte-identically for an
    // unchanged seed: ScaleType::minor + pitch class 0 (C) anchored at
    // kPitchClassAnchor (48) is literally Scale::minor(48).
    ScaleType scaleType     = ScaleType::minor;
    int       rootPitchClass = 0;
    int       rangeLow       = kDefaultRangeLow;
    int       rangeHigh      = kDefaultRangeHigh;

    // Pitch-selection style; LAST field so existing aggregate initializers still compile.
    MelodyStyle melody = MelodyStyle::random;
};

} // namespace berlin
