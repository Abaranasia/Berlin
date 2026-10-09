/*
  ==============================================================================

   Lfo - a hand-rolled, JUCE-free double phase accumulator (design.md
   Decision 2, roadmap Phase 8 / internal-synth, Phase 4).

   juce::dsp::LFO does not exist in the pinned JUCE 9.0.1 checkout (verified
   in design.md's verified-corrections table), so this is a from-scratch
   ~20-line class, directly unit-testable and deterministic. It is evaluated
   at CONTROL RATE (once per 32-sample block) by its caller (SynthVoice),
   never per sample - see SynthVoice::render.

   JUCE-free: standard library only (<cmath> for std::sin), per design.md's
   scoped convention exception guardrail (a) - Lfo stays JUCE-free even
   though SynthVoice/SynthEngine/SynthEffects are JUCE-aware.

  ==============================================================================
*/

#pragma once

namespace berlin
{

class Lfo
{
public:
    Lfo() = default;

    // Not on the audio thread's hot path (SynthVoice::prepare calls this),
    // but contains no allocation either way - plain scalar state.
    void prepare (double newSampleRate) noexcept;

    // ---- AUDIO THREAD; allocation-, lock- and log-free ----
    void setRate (float newRateHz) noexcept;
    void advance (int numSamples) noexcept;
    double getValue() const noexcept;   // current phase's value, in [-1, 1]
    void reset() noexcept;              // zeroes phase; keeps sampleRate/rate

private:
    double sampleRate = 44100.0;
    double rateHz     = 0.0;
    double phase      = 0.0;   // wrapped into [0, 1)
};

} // namespace berlin
