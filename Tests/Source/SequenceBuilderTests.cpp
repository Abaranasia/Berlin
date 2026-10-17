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

        // ---- variable length (step-model) ----

        beginTest ("randomActiveCount keeps the 11-of-16 ratio and never drops below 1");
        {
            expectEquals (berlin::randomActiveCount (16), 11);
            expectEquals (berlin::randomActiveCount (1), 1);
            expectEquals (berlin::randomActiveCount (64), 44);
        }

        beginTest ("every mode builds a Sequence of exactly params.length steps, clamped to 1..kMaxSteps");
        {
            for (auto mode : { berlin::RhythmMode::random, berlin::RhythmMode::euclidean, berlin::RhythmMode::probability })
            {
                for (int length : { 1, 8, 24, 64 })
                {
                    auto params = paramsFor (mode, 5, 0, 0.5f);
                    params.length = length;
                    expectEquals (berlin::buildSeededSequence (seed, params).size(), length);
                }

                auto params = paramsFor (mode, 5, 0, 0.5f);
                params.length = 0;
                expectEquals (berlin::buildSeededSequence (seed, params).size(), 1);
                params.length = 500;
                expectEquals (berlin::buildSeededSequence (seed, params).size(), berlin::kMaxSteps);
            }
        }

        beginTest ("random mode at length 64 has randomActiveCount active steps");
        {
            auto params = paramsFor (berlin::RhythmMode::random, 5, 0, 0.5f);
            params.length = 64;
            const auto seq = berlin::buildSeededSequence (seed, params);

            int active = 0;
            for (int i = 0; i < seq.size(); ++i)
                if (seq[i].active)
                    ++active;

            expectEquals (active, 44);
        }

        beginTest ("euclidean pulses above the length are clamped and rotation wraps within it");
        {
            auto params = paramsFor (berlin::RhythmMode::euclidean, 20, 0, 0.5f);
            params.length = 8;
            const auto seq = berlin::buildSeededSequence (seed, params);

            for (int i = 0; i < seq.size(); ++i)
                expect (seq[i].active);

            params.pulses   = 3;
            params.rotation = 11;   // == 3 within length 8
            auto rotated = params;
            rotated.rotation = 3;
            expect (berlin::buildSeededSequence (seed, params) == berlin::buildSeededSequence (seed, rotated));
        }

        beginTest ("generation never sets skipped");
        {
            for (auto mode : { berlin::RhythmMode::random, berlin::RhythmMode::euclidean, berlin::RhythmMode::probability })
            {
                auto params = paramsFor (mode, 5, 0, 0.5f);
                params.length = 32;
                const auto seq = berlin::buildSeededSequence (seed, params);

                for (int i = 0; i < seq.size(); ++i)
                    expect (! seq[i].skipped);
            }
        }

        beginTest ("length defaults to 16 and an explicit 16 matches the default output");
        {
            const berlin::GenerationParams defaults;
            expectEquals (defaults.length, berlin::kNumSteps);

            for (auto mode : { berlin::RhythmMode::random, berlin::RhythmMode::euclidean, berlin::RhythmMode::probability })
            {
                auto explicit16 = paramsFor (mode, 5, 0, 0.5f);
                explicit16.length = 16;
                expect (berlin::buildSeededSequence (seed, paramsFor (mode, 5, 0, 0.5f))
                        == berlin::buildSeededSequence (seed, explicit16));
            }
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
