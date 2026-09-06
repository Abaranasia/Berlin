/*
  ==============================================================================

   SkipMaskGenerator tests (deterministic-generation spec). RED first:
   Source/generation/SkipMaskGenerator.h/.cpp do not exist yet, so this suite
   fails to compile until 2.3 creates them. Covers exact activeSteps count,
   size == numSteps, step 0 never skipped, note field untouched, activeSteps
   clamped at both ends (0 -> 1, > numSteps -> numSteps), same-seed ->
   identical mask, different seeds -> differing masks, and exactly numSkips
   RNG draws consumed (design.md Decision 3's partial Fisher-Yates).

  ==============================================================================
*/

#include <juce_core/juce_core.h>

#include "generation/DeterministicRandom.h"
#include "generation/SkipMaskGenerator.h"

class SkipMaskGeneratorTests final : public juce::UnitTest
{
public:
    SkipMaskGeneratorTests() : juce::UnitTest ("SkipMaskGenerator", "Berlin") {}

    void runTest() override
    {
        beginTest ("generate produces exactly activeSteps active steps and size == numSteps");
        {
            berlin::SkipMaskGenerator generator (16, 11);
            berlin::DeterministicRandom random (12345);

            const berlin::Sequence sequence = generator.generate (random);

            expectEquals (sequence.size(), 16);

            int activeCount = 0;
            for (int i = 0; i < sequence.size(); ++i)
                if (sequence[i].active)
                    ++activeCount;

            expectEquals (activeCount, 11);
        }

        beginTest ("step 0 is never skipped, across many seeds");
        {
            for (juce::int64 seed = 0; seed < 30; ++seed)
            {
                berlin::SkipMaskGenerator generator (16, 11);
                berlin::DeterministicRandom random (seed);

                const berlin::Sequence sequence = generator.generate (random);

                expect (sequence[0].active);
            }
        }

        beginTest ("only active is populated; note stays at its default");
        {
            berlin::SkipMaskGenerator generator (16, 11);
            berlin::DeterministicRandom random (99);

            const berlin::Sequence sequence = generator.generate (random);

            for (int i = 0; i < sequence.size(); ++i)
                expectEquals (sequence[i].note, 0);
        }

        beginTest ("activeSteps clamped at the low end: 0 is treated as 1 (only step 0 active)");
        {
            berlin::SkipMaskGenerator generator (16, 0);
            berlin::DeterministicRandom random (1);

            const berlin::Sequence sequence = generator.generate (random);

            expectEquals (sequence.size(), 16);

            int activeCount = 0;
            for (int i = 0; i < sequence.size(); ++i)
                if (sequence[i].active)
                    ++activeCount;

            expectEquals (activeCount, 1);
            expect (sequence[0].active);
        }

        beginTest ("activeSteps clamped at the high end: > numSteps is treated as numSteps (all active)");
        {
            berlin::SkipMaskGenerator generator (16, 20);
            berlin::DeterministicRandom random (2);

            const berlin::Sequence sequence = generator.generate (random);

            expectEquals (sequence.size(), 16);

            for (int i = 0; i < sequence.size(); ++i)
                expect (sequence[i].active);
        }

        beginTest ("same seed produces an identical mask");
        {
            berlin::SkipMaskGenerator generatorA (16, 11);
            berlin::SkipMaskGenerator generatorB (16, 11);
            berlin::DeterministicRandom randomA (777);
            berlin::DeterministicRandom randomB (777);

            const berlin::Sequence sequenceA = generatorA.generate (randomA);
            const berlin::Sequence sequenceB = generatorB.generate (randomB);

            expect (sequenceA == sequenceB);
        }

        beginTest ("different seeds produce differing masks");
        {
            berlin::SkipMaskGenerator generatorA (16, 11);
            berlin::SkipMaskGenerator generatorB (16, 11);
            berlin::DeterministicRandom randomA (111);
            berlin::DeterministicRandom randomB (222);

            const berlin::Sequence sequenceA = generatorA.generate (randomA);
            const berlin::Sequence sequenceB = generatorB.generate (randomB);

            expect (! (sequenceA == sequenceB));
        }

        beginTest ("consumes exactly numSkips RNG draws (numSteps - activeSteps), ascending order");
        {
            constexpr int numSteps = 16;
            constexpr int activeSteps = 11;   // numSkips = 5
            constexpr juce::int64 seed = 555;

            berlin::SkipMaskGenerator generator (numSteps, activeSteps);
            berlin::DeterministicRandom randomA (seed);
            generator.generate (randomA);

            berlin::DeterministicRandom randomB (seed);
            for (int k = 0; k < numSteps - activeSteps; ++k)
                randomB.nextInt (1000);   // any bound: state advances once per call

            // If generate() consumed exactly numSkips draws, both streams are
            // now in lock-step.
            for (int i = 0; i < 10; ++i)
                expectEquals (randomA.nextInt (1000), randomB.nextInt (1000));
        }
    }
};

static SkipMaskGeneratorTests skipMaskGeneratorTests;
