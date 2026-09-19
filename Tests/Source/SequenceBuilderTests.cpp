/*
  ==============================================================================

   SequenceBuilder tests (roadmap Phase 11 / vst3-au-plugin, design.md
   Decision 6 / D6). RED first: Source/generation/SequenceBuilder.h does not
   exist yet, so this suite must fail to compile until the GREEN step creates
   it.

   Covers: buildSeededSequence is deterministic per RhythmMode - the SAME seed
   (and same mode/pulses/rotation/stepProbability) MUST produce a bit-identical
   Sequence across repeated calls, for every RhythmMode (random/euclidean/
   probability). This is the exact behavior MainComponent::buildSeededSequence
   already had; the move to Source/generation/ must not change it.

  ==============================================================================
*/

#include <juce_core/juce_core.h>

#include "generation/GenerationParams.h"
#include "generation/SequenceBuilder.h"

class SequenceBuilderTests final : public juce::UnitTest
{
public:
    SequenceBuilderTests() : juce::UnitTest ("SequenceBuilder", "Berlin") {}

    void runTest() override
    {
        constexpr juce::int64 seed = 424242;

        beginTest ("RhythmMode::random: same seed produces an identical Sequence");
        {
            const auto a = berlin::buildSeededSequence (seed, berlin::RhythmMode::random, 5, 0, 0.5f);
            const auto b = berlin::buildSeededSequence (seed, berlin::RhythmMode::random, 5, 0, 0.5f);
            expect (a == b);
        }

        beginTest ("RhythmMode::euclidean: same seed/pulses/rotation produces an identical Sequence");
        {
            const auto a = berlin::buildSeededSequence (seed, berlin::RhythmMode::euclidean, 7, 3, 0.5f);
            const auto b = berlin::buildSeededSequence (seed, berlin::RhythmMode::euclidean, 7, 3, 0.5f);
            expect (a == b);
        }

        beginTest ("RhythmMode::probability: same seed/stepProbability produces an identical Sequence");
        {
            const auto a = berlin::buildSeededSequence (seed, berlin::RhythmMode::probability, 5, 0, 0.35f);
            const auto b = berlin::buildSeededSequence (seed, berlin::RhythmMode::probability, 5, 0, 0.35f);
            expect (a == b);
        }

        beginTest ("Different seeds produce different Sequences for RhythmMode::random");
        {
            const auto a = berlin::buildSeededSequence (seed, berlin::RhythmMode::random, 5, 0, 0.5f);
            const auto b = berlin::buildSeededSequence (seed + 1, berlin::RhythmMode::random, 5, 0, 0.5f);
            expect (! (a == b));
        }

        beginTest ("Euclidean pulses/rotation change is reflected even with the same seed");
        {
            const auto a = berlin::buildSeededSequence (seed, berlin::RhythmMode::euclidean, 5, 0, 0.5f);
            const auto b = berlin::buildSeededSequence (seed, berlin::RhythmMode::euclidean, 9, 2, 0.5f);
            expect (! (a == b));
        }
    }
};

static SequenceBuilderTests sequenceBuilderTests;
