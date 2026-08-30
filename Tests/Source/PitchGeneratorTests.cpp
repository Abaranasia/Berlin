/*
  ==============================================================================

   PitchGenerator tests (deterministic-generation spec). RED first:
   Source/generation/PitchGenerator.h/.cpp do not exist yet, so this suite
   fails to compile until 5.2 creates them. Covers in-range/in-scale note
   generation, the empty-range clamp fallback (nearest in-scale note outside
   the range, computed at ctor time), and its lower-note tie-break rule
   (design.md: ties resolved to the LOWER note; the fallback consumes no RNG
   draw).

  ==============================================================================
*/

#include <juce_core/juce_core.h>

#include "core/Scale.h"
#include "generation/DeterministicRandom.h"
#include "generation/PitchGenerator.h"

class PitchGeneratorTests final : public juce::UnitTest
{
public:
    PitchGeneratorTests() : juce::UnitTest ("PitchGenerator", "Berlin") {}

    void runTest() override
    {
        beginTest ("generateNextNote returns a note in range and in scale");
        {
            const berlin::Scale scale = berlin::Scale::major (60);
            berlin::PitchGenerator generator (scale, 60, 72);
            berlin::DeterministicRandom random (12345);

            for (int i = 0; i < 50; ++i)
            {
                const int note = generator.generateNextNote (random);

                expect (note >= 60 && note <= 72);
                expect (scale.contains (note));
            }
        }

        beginTest ("same seed produces same sequence of generated notes");
        {
            const berlin::Scale scale = berlin::Scale::major (60);
            berlin::PitchGenerator generatorA (scale, 60, 72);
            berlin::PitchGenerator generatorB (scale, 60, 72);
            berlin::DeterministicRandom randomA (555);
            berlin::DeterministicRandom randomB (555);

            for (int i = 0; i < 20; ++i)
                expectEquals (generatorA.generateNextNote (randomA), generatorB.generateNextNote (randomB));
        }

        beginTest ("empty-range clamp - single out-of-scale note ties, resolves to the lower in-scale neighbour");
        {
            const berlin::Scale scale = berlin::Scale::major (60);

            // 61 (C#) is not in C major; its nearest in-scale neighbours are
            // 60 (root, one semitone below) and 62 (one semitone above) - an
            // exact tie at radius 1. design.md: ties resolve to the LOWER note.
            expect (! scale.contains (61));
            expect (scale.contains (60));
            expect (scale.contains (62));

            berlin::PitchGenerator generator (scale, 61, 61);
            berlin::DeterministicRandom random (1);

            expectEquals (generator.generateNextNote (random), 60);
        }

        beginTest ("empty-range clamp fallback consumes no RNG draw");
        {
            const berlin::Scale scale = berlin::Scale::major (60);
            berlin::PitchGenerator generator (scale, 61, 61);

            berlin::DeterministicRandom randomA (2026);
            berlin::DeterministicRandom randomB (2026);

            // Calling the fallback path repeatedly must not advance randomA's
            // draw sequence relative to an untouched randomB constructed from
            // the same seed.
            for (int i = 0; i < 5; ++i)
                generator.generateNextNote (randomA);

            for (int i = 0; i < 10; ++i)
                expectEquals (randomA.nextInt (1000), randomB.nextInt (1000));
        }

        beginTest ("constructor swaps an inverted range so low <= high");
        {
            const berlin::Scale scale = berlin::Scale::major (60);
            berlin::PitchGenerator generator (scale, 72, 60); // deliberately inverted
            berlin::DeterministicRandom random (99);

            for (int i = 0; i < 20; ++i)
            {
                const int note = generator.generateNextNote (random);

                expect (note >= 60 && note <= 72);
                expect (scale.contains (note));
            }
        }
    }
};

static PitchGeneratorTests pitchGeneratorTests;
