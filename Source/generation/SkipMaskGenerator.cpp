/*
  ==============================================================================

   SkipMaskGenerator - out-of-line definitions (deterministic-generation
   spec).

  ==============================================================================
*/

#include "SkipMaskGenerator.h"

#include <algorithm>
#include <vector>

namespace berlin
{

SkipMaskGenerator::SkipMaskGenerator (int numStepsIn, int activeStepsIn)
    : numSteps (numStepsIn),
      activeSteps (std::clamp (activeStepsIn, 1, numStepsIn))
{
}

Sequence SkipMaskGenerator::generate (DeterministicRandom& random) const
{
    Sequence sequence (numSteps);

    for (int i = 0; i < numSteps; ++i)
        sequence[i].active = true; // start from a full pulse train

    // Step 0 is the phase anchor and is never skipped; the scratch array
    // holds every OTHER step index, [1 .. numSteps-1].
    std::vector<int> scratch (static_cast<std::size_t> (numSteps - 1));
    for (int i = 0; i < numSteps - 1; ++i)
        scratch[static_cast<std::size_t> (i)] = i + 1;

    const int numSkips = numSteps - activeSteps; // exactly one draw per skip

    for (int i = 0; i < numSkips; ++i)
    {
        // Partial Fisher-Yates: pick one remaining index at random and swap
        // it to the front of the unprocessed range, then mark it skipped.
        const int j = i + random.nextInt ((numSteps - 1) - i);
        std::swap (scratch[static_cast<std::size_t> (i)], scratch[static_cast<std::size_t> (j)]);
        sequence[scratch[static_cast<std::size_t> (i)]].active = false;
    }

    return sequence;
}

} // namespace berlin
