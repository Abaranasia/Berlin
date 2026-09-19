/*
  ==============================================================================

   SequenceBuilder - out-of-line definition (roadmap Phase 11 / vst3-au-plugin,
   design.md Decision 6 / D6). Moved VERBATIM from
   MainComponent::buildSeededSequence - the constants and algorithm below are
   byte-identical to the pre-extraction implementation.

  ==============================================================================
*/

#include "SequenceBuilder.h"

#include "core/Scale.h"
#include "generation/DeterministicRandom.h"
#include "generation/EuclideanRhythmGenerator.h"
#include "generation/PitchGenerator.h"
#include "generation/RhythmGenerator.h"
#include "generation/SkipMaskGenerator.h"

#include <vector>

namespace
{
    constexpr int kNumSteps    = 16;
    constexpr int kActiveSteps = 11;   // research's cited 16 -> 11 displacement (design.md Decision 3)
}

namespace berlin
{

Sequence buildSeededSequence (juce::int64 seed, RhythmMode mode, int pulses, int rotation, float stepProbability)
{
    DeterministicRandom rng (seed);
    auto sequence = [&]
    {
        switch (mode)
        {
            case RhythmMode::euclidean:
                return EuclideanRhythmGenerator (kNumSteps, pulses, rotation).generate();   // 0 draws
            case RhythmMode::probability:
                return RhythmGenerator (kNumSteps, std::vector<float> ((std::size_t) kNumSteps, stepProbability))
                    .generate (rng);   // exactly kNumSteps draws (probability-matrices)
            case RhythmMode::random:
            default:
                return SkipMaskGenerator (kNumSteps, kActiveSteps).generate (rng);   // exactly 5 draws (design.md V1)
        }
    }();

    PitchGenerator pitch (Scale::minor (48), 36, 72);
    for (int i = 0; i < sequence.size(); ++i)
    {
        if (sequence[i].active)
            sequence[i].note = pitch.generateNextNote (rng);
    }

    return sequence;
}

} // namespace berlin
