/*
  ==============================================================================

   RhythmGenerator - out-of-line definitions (deterministic-generation spec).

  ==============================================================================
*/

#include "RhythmGenerator.h"

#include <algorithm>

namespace berlin
{

RhythmGenerator::RhythmGenerator (int numStepsIn, float densityIn)
    : numSteps (numStepsIn),
      density (std::clamp (densityIn, 0.0f, 1.0f))
{
}

Sequence RhythmGenerator::generate (DeterministicRandom& random) const
{
    Sequence sequence (numSteps);

    for (int i = 0; i < numSteps; ++i)
    {
        // Exactly one draw per step, in ascending index order, regardless of
        // which endpoint guard fires below - this keeps the RNG stream
        // length independent of density (design.md).
        const float draw = random.nextFloat();

        bool active = false;

        if (density >= 1.0f)
            active = true; // never rely on nextFloat()'s upper bound being < 1.0
        else if (density <= 0.0f)
            active = false;
        else
            active = draw < density;

        sequence[i].active = active;
    }

    return sequence;
}

} // namespace berlin
