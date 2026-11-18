/*
  ==============================================================================

   PitchGenerator - a Scale-constrained pitch generator over a fixed note
   range (deterministic-generation spec).

   Depends on Scale (Source/core/Scale.h) and DeterministicRandom
   (Source/generation/DeterministicRandom.h). Configuration is immutable
   after construction: the in-scale candidate list within [rangeLow,
   rangeHigh] is precomputed once in the ctor. If the configured range
   contains no in-scale note, a single fallback note is also precomputed at
   ctor time - the nearest in-scale note outside the range, found by
   expanding one semitone at a time and resolving exact ties to the LOWER
   note (design.md rationale: draw-then-snap is non-uniform and can leave
   the range; a per-call nearest search is O(range) per step with an
   undocumented tie-break). generateNextNote() spends one RNG draw to index
   the candidate list, or returns the fallback note while consuming NO RNG
   draw at all.

  ==============================================================================
*/

#pragma once

#include <iterator>
#include <optional>
#include <vector>

#include "../core/Scale.h"
#include "DeterministicRandom.h"

namespace berlin
{

// Smooth melody weighting (musical-notes design.md D2/D3/D4): integer-only,
// weight = P[d] * M * K, where d is the distance in candidate-index
// (scale-degree) steps from the previous active note. Calibrated, not sourced.
namespace melody
{
inline constexpr int kProximityWeights[] = { 6, 16, 8, 3, 1 };   // d = 0..4; d >= 5 weighs 0
inline constexpr int kWindow = static_cast<int> (std::size (kProximityWeights)) - 1;   // 4: furthest reachable index distance
inline constexpr int kTonicWeight = 2;
inline constexpr int kOtherWeight = 1;

// previousIndex empty (first active step): the weight is K alone. count is the
// candidate count (>= 1); isTonic is (note - root) mod 12 == 0.
int smoothWeight (int candidateIndex, std::optional<int> previousIndex, int count, bool isTonic) noexcept;
} // namespace melody

class PitchGenerator
{
public:
    PitchGenerator (Scale scale, int rangeLow, int rangeHigh); // swaps if inverted

    int generateNextNote (DeterministicRandom& random) const; // config immutable

    // Smooth pick: exactly ONE nextInt draw per call (none when the range has no
    // in-scale note, which returns the same fallbackNote as generateNextNote).
    int generateMelodyNote (DeterministicRandom& random, std::optional<int> previousNote) const;

private:
    Scale scale;
    int rangeLow { 0 };
    int rangeHigh { 0 };

    std::vector<int> candidates;
    int fallbackNote { 0 };
};

} // namespace berlin
