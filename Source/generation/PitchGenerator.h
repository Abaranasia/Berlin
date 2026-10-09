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

#include <vector>

#include "../core/Scale.h"
#include "DeterministicRandom.h"

namespace berlin
{

class PitchGenerator
{
public:
    PitchGenerator (Scale scale, int rangeLow, int rangeHigh); // swaps if inverted

    int generateNextNote (DeterministicRandom& random) const; // config immutable

private:
    Scale scale;
    int rangeLow { 0 };
    int rangeHigh { 0 };

    std::vector<int> candidates;
    int fallbackNote { 0 };
};

} // namespace berlin
