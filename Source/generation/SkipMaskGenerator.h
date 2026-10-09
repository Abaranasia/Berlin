/*
  ==============================================================================

   SkipMaskGenerator - a deterministic displacement-pattern generator
   (deterministic-generation spec).

   Depends on Sequence (Source/core/Sequence.h) and DeterministicRandom
   (Source/generation/DeterministicRandom.h). Configuration is immutable
   after construction: numSteps and activeSteps are fixed at ctor time,
   activeSteps clamped to [1, numSteps]. generate() starts from a fully-
   active pulse train and deterministically skips exactly
   numSteps - activeSteps steps via a partial Fisher-Yates shuffle over
   [1 .. numSteps-1] - step 0 is the phase anchor and is never skipped. Only
   `active` is written; `note` is left at its default. This is a distinct
   capability from RhythmGenerator (per-step independent probability): here
   the active count is EXACT, not probabilistic, which is the whole point of
   a skip-mask displacement pattern (design.md Decision 2/3).

  ==============================================================================
*/

#pragma once

#include "../core/Sequence.h"
#include "DeterministicRandom.h"

namespace berlin
{

class SkipMaskGenerator
{
public:
    SkipMaskGenerator (int numSteps, int activeSteps); // activeSteps clamped to [1, numSteps]

    Sequence generate (DeterministicRandom& random) const; // config immutable

private:
    int numSteps { 0 };
    int activeSteps { 0 };
};

} // namespace berlin
