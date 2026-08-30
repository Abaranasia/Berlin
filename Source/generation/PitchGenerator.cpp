/*
  ==============================================================================

   PitchGenerator - out-of-line definitions (deterministic-generation spec).

  ==============================================================================
*/

#include "PitchGenerator.h"

#include <algorithm>
#include <utility>

namespace berlin
{

PitchGenerator::PitchGenerator (Scale scaleIn, int rangeLowIn, int rangeHighIn)
    : scale (std::move (scaleIn)),
      rangeLow (std::min (rangeLowIn, rangeHighIn)),
      rangeHigh (std::max (rangeLowIn, rangeHighIn))
{
    for (int note = rangeLow; note <= rangeHigh; ++note)
        if (scale.contains (note))
            candidates.push_back (note);

    if (candidates.empty())
    {
        // No in-scale note anywhere in [rangeLow, rangeHigh]: expand outward
        // one semitone at a time. The lower side is checked first at each
        // radius, so an exact tie (both sides in-scale at the same radius)
        // resolves to the LOWER note, per design.md.
        for (int radius = 1;; ++radius)
        {
            const int lowerCandidate = rangeLow - radius;

            if (scale.contains (lowerCandidate))
            {
                fallbackNote = lowerCandidate;
                break;
            }

            const int higherCandidate = rangeHigh + radius;

            if (scale.contains (higherCandidate))
            {
                fallbackNote = higherCandidate;
                break;
            }
        }
    }
}

int PitchGenerator::generateNextNote (DeterministicRandom& random) const
{
    if (candidates.empty())
        return fallbackNote; // no RNG draw consumed

    const auto index = static_cast<std::size_t> (random.nextInt (static_cast<int> (candidates.size())));
    return candidates[index];
}

} // namespace berlin
