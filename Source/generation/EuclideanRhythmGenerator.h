/*
  ==============================================================================

   EuclideanRhythmGenerator - Bjorklund's algorithm, a pure Euclidean-rhythm
   generator (euclidean-rhythm spec, roadmap Phase 10 "Advanced Generators",
   Slice 1 of 3).

   Depends on Sequence (Source/core/Sequence.h) only. Configuration is
   immutable after construction: numSteps/pulses/rotation are fixed at ctor
   time. `pulses` is clamped to [0, numSteps] (floor 0, NOT SkipMaskGenerator's
   floor of 1 - Euclidean has no phase anchor, so pulses == 0 must be total,
   mirroring RhythmGenerator's density <= 0 precedent, design.md Decision 6).
   `rotation` is wrapped via a true mathematical modulo of numSteps (always
   non-negative, including for negative input), design.md Decision 7.

   DELIBERATE DEVIATION from every sibling generator (RhythmGenerator,
   SkipMaskGenerator, PitchGenerator): generate() takes NO
   DeterministicRandom parameter. Bjorklund's algorithm is a pure function of
   (numSteps, pulses, rotation); removing the parameter makes "Euclidean
   consumes zero RNG draws" a compile-time guarantee, not something to be
   tested at runtime (design.md Decision 1).

   Only `active` is written; `note` is left at its default.

  ==============================================================================
*/

#pragma once

#include "../core/Sequence.h"

namespace berlin
{

class EuclideanRhythmGenerator
{
public:
    // pulses clamped to [0, numSteps]; rotation wrapped mod numSteps (any int, incl. negative)
    EuclideanRhythmGenerator (int numSteps, int pulses, int rotation);

    // NOTE the deliberate deviation from PitchGenerator/SkipMaskGenerator:
    // NO DeterministicRandom parameter. Bjorklund is a pure function of the
    // ctor config, so "consumes zero RNG draws" is enforced by the compiler.
    // Writes only `active`; `note` is left at its default.
    Sequence generate() const;

private:
    int numSteps { 0 }, pulses { 0 }, rotation { 0 };
};

} // namespace berlin
