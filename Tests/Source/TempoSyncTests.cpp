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
    }
};

static TempoSyncTests tempoSyncTests;
