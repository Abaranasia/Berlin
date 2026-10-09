/*
  ==============================================================================

   RhythmGenerator - out-of-line definitions (deterministic-generation spec).

  ==============================================================================
*/

#include "RhythmGenerator.h"

#include <algorithm>

namespace berlin
{

RhythmGenerator::RhythmGenerator (int numStepsIn, std::vector<float> probabilitiesIn)
    : numSteps (numStepsIn),
      probabilities (std::move (probabilitiesIn))
{
    // Pad with 0.0f (silent) or truncate to exactly numSteps, then clamp
    // each element to [0, 1] - all at construction time (design.md
    // Decisions 3/4). max(numSteps, 0) guards vector's size_type against a
    // negative numSteps.
    probabilities.resize ((std::size_t) std::max (numSteps, 0), 0.0f);

    for (auto& p : probabilities)
        p = std::clamp (p, 0.0f, 1.0f);
}

RhythmGenerator::RhythmGenerator (int numStepsIn, float densityIn)
    : RhythmGenerator (numStepsIn, std::vector<float> ((std::size_t) std::max (numStepsIn, 0), densityIn))
{
}

Sequence RhythmGenerator::generate (DeterministicRandom& random) const
{
    Sequence sequence (numSteps);

    for (int i = 0; i < numSteps; ++i)
    {
        // Exactly one draw per step, in ascending index order, regardless of
        // which endpoint guard fires below - this keeps the RNG stream
        // length independent of the per-step probability (design.md).
        const float draw = random.nextFloat();
        const float p = probabilities[(std::size_t) i];

        bool active = false;

        if (p >= 1.0f)
            active = true; // never rely on nextFloat()'s upper bound being < 1.0
        else if (p <= 0.0f)
            active = false;
        else
            active = draw < p;

        sequence[i].active = active;
    }

    return sequence;
}

} // namespace berlin
