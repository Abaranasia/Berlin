/*
  ==============================================================================

   PitchGenerator - out-of-line definitions (deterministic-generation spec).

  ==============================================================================
*/

#include "PitchGenerator.h"

#include <algorithm>
#include <cstdlib>
#include <iterator>
#include <utility>

namespace berlin
{

namespace melody
{

int smoothWeight (int candidateIndex, std::optional<int> previousIndex, int count, bool isTonic) noexcept
{
    const int tonicFactor = isTonic ? kTonicWeight : kOtherWeight;

    if (! previousIndex.has_value())
        return tonicFactor;

    const int distance = std::abs (candidateIndex - *previousIndex);

    if (distance >= static_cast<int> (std::size (kProximityWeights)))
        return 0;

    // Pull toward the median: offsets are doubled to stay integer (centre = count - 1).
    const int previousOffset  = std::abs (2 * *previousIndex - (count - 1));
    const int candidateOffset = std::abs (2 * candidateIndex - (count - 1));
    const int medianFactor    = candidateOffset < previousOffset ? count + 2 * previousOffset : count;

    return kProximityWeights[distance] * medianFactor * tonicFactor;
}

} // namespace melody

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

int PitchGenerator::generateMelodyNote (DeterministicRandom& random, std::optional<int> previousNote) const
{
    if (candidates.empty())
        return fallbackNote; // no RNG draw consumed (same as generateNextNote)

    const int count = static_cast<int> (candidates.size());

    std::optional<int> previousIndex;
    int first = 0;
    int last  = count - 1;

    if (previousNote.has_value())
    {
        const auto it = std::lower_bound (candidates.begin(), candidates.end(), *previousNote);
        const int index = std::min (static_cast<int> (it - candidates.begin()), count - 1);
        previousIndex = index;
        first = std::max (0, index - melody::kWindow);
        last  = std::min (count - 1, index + melody::kWindow);
    }

    // Floored modulo: correct for notes below the root.
    const auto isTonic = [this] (int note) { return ((note - scale.getRoot()) % 12 + 12) % 12 == 0; };

    int total = 0;

    for (int i = first; i <= last; ++i)
        total += melody::smoothWeight (i, previousIndex, count, isTonic (candidates[static_cast<std::size_t> (i)]));

    // total > 0 always: the repeat candidate (or any first-step candidate) weighs >= 1 * 1 * 1.
    const int target = random.nextInt (total);
    int cumulative = 0;

    for (int i = first; i <= last; ++i)
    {
        cumulative += melody::smoothWeight (i, previousIndex, count, isTonic (candidates[static_cast<std::size_t> (i)]));

        if (target < cumulative)
            return candidates[static_cast<std::size_t> (i)];
    }

    return candidates[static_cast<std::size_t> (last)]; // unreachable: target < total
}

} // namespace berlin
