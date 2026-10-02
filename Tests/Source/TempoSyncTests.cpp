/*
  ==============================================================================

   TempoSync tests (tempo-control spec, Phase 1). RED first: Source/core/
   TempoSync.h exposes no SyncDivision/delaySecondsFor yet (Phase 0 registered
   an empty stub), so this suite must fail to compile until Phase 1.2 adds
   them. Phase 7 extends this same file with the kMaxDelaySeconds bound test
   once that constant moves to SynthPatch.h.

  ==============================================================================
*/

#include <juce_core/juce_core.h>

#include <algorithm>
#include <cmath>

#include "core/TempoSync.h"
#include "synth/SynthPatch.h"

namespace
{
    struct DivisionFactor
    {
        berlin::SyncDivision division;
        double factor;
    };
}

class TempoSyncTests final : public juce::UnitTest
{
public:
    TempoSyncTests() : juce::UnitTest ("TempoSync", "Berlin") {}

    void runTest() override
    {
        beginTest ("delaySecondsFor(120, quarter) == 0.5s");
        {
            expectWithinAbsoluteError (berlin::delaySecondsFor (120.0, berlin::SyncDivision::quarter),
                                       0.5, 1.0e-9);
        }

        beginTest ("delaySecondsFor(90, dottedEighth) == 0.5s");
        {
            // (60/90) * 0.75 = 0.5
            expectWithinAbsoluteError (berlin::delaySecondsFor (90.0, berlin::SyncDivision::dottedEighth),
                                       0.5, 1.0e-9);
        }

        beginTest ("eighthTriplet is exactly 1/3 of the quarter-note seconds at any BPM");
        {
            for (const double bpm : { 40.0, 120.0, 240.0 })
            {
                const double quarterSeconds = berlin::delaySecondsFor (bpm, berlin::SyncDivision::quarter);
                const double tripletSeconds = berlin::delaySecondsFor (bpm, berlin::SyncDivision::eighthTriplet);

                expectWithinAbsoluteError (tripletSeconds, quarterSeconds / 3.0, 1.0e-9);
            }
        }

        beginTest ("all 6 divisions at 40/120/240 BPM match (60/bpm) * factorFor(division)");
        {
            const DivisionFactor divisions[] {
                { berlin::SyncDivision::half,          2.0 },
                { berlin::SyncDivision::quarter,       1.0 },
                { berlin::SyncDivision::dottedEighth,  0.75 },
                { berlin::SyncDivision::eighth,        0.5 },
                { berlin::SyncDivision::eighthTriplet, 1.0 / 3.0 },
                { berlin::SyncDivision::sixteenth,     0.25 },
            };

            for (const double bpm : { 40.0, 120.0, 240.0 })
            {
                for (const auto& d : divisions)
                {
                    const double expected = (60.0 / bpm) * d.factor;
                    expectWithinAbsoluteError (berlin::delaySecondsFor (bpm, d.division), expected, 1.0e-9);
                }
            }
        }

        beginTest ("bpm <= 0 returns 0 for every division");
        {
            expectEquals (berlin::delaySecondsFor (0.0, berlin::SyncDivision::quarter), 0.0);
            expectEquals (berlin::delaySecondsFor (-10.0, berlin::SyncDivision::half), 0.0);
        }

        // ---- Phase 7 (internal-synth-output spec, design.md D5): kMaxDelaySeconds
        // now lives in SynthPatch.h, raised 2.0f -> 3.0f so every sync division is
        // reachable at kMinBpm without ever hitting the clamp in normal Sync use. ----

        beginTest ("the longest sync division at kMinBpm reaches exactly kMaxDelaySeconds, never exceeds it");
        {
            double maxSeconds = 0.0;

            for (int i = 0; i < berlin::kNumSyncDivisions; ++i)
            {
                const auto division = static_cast<berlin::SyncDivision> (i);
                maxSeconds = std::max (maxSeconds, berlin::delaySecondsFor (berlin::kMinBpm, division));
            }

            // (60/40) * 2.0 (half note factor) == 3.0 exactly.
            expectWithinAbsoluteError (maxSeconds, static_cast<double> (berlin::kMaxDelaySeconds), 1.0e-9);

            expectWithinAbsoluteError (berlin::delaySecondsFor (berlin::kMinBpm, berlin::SyncDivision::half),
                                       static_cast<double> (berlin::kMaxDelaySeconds), 1.0e-9);

            // Every other division at kMinBpm stays strictly below the bound - only
            // half hits it exactly, so a hypothetical longer division added later
            // would fail this test loudly rather than silently exceeding the bound.
            for (int i = 0; i < berlin::kNumSyncDivisions; ++i)
            {
                const auto division = static_cast<berlin::SyncDivision> (i);
                if (division == berlin::SyncDivision::half)
                    continue;

                expect (berlin::delaySecondsFor (berlin::kMinBpm, division) < static_cast<double> (berlin::kMaxDelaySeconds));
            }
        }

        // ---- delay-time-recommendation spec (Slice 3/5, Req 1-2): display
        // helpers layered on top of delaySecondsFor - JUCE-free, unit-testable. ----

        beginTest ("delayMillisecondsFor matches std::lround(delaySecondsFor(bpm, division) * 1000) at every division");
        {
            const double bpms[] { 160.0, 120.0, 240.0, 40.0 };

            for (const double bpm : bpms)
            {
                for (int i = 0; i < berlin::kNumSyncDivisions; ++i)
                {
                    const auto division = static_cast<berlin::SyncDivision> (i);
                    const int expected = static_cast<int> (std::lround (berlin::delaySecondsFor (bpm, division) * 1000.0));
                    expectEquals (berlin::delayMillisecondsFor (bpm, division), expected);
                }
            }
        }

        beginTest ("delayMillisecondsFor exact values at 160 BPM (spec Req 2)");
        {
            const int expectedMs[] { 750, 375, 281, 188, 125, 94 };

            for (int i = 0; i < berlin::kNumSyncDivisions; ++i)
                expectEquals (berlin::delayMillisecondsFor (160.0, static_cast<berlin::SyncDivision> (i)), expectedMs[i]);
        }

        beginTest ("delayMillisecondsFor exact values at 120 BPM (spec Req 2)");
        {
            const int expectedMs[] { 1000, 500, 375, 250, 167, 125 };

            for (int i = 0; i < berlin::kNumSyncDivisions; ++i)
                expectEquals (berlin::delayMillisecondsFor (120.0, static_cast<berlin::SyncDivision> (i)), expectedMs[i]);
        }

        beginTest ("delayMillisecondsFor exact values at 240 BPM, the upper BPM bound (spec Req 2)");
        {
            const int expectedMs[] { 500, 250, 188, 125, 83, 63 };

            for (int i = 0; i < berlin::kNumSyncDivisions; ++i)
                expectEquals (berlin::delayMillisecondsFor (240.0, static_cast<berlin::SyncDivision> (i)), expectedMs[i]);
        }

        beginTest ("delayMillisecondsFor(40, half) stays expressed in milliseconds at or above 1000 ms");
        {
            expectEquals (berlin::delayMillisecondsFor (40.0, berlin::SyncDivision::half), 3000);
        }

        beginTest ("delayMillisecondsFor returns 0 for every division when bpm <= 0 (non-positive BPM guard)");
        {
            for (int i = 0; i < berlin::kNumSyncDivisions; ++i)
            {
                const auto division = static_cast<berlin::SyncDivision> (i);
                expectEquals (berlin::delayMillisecondsFor (0.0, division), 0);
                expectEquals (berlin::delayMillisecondsFor (-10.0, division), 0);
            }
        }

        beginTest ("divisionLabelFor returns the 6 labels in enum order");
        {
            const char* const expectedLabels[] { "1/2", "1/4", "1/8.", "1/8", "1/8T", "1/16" };

            for (int i = 0; i < berlin::kNumSyncDivisions; ++i)
                expectEquals (juce::String (berlin::divisionLabelFor (static_cast<berlin::SyncDivision> (i))),
                              juce::String (expectedLabels[i]));
        }

        beginTest ("formatDelayRecommendations(160) matches the exact spec string, enum order, \" | \" separator");
        {
            expectEquals (juce::String (berlin::formatDelayRecommendations (160.0)),
                          juce::String ("1/2 750 ms | 1/4 375 ms | 1/8. 281 ms | 1/8 188 ms | 1/8T 125 ms | 1/16 94 ms"));
        }

        beginTest ("formatDelayRecommendations(40) keeps the >=1000ms division in milliseconds, no seconds unit");
        {
            expectEquals (juce::String (berlin::formatDelayRecommendations (40.0)),
                          juce::String ("1/2 3000 ms | 1/4 1500 ms | 1/8. 1125 ms | 1/8 750 ms | 1/8T 500 ms | 1/16 375 ms"));
        }
    }
};

static TempoSyncTests tempoSyncTests;
