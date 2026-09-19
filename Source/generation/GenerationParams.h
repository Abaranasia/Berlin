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
   them. This struct is NOT persisted (design.md Decision 4 / D4) - it is
   GUI-adjacent configuration, not the versioned Preset schema.

  ==============================================================================
*/

#pragma once

namespace berlin
{

enum class RhythmMode { random, euclidean, probability };

struct GenerationParams
{
    RhythmMode mode            = RhythmMode::random;
    int        pulses          = 5;
    int        rotation        = 0;
    float      stepProbability = 0.5f;
    bool       lockSeed        = false;
};

} // namespace berlin
