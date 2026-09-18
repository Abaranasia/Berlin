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

#include <vector>

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

        // Characterization pin (design.md Decision 5): captures the EXACT
        // 16-bit active mask produced by the CURRENT, unmodified scalar-ctor
        // RhythmGenerator(16, 0.5f) at seed 12345. This is deliberately
        // written and observed GREEN against unmodified source BEFORE any
        // edit to RhythmGenerator.h/.cpp, then must still pass unmodified
        // after the vector-ctor generalization - that is the actual
        // regression proof for the scalar-ctor delegating overload (the
        // existing ReproducibilityTests golden only proves determinism, not
        // behavior preservation).
        beginTest ("characterization: RhythmGenerator(16, 0.5f) at seed 12345 pins an exact active mask");
        {
            berlin::RhythmGenerator generator (16, 0.5f);
            berlin::DeterministicRandom random (12345);

            const berlin::Sequence sequence = generator.generate (random);

            const bool expectedActive[16] = { true, false, true, false, true, false, false, true,
                                               true, true, true, false, true, false, false, false };

            for (int i = 0; i < sequence.size(); ++i)
                expectEquals ((int) sequence[i].active, (int) expectedActive[i]);
        }

        // --- Vector-ctor generalization (design.md Decisions 1-4) ---

        beginTest ("delegation equivalence: scalar ctor matches an equivalent uniform vector ctor, same seed");
        {
            constexpr int numSteps = 16;
            constexpr float density = 0.37f;

            berlin::RhythmGenerator scalarGenerator (numSteps, density);
            berlin::RhythmGenerator vectorGenerator (numSteps, std::vector<float> (numSteps, density));

            berlin::DeterministicRandom randomA (98765);
            berlin::DeterministicRandom randomB (98765);

            const berlin::Sequence sequenceA = scalarGenerator.generate (randomA);
            const berlin::Sequence sequenceB = vectorGenerator.generate (randomB);

            expect (sequenceA == sequenceB);
        }

        beginTest ("per-step endpoints are exact per index, not statistical, across many seeds");
        {
            constexpr int numSteps = 8;
            const std::vector<float> probabilities { 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f };

            for (juce::int64 seed = 0; seed < 20; ++seed)
            {
                berlin::RhythmGenerator generator (numSteps, probabilities);
                berlin::DeterministicRandom random (seed);

                const berlin::Sequence sequence = generator.generate (random);

                for (int i = 0; i < numSteps; ++i)
                    expectEquals ((int) sequence[i].active, (int) (probabilities[(size_t) i] >= 1.0f));
            }
        }

        beginTest ("stream length is numSteps regardless of the probability vector's own length");
        {
            constexpr int numSteps = 16;

            // Short (pad), long (truncate), and mixed-length vectors must all
            // consume exactly numSteps draws, leaving same-seed streams in
            // lock-step afterwards - same idiom as the existing scalar-ctor
            // stream-length test above (RhythmGeneratorTests.cpp:79-98).
            const std::vector<float> shortVector { 1.0f, 1.0f };                         // padded to 16
            const std::vector<float> longVector (32, 0.5f);                              // truncated to 16
            const std::vector<float> mixedVector { 0.0f, 1.0f, 0.25f, 0.75f, 0.5f };      // padded to 16

            for (const auto& probabilities : { shortVector, longVector, mixedVector })
            {
                berlin::RhythmGenerator generator (numSteps, probabilities);
                berlin::RhythmGenerator referenceGenerator (numSteps, 0.0f);

                berlin::DeterministicRandom randomA (4321);
                berlin::DeterministicRandom randomB (4321);

                generator.generate (randomA);
                referenceGenerator.generate (randomB);

                for (int i = 0; i < 10; ++i)
                    expectEquals (randomA.nextInt (1000), randomB.nextInt (1000));
            }
        }

        beginTest ("clamp: probabilities outside [0,1] are clamped at construction time");
        {
            constexpr int numSteps = 4;
            const std::vector<float> probabilities { 1.5f, -0.5f, 1.0f, 0.0f };

            for (juce::int64 seed = 0; seed < 20; ++seed)
            {
                berlin::RhythmGenerator generator (numSteps, probabilities);
                berlin::DeterministicRandom random (seed);

                const berlin::Sequence sequence = generator.generate (random);

                expect (sequence[0].active);   // 1.5 clamps to 1.0 -> always active
                expect (! sequence[1].active); // -0.5 clamps to 0.0 -> never active
                expect (sequence[2].active);   // 1.0 -> always active
                expect (! sequence[3].active); // 0.0 -> never active
            }
        }

        beginTest ("pad semantics: a vector shorter than numSteps is padded with 0.0f (silent)");
        {
            constexpr int numSteps = 8;
            const std::vector<float> probabilities { 1.0f, 1.0f }; // steps 2..7 padded

            for (juce::int64 seed = 0; seed < 20; ++seed)
            {
                berlin::RhythmGenerator generator (numSteps, probabilities);
                berlin::DeterministicRandom random (seed);

                const berlin::Sequence sequence = generator.generate (random);

                expect (sequence[0].active);
                expect (sequence[1].active);
                for (int i = 2; i < numSteps; ++i)
                    expect (! sequence[i].active);
            }
        }

        beginTest ("truncate semantics: a vector longer than numSteps drops the extra elements");
        {
            constexpr int numSteps = 8;
            std::vector<float> probabilities (20, 1.0f); // far more than numSteps, all 1.0

            for (juce::int64 seed = 0; seed < 20; ++seed)
            {
                berlin::RhythmGenerator generator (numSteps, probabilities);
                berlin::DeterministicRandom random (seed);

                const berlin::Sequence sequence = generator.generate (random);

                expectEquals (sequence.size(), numSteps);
                for (int i = 0; i < numSteps; ++i)
                    expect (sequence[i].active);
            }
        }

        beginTest ("per-step statistical frequency: each index's observed rate approximates its own probability");
        {
            constexpr int numSteps = 5;
            const std::vector<float> probabilities { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };
            constexpr int numSeeds = 500;

            std::vector<int> activeCounts (numSteps, 0);

            for (juce::int64 seed = 0; seed < numSeeds; ++seed)
            {
                berlin::RhythmGenerator generator (numSteps, probabilities);
                berlin::DeterministicRandom random (seed);

                const berlin::Sequence sequence = generator.generate (random);

                for (int i = 0; i < numSteps; ++i)
                    if (sequence[i].active)
                        ++activeCounts[(size_t) i];
            }

            for (int i = 0; i < numSteps; ++i)
            {
                const double observedRate = static_cast<double> (activeCounts[(size_t) i]) / static_cast<double> (numSeeds);
                expect (std::abs (observedRate - static_cast<double> (probabilities[(size_t) i])) < 0.1);
            }
        }
    }
};

static RhythmGeneratorTests rhythmGeneratorTests;
