/*
  ==============================================================================

   Lfo tests (design.md Decision 2, roadmap Phase 8 / internal-synth Phase 4).
   RED first: Source/synth/Lfo.h does not exist yet, so this suite must fail
   to compile until Phase 4's production file is created.

   JUCE-free production class (Lfo is JUCE-free per the scoped convention
   exception's guardrail (a)), but the test file itself uses juce::UnitTest
   as the harness, matching every other suite in this project.

   Covers: exact period at a known rate; range stays in [-1, 1]; advance(n)
   equals n x advance(1).

  ==============================================================================
*/

#include <cmath>

#include <juce_core/juce_core.h>

#include "synth/Lfo.h"

class LfoTests final : public juce::UnitTest
{
public:
    LfoTests() : juce::UnitTest ("Lfo", "Berlin") {}

    void runTest() override
    {
        constexpr double sampleRate = 48000.0;

        beginTest ("exact period at a known rate: one full cycle returns to the same value");
        {
            berlin::Lfo lfo;
            lfo.prepare (sampleRate);
            lfo.setRate (100.0f);   // 48000 / 100 = 480 samples per cycle exactly

            const double valueAtPhaseZero = lfo.getValue();

            lfo.advance (480);   // exactly one full period

            expectWithinAbsoluteError (lfo.getValue(), valueAtPhaseZero, 1.0e-9);
        }

        beginTest ("range stays in [-1, 1] across many advances at an arbitrary rate");
        {
            berlin::Lfo lfo;
            lfo.prepare (sampleRate);
            lfo.setRate (7.3f);   // arbitrary, non-integer-period rate

            for (int block = 0; block < 500; ++block)
            {
                lfo.advance (37);   // arbitrary, non-control-rate-aligned step

                const double value = lfo.getValue();
                expect (value >= -1.0 && value <= 1.0);
            }
        }

        beginTest ("advance(n) equals n x advance(1)");
        {
            berlin::Lfo bulkAdvance;
            bulkAdvance.prepare (sampleRate);
            bulkAdvance.setRate (4.0f);

            berlin::Lfo stepAdvance;
            stepAdvance.prepare (sampleRate);
            stepAdvance.setRate (4.0f);

            constexpr int n = 137;   // arbitrary, not a clean divisor of the period

            bulkAdvance.advance (n);

            for (int i = 0; i < n; ++i)
                stepAdvance.advance (1);

            expectWithinAbsoluteError (bulkAdvance.getValue(), stepAdvance.getValue(), 1.0e-6);
        }

        beginTest ("setRate(0) holds phase steady - no modulation");
        {
            berlin::Lfo lfo;
            lfo.prepare (sampleRate);
            lfo.setRate (0.0f);

            const double before = lfo.getValue();
            lfo.advance (1000);
            expectWithinAbsoluteError (lfo.getValue(), before, 1.0e-12);
        }
    }
};

static LfoTests lfoTests;
