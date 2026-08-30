/*
  ==============================================================================

   RhythmGenerator tests (deterministic-generation spec). RED first:
   Source/generation/RhythmGenerator.h/.cpp do not exist yet, so this suite
   fails to compile until 6.2 creates them. Covers output size == numSteps,
   only the `active` field being written (note stays at its default), the
   density 0.0/1.0 endpoint guards (design.md: nextFloat()'s upper bound is
   not guaranteed excluded, so density == 1.0 must not rely on strict-
   inequality luck), the fact that the per-step draw still happens at both
   endpoints (density must never shift the RNG stream length), and a
   statistical mean-density check across many distinct seeds (density is an
   average, not an exact per-run quota).

  ==============================================================================
*/

#include <juce_core/juce_core.h>

#include "generation/DeterministicRandom.h"
#include "generation/RhythmGenerator.h"

class RhythmGeneratorTests final : public juce::UnitTest
{
public:
    RhythmGeneratorTests() : juce::UnitTest ("RhythmGenerator", "Berlin") {}

    void runTest() override
    {
        beginTest ("generate produces a Sequence with exactly numSteps steps");
        {
            berlin::RhythmGenerator generator (16, 0.5f);
            berlin::DeterministicRandom random (1);

            const berlin::Sequence sequence = generator.generate (random);

            expectEquals (sequence.size(), 16);
        }

        beginTest ("generate only writes active; note stays at its default");
        {
            berlin::RhythmGenerator generator (32, 0.5f);
            berlin::DeterministicRandom random (2);

            const berlin::Sequence sequence = generator.generate (random);

            for (int i = 0; i < sequence.size(); ++i)
                expectEquals (sequence[i].note, 0);
        }

        beginTest ("density 0.0 - no step is ever active, across many seeds");
        {
            for (juce::int64 seed = 0; seed < 20; ++seed)
            {
                berlin::RhythmGenerator generator (32, 0.0f);
                berlin::DeterministicRandom random (seed);

                const berlin::Sequence sequence = generator.generate (random);

                for (int i = 0; i < sequence.size(); ++i)
                    expect (! sequence[i].active);
            }
        }

        beginTest ("density 1.0 - every step is always active, across many seeds");
        {
            for (juce::int64 seed = 0; seed < 20; ++seed)
            {
                berlin::RhythmGenerator generator (32, 1.0f);
                berlin::DeterministicRandom random (seed + 1000);

                const berlin::Sequence sequence = generator.generate (random);

                for (int i = 0; i < sequence.size(); ++i)
                    expect (sequence[i].active);
            }
        }

        beginTest ("density never shifts the RNG stream length (per-step draw at both endpoints)");
        {
            constexpr int numSteps = 16;

            berlin::RhythmGenerator inactiveGenerator (numSteps, 0.0f);
            berlin::RhythmGenerator activeGenerator (numSteps, 1.0f);

            berlin::DeterministicRandom randomA (777);
            berlin::DeterministicRandom randomB (777);

            inactiveGenerator.generate (randomA);
            activeGenerator.generate (randomB);

            // Both generate() calls must have consumed exactly the same
            // number of draws (one nextFloat() per step) regardless of the
            // endpoint guard taken, so the two same-seed streams stay in
            // lock-step afterwards.
            for (int i = 0; i < 10; ++i)
                expectEquals (randomA.nextInt (1000), randomB.nextInt (1000));
        }

        beginTest ("density approximates the mean active-step proportion across many seeds");
        {
            constexpr int numSteps = 16;
            constexpr float density = 0.5f;
            constexpr int numSeeds = 500;

            int totalActive = 0;

            for (juce::int64 seed = 0; seed < numSeeds; ++seed)
            {
                berlin::RhythmGenerator generator (numSteps, density);
                berlin::DeterministicRandom random (seed);

                const berlin::Sequence sequence = generator.generate (random);

                for (int i = 0; i < sequence.size(); ++i)
                    if (sequence[i].active)
                        ++totalActive;
            }

            const double meanActivePerRun = static_cast<double> (totalActive) / static_cast<double> (numSeeds);
            const double expectedMean = numSteps * density;

            // Individual runs are allowed to differ (e.g. 7, 8, or 9 active
            // steps out of 16); only the mean across many seeds must
            // approximate numSteps * density within statistical tolerance.
            expect (std::abs (meanActivePerRun - expectedMean) < 0.75);
        }

        beginTest ("individual runs are permitted to report differing exact active counts");
        {
            constexpr int numSteps = 16;
            constexpr float density = 0.5f;

            int firstCount = -1;
            bool sawDifferentCount = false;

            for (juce::int64 seed = 0; seed < 30; ++seed)
            {
                berlin::RhythmGenerator generator (numSteps, density);
                berlin::DeterministicRandom random (seed);

                const berlin::Sequence sequence = generator.generate (random);

                int activeCount = 0;
                for (int i = 0; i < sequence.size(); ++i)
                    if (sequence[i].active)
                        ++activeCount;

                if (firstCount < 0)
                    firstCount = activeCount;
                else if (activeCount != firstCount)
                    sawDifferentCount = true;
            }

            expect (sawDifferentCount);
        }
    }
};

static RhythmGeneratorTests rhythmGeneratorTests;
