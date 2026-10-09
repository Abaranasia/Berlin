/*
  ==============================================================================

   DeterministicRandom tests (deterministic-generation spec). RED first:
   Source/generation/DeterministicRandom.h does not exist yet, so this suite
   fails to compile until 4.2 creates it. Covers explicit-seed construction,
   same-seed reproducibility of a draw sequence, and the compile-time
   no-default-constructor contract (a seed must always be supplied - no
   default-seeded or time-seeded construction is available).

  ==============================================================================
*/

#include <type_traits>

#include <juce_core/juce_core.h>

#include "generation/DeterministicRandom.h"

// Compile-time proof of the explicit-seed-only contract: DeterministicRandom
// must not be default constructible - a caller cannot smuggle in a
// default-seeded or time-seeded instance.
static_assert (! std::is_default_constructible_v<berlin::DeterministicRandom>,
               "DeterministicRandom must not have a default constructor - a "
               "seed must always be supplied explicitly.");

class DeterministicRandomTests final : public juce::UnitTest
{
public:
    DeterministicRandomTests() : juce::UnitTest ("DeterministicRandom", "Berlin") {}

    void runTest() override
    {
        beginTest ("getSeed() returns the seed it was constructed with");
        {
            berlin::DeterministicRandom random (12345);

            expectEquals (random.getSeed(), (juce::int64) 12345);
        }

        beginTest ("same seed produces same sequence of nextInt draws");
        {
            berlin::DeterministicRandom a (42);
            berlin::DeterministicRandom b (42);

            for (int i = 0; i < 10; ++i)
                expectEquals (a.nextInt (1000), b.nextInt (1000));
        }

        beginTest ("same seed produces same sequence of nextFloat draws");
        {
            berlin::DeterministicRandom a (777);
            berlin::DeterministicRandom b (777);

            for (int i = 0; i < 10; ++i)
                expectEquals (a.nextFloat(), b.nextFloat());
        }

        beginTest ("different seeds are not required to match, but each instance is internally consistent");
        {
            berlin::DeterministicRandom random (99);

            expectEquals (random.getSeed(), (juce::int64) 99);
        }
    }
};

static DeterministicRandomTests deterministicRandomTests;
