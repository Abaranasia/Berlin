/*
  ==============================================================================

   Lfo - out-of-line definitions (design.md Decision 2, roadmap Phase 8 /
   internal-synth, Phase 4).

  ==============================================================================
*/

#include "synth/Lfo.h"

#include <cmath>

namespace berlin
{

namespace
{
    constexpr double kTwoPi = 6.283185307179586476925286766559;

    // Wrap into [0, 1) - std::fmod alone can return a small negative result
    // for a value just under 0, which this class never expects to see given
    // rateHz >= 0, but the guard is cheap and keeps the invariant explicit.
    double wrapPhase (double p) noexcept
    {
        p -= std::floor (p);
        return p;
    }
}

void Lfo::prepare (double newSampleRate) noexcept
{
    sampleRate = newSampleRate;
    reset();
}

void Lfo::setRate (float newRateHz) noexcept
{
    rateHz = static_cast<double> (newRateHz);
}

void Lfo::advance (int numSamples) noexcept
{
    if (sampleRate <= 0.0)
        return;

    const double phaseIncrementPerSample = rateHz / sampleRate;
    phase = wrapPhase (phase + phaseIncrementPerSample * static_cast<double> (numSamples));
}

double Lfo::getValue() const noexcept
{
    return std::sin (kTwoPi * phase);
}

void Lfo::reset() noexcept
{
    phase = 0.0;
}

} // namespace berlin
