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

        auto paramsFor = [] (berlin::RhythmMode mode, int pulses, int rotation, float stepProbability)
        {
            berlin::GenerationParams params;
            params.mode            = mode;
            params.pulses          = pulses;
            params.rotation        = rotation;
            params.stepProbability = stepProbability;
            return params;
        };

        beginTest ("RhythmMode::random: same seed produces an identical Sequence");
        {
            const auto params = paramsFor (berlin::RhythmMode::random, 5, 0, 0.5f);
            const auto a = berlin::buildSeededSequence (seed, params);
            const auto b = berlin::buildSeededSequence (seed, params);
            expect (a == b);
        }

        beginTest ("RhythmMode::euclidean: same seed/pulses/rotation produces an identical Sequence");
        {
            const auto params = paramsFor (berlin::RhythmMode::euclidean, 7, 3, 0.5f);
            const auto a = berlin::buildSeededSequence (seed, params);
            const auto b = berlin::buildSeededSequence (seed, params);
            expect (a == b);
        }

        beginTest ("RhythmMode::probability: same seed/stepProbability produces an identical Sequence");
        {
            const auto params = paramsFor (berlin::RhythmMode::probability, 5, 0, 0.35f);
            const auto a = berlin::buildSeededSequence (seed, params);
            const auto b = berlin::buildSeededSequence (seed, params);
            expect (a == b);
        }

        beginTest ("Different seeds produce different Sequences for RhythmMode::random");
        {
            const auto params = paramsFor (berlin::RhythmMode::random, 5, 0, 0.5f);
            const auto a = berlin::buildSeededSequence (seed, params);
            const auto b = berlin::buildSeededSequence (seed + 1, params);
            expect (! (a == b));
        }

        beginTest ("Euclidean pulses/rotation change is reflected even with the same seed");
        {
            const auto a = berlin::buildSeededSequence (seed, paramsFor (berlin::RhythmMode::euclidean, 5, 0, 0.5f));
            const auto b = berlin::buildSeededSequence (seed, paramsFor (berlin::RhythmMode::euclidean, 9, 2, 0.5f));
            expect (! (a == b));
        }

        // ---- normalizePitchRange (scale-aware-generation design.md invariant) ----

        beginTest ("normalizePitchRange swaps an inverted pair so low <= high");
        {
            int low = 72, high = 36;
            berlin::normalizePitchRange (low, high);
            expect (low <= high);
            expectEquals (low, 36);
            expectEquals (high, 72);
        }

        beginTest ("normalizePitchRange clamps out-of-[0,127] bounds");
        {
            int low = -20, high = 200;
            berlin::normalizePitchRange (low, high);
            expect (low >= berlin::kMinPitch);
            expect (high <= berlin::kMaxPitch);
            expectEquals (low, 0);
            expectEquals (high, 127);
        }

        beginTest ("normalizePitchRange widens a sub-octave span upward (away from the ceiling) to at least kMinPitchRangeSpan");
        {
            int low = 60, high = 61;   // 1-semitone span, nowhere near the 127 ceiling
            berlin::normalizePitchRange (low, high);
            expect (high - low >= berlin::kMinPitchRangeSpan);
            expectEquals (low, 60);    // low must not move - widening happens upward here
            expectEquals (high, 72);
        }

        beginTest ("normalizePitchRange at the 127 ceiling widens downward, not upward past the ceiling");
        {
            int low = 125, high = 127;   // pinned at the ceiling, 2-semitone span
            berlin::normalizePitchRange (low, high);
            expectEquals (high, 127);   // high must not move past the ceiling
            expect (high - low >= berlin::kMinPitchRangeSpan);
            expect (low < 125);   // widened by moving low DOWN, not high up
        }

        beginTest ("normalizePitchRange is a no-op for an already-valid range");
        {
            int low = 36, high = 72;
            berlin::normalizePitchRange (low, high);
            expectEquals (low, 36);
            expectEquals (high, 72);
        }
    }
};

static SequenceBuilderTests sequenceBuilderTests;
