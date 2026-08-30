/*
  ==============================================================================

   RhythmGenerator - per-step independent-probability gate generator
   (deterministic-generation spec).

   Depends on Sequence (Source/core/Sequence.h) and DeterministicRandom
   (Source/generation/DeterministicRandom.h). Configuration is immutable
   after construction: numSteps and density are fixed at ctor time, density
   clamped to [0, 1]. generate() draws exactly one nextFloat() per step, in
   ascending index order - that draw order is the reproducibility contract
   (design.md) and must never be reordered or short-circuited. Only `active`
   is written; `note` is left at its default.

  ==============================================================================
*/

#pragma once

#include "../core/Sequence.h"
#include "DeterministicRandom.h"

namespace berlin
{

class RhythmGenerator
{
public:
    RhythmGenerator (int numSteps, float density); // density clamped to [0, 1]

    Sequence generate (DeterministicRandom& random) const; // config immutable

private:
    int numSteps { 0 };
    float density { 0.0f };
};

} // namespace berlin
