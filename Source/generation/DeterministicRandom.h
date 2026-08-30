/*
  ==============================================================================

   DeterministicRandom - explicit-seed-only wrapper over juce::Random
   (deterministic-generation spec).

   Depends on juce_core only. Header-only: a thin set of inline forwarders
   over juce::Random, per design.md's ".h/.cpp for types with out-of-line
   definitions; header-only for pure aggregates/forwarders" decision.

   The default constructor is deleted so a seed must always be supplied
   explicitly - no default-seeded or time-seeded construction is possible.
   This is what makes the seed-in/identical-out reproducibility contract
   (later depended on by PitchGenerator/RhythmGenerator) provable rather
   than incidental.

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>

namespace berlin
{

class DeterministicRandom
{
public:
    explicit DeterministicRandom (juce::int64 seedValue)
        : seed (seedValue), rng (seedValue)
    {
    }

    DeterministicRandom() = delete; // contract made explicit & static_assert-able

    int nextInt (int exclusiveUpperBound) { return rng.nextInt (exclusiveUpperBound); }
    float nextFloat() { return rng.nextFloat(); }

    juce::int64 getSeed() const noexcept { return seed; }

private:
    juce::int64 seed;
    juce::Random rng;
};

} // namespace berlin
