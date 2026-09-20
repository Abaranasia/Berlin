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

#include <algorithm>
#include <vector>

namespace
{
    constexpr int kActiveSteps = 11;   // research's cited 16 -> 11 displacement (design.md Decision 3)
}

namespace berlin
{

void normalizePitchRange (int& low, int& high) noexcept
{
    if (low > high)
        std::swap (low, high);

    low  = juce::jlimit (kMinPitch, kMaxPitch, low);
    high = juce::jlimit (kMinPitch, kMaxPitch, high);

    if (low > high)
        std::swap (low, high);

    // Widen upward preferentially; only widen downward once high is pinned
    // at the ceiling (scale-aware-generation design.md: "span at the 127
    // ceiling widens downward").
    while (high - low < kMinPitchRangeSpan)
    {
        if (high < kMaxPitch)
            ++high;
        else
            --low;
    }
}

Sequence buildSeededSequence (juce::int64 seed, const GenerationParams& params)
{
    DeterministicRandom rng (seed);
    auto sequence = [&]
    {
        switch (params.mode)
        {
            case RhythmMode::euclidean:
                return EuclideanRhythmGenerator (kNumSteps, params.pulses, params.rotation).generate();   // 0 draws
            case RhythmMode::probability:
                return RhythmGenerator (kNumSteps, std::vector<float> ((std::size_t) kNumSteps, params.stepProbability))
                    .generate (rng);   // exactly kNumSteps draws (probability-matrices)
            case RhythmMode::random:
            default:
                return SkipMaskGenerator (kNumSteps, kActiveSteps).generate (rng);   // exactly 5 draws (design.md V1)
        }
    }();

    int rangeLow  = params.rangeLow;
    int rangeHigh = params.rangeHigh;
    normalizePitchRange (rangeLow, rangeHigh);

    PitchGenerator pitch (Scale::fromPitchClass (params.scaleType, params.rootPitchClass), rangeLow, rangeHigh);
    for (int i = 0; i < sequence.size(); ++i)
    {
        if (sequence[i].active)
            sequence[i].note = pitch.generateNextNote (rng);
    }

    return sequence;
}

} // namespace berlin
