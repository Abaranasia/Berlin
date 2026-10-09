/*
  ==============================================================================

   RhythmGenerator - per-step independent-probability gate generator
   (deterministic-generation spec, generalized to per-step probabilities by
   the probability-matrices slice).

   Depends on Sequence (Source/core/Sequence.h) and DeterministicRandom
   (Source/generation/DeterministicRandom.h). Configuration is immutable
   after construction: numSteps and the per-step probability vector are
   fixed at ctor time. The vector ctor is padded with 0.0f (silent) or
   truncated to exactly numSteps elements, and each element is clamped to
   [0, 1], all at construction time. The scalar ctor (numSteps, density) is
   a thin delegating overload that expands density into a uniform vector of
   length numSteps - it is not a parallel implementation. generate() draws
   exactly one nextFloat() per step, in ascending index order, regardless of
   per-step probability value - that draw order/count is the reproducibility
   contract (design.md) and must never be reordered or short-circuited. Only
   `active` is written; `note` is left at its default.

  ==============================================================================
*/

#pragma once

#include <vector>

#include "../core/Sequence.h"
#include "DeterministicRandom.h"

namespace berlin
{

class RhythmGenerator
{
public:
    // Per-step probabilities. Each element clamped to [0, 1]; the vector is
    // padded with 0.0f (silent) or truncated to exactly numSteps at ctor
    // time.
    RhythmGenerator (int numSteps, std::vector<float> probabilities);

    // Uniform-density convenience overload. Delegates to the vector ctor
    // with vector(numSteps, density) - one real init path, one clamp site.
    RhythmGenerator (int numSteps, float density);

    Sequence generate (DeterministicRandom& random) const; // config immutable

private:
    int numSteps { 0 };
    std::vector<float> probabilities;
};

} // namespace berlin
