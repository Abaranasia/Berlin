/*
  ==============================================================================

   DspLinkSmoke test (internal-synth spec, Phase 1 "Module/Build Plumbing").
   Proves the test binary links and runs with the newly-registered
   juce_dsp / juce_audio_formats / juce_audio_basics modules. This is NOT
   real synth DSP - it merely constructs and prepares a
   juce::dsp::Oscillator<float> and asserts processSample returns a finite
   value, per design.md's Testing Strategy "Build" row.

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>

class DspLinkSmokeTests final : public juce::UnitTest
{
public:
    DspLinkSmokeTests() : juce::UnitTest ("DspLinkSmoke", "Berlin") {}

    void runTest() override
    {
        beginTest ("juce_dsp links into the test target and a prepared Oscillator produces a finite sample");
        {
            juce::dsp::Oscillator<float> osc;
            osc.initialise ([] (float x) { return std::sin (x); }, 128);

            juce::dsp::ProcessSpec spec { 48000.0, 512, 2 };
            osc.prepare (spec);
            osc.setFrequency (440.0f);

            const auto sample = osc.processSample (0.0f);
            expect (std::isfinite (sample));
        }
    }
};

static DspLinkSmokeTests dspLinkSmokeTests;
