/*
  ==============================================================================

   SynthVoice - one monophonic oscillator + low-pass filter + ADSR voice
   (internal-synth-voice spec, roadmap Phase 8 / internal-synth, Phase 2).

   JUCE-aware (juce_dsp, juce_audio_basics): a documented, scoped exception to
   this project's JUCE-free-core convention (design.md's "Scoped convention
   exception"). Built on juce::dsp::Oscillator, juce::dsp::StateVariableTPTFilter
   and juce::ADSR.

   The pulse waveform reads a LIVE `pulseWidth` member from inside its
   generator lambda (design.md Decision 3), which captures `this`. A copy or
   move of SynthVoice would leave that lambda's captured `this` dangling
   inside the moved-from/copied instance, so the class is explicitly
   non-copyable AND non-movable.

   prepare() performs every allocating call (Oscillator::initialise/prepare,
   StateVariableTPTFilter::prepare, ADSR::setSampleRate/setParameters) -
   design.md Decision 5. noteOn()/noteOff()/render() allocate nothing.

   Phase 4: owns one Lfo (design.md Decision 2), modulating exactly one
   destination per the fixed patch (pitch / cutoff / amplitude / pulse
   width). Modulation is recomputed once per CONTROL BLOCK of 32 samples,
   subdividing render()'s segment - never per sample, because
   setCutoffFrequency() costs a std::tan per call. Pitch modulation calls
   oscillator.setFrequency(f, true) with force=true so the LFO does not
   fight the oscillator's 0.05s frequency ramp (verified-corrections
   table).

  ==============================================================================
*/

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include "synth/Lfo.h"
#include "synth/SynthPatch.h"

namespace berlin
{

class SynthVoice
{
public:
    SynthVoice() = default;

    JUCE_DECLARE_NON_COPYABLE (SynthVoice)
    JUCE_DECLARE_NON_MOVEABLE (SynthVoice)

    // Message thread; allocates (Oscillator::initialise/prepare,
    // StateVariableTPTFilter::prepare, ADSR::setSampleRate/setParameters).
    void prepare (const juce::dsp::ProcessSpec& spec, const SynthPatch& patch);

    // ---- AUDIO THREAD; allocation-, lock- and log-free ----
    void noteOn (float frequencyHz) noexcept;
    void noteOff() noexcept;
    void render (float* left, float* right, int numSamples) noexcept;
    void reset() noexcept;

private:
    void applyWaveform (Waveform waveform);
    void updateLfoModulation() noexcept;   // recomputed once per control block (Decision 2)

    juce::dsp::Oscillator<float> oscillator;
    juce::dsp::StateVariableTPTFilter<float> filter;
    juce::ADSR adsr;
    Lfo lfo;

    float pulseWidth     = 0.5f;   // live-read by the pulse generator lambda (Decision 3)
    float basePulseWidth = 0.5f;   // unmodulated patch value; pulseWidth = base + LFO offset
    float baseCutoffHz   = 4000.0f;
    float maxCutoffHz    = 20000.0f;   // ~0.49x prepared sample rate, set in prepare()
    float baseFrequencyHz = 440.0f;    // set on noteOn(); pitch LFO modulates around this
    float lfoDepth         = 0.0f;
    float amplitudeLfoGain = 1.0f;     // only applied when lfoDestination == amplitude
    LfoDestination lfoDestination = LfoDestination::pitch;
};

} // namespace berlin
