/*
  ==============================================================================

   SynthVoice - out-of-line definitions (internal-synth-voice spec, roadmap
   Phase 8 / internal-synth, Phase 2).

  ==============================================================================
*/

#include "synth/SynthVoice.h"

#include <cmath>

namespace berlin
{

namespace
{
    constexpr int    kControlBlockSize = 32;   // Decision 2: LFO/cutoff modulation recomputed at this rate, never per-sample

    constexpr float kPitchModSemitoneRange  = 12.0f;   // full depth = +/-1 octave around baseFrequencyHz
    constexpr float kCutoffModOctaveRange   = 2.0f;    // full depth = +/-2 octaves around baseCutoffHz
    constexpr float kAmplitudeModDepthScale = 0.5f;    // full depth = tremolo down to (1 - depth) at trough
    constexpr float kPulseWidthModRange     = 0.45f;   // full depth = +/-0.45 around basePulseWidth
    // kMinCutoffHz/kMinPulseWidth/kMaxPulseWidth now live in SynthPatch.h (Phase 9 parameter-controls).
}

void SynthVoice::setWaveform (Waveform newWaveform) noexcept
{
    waveform = newWaveform;
}

void SynthVoice::prepare (const juce::dsp::ProcessSpec& spec, const SynthPatch& patch)
{
    const juce::dsp::ProcessSpec monoSpec { spec.sampleRate, spec.maximumBlockSize, 1 };

    pulseWidth     = patch.pulseWidth;
    basePulseWidth = patch.pulseWidth;
    waveform       = patch.waveform;

    // Decision 1: a single 4-way generator lambda for every waveform, with
    // lookupTableNumPoints = 0 - table-free. Switching `waveform` mid-note is
    // therefore a plain member assignment (setWaveform() above); no
    // re-initialise, so live switching allocates nothing structurally. Called
    // exactly once, here in prepare().
    oscillator.initialise ([this] (float x) noexcept
    {
        switch (waveform)
        {
            case Waveform::saw:
                return x / juce::MathConstants<float>::pi;

            case Waveform::square:
                return x < 0.0f ? -1.0f : 1.0f;

            case Waveform::triangle:
            {
                const float absX = x < 0.0f ? -x : x;
                return (2.0f / juce::MathConstants<float>::pi) * absX - 1.0f;
            }

            case Waveform::pulse:
            {
                const float edge = juce::MathConstants<float>::pi * (2.0f * pulseWidth - 1.0f);
                return x < edge ? -1.0f : 1.0f;
            }
        }
        return 0.0f;
    }, 0);

    oscillator.prepare (monoSpec);

    baseCutoffHz = patch.cutoffHz;
    maxCutoffHz  = static_cast<float> (spec.sampleRate) * 0.49f;

    filter.prepare (monoSpec);
    filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    filter.setCutoffFrequency (patch.cutoffHz);
    filter.setResonance (patch.resonance);

    adsr.setSampleRate (spec.sampleRate);
    adsr.setParameters ({ patch.attack, patch.decay, patch.sustain, patch.release });

    lfo.prepare (spec.sampleRate);
    lfo.setRate (patch.lfoRateHz);
    lfoDepth       = patch.lfoDepth;
    lfoDestination = patch.lfoDestination;

    reset();
}

void SynthVoice::noteOn (float frequencyHz) noexcept
{
    baseFrequencyHz = frequencyHz;
    oscillator.setFrequency (frequencyHz, true);   // force=true: snap to pitch, no portamento glide
    adsr.noteOn();
}

void SynthVoice::noteOff() noexcept
{
    adsr.noteOff();
}

void SynthVoice::updateLfoModulation() noexcept
{
    const float lfoValue = static_cast<float> (lfo.getValue());

    switch (lfoDestination)
    {
        case LfoDestination::pitch:
        {
            const float semitoneOffset = lfoDepth * lfoValue * kPitchModSemitoneRange;
            const float modulatedFrequency = baseFrequencyHz * std::pow (2.0f, semitoneOffset / 12.0f);
            oscillator.setFrequency (modulatedFrequency, true);   // force=true (verified-corrections table)
            break;
        }

        case LfoDestination::cutoff:
        {
            const float octaveOffset = lfoDepth * lfoValue * kCutoffModOctaveRange;
            const float modulatedCutoff = juce::jlimit (kMinCutoffHz, maxCutoffHz,
                                                          baseCutoffHz * std::pow (2.0f, octaveOffset));
            filter.setCutoffFrequency (modulatedCutoff);   // control-rate only (std::tan per call)
            break;
        }

        case LfoDestination::amplitude:
            amplitudeLfoGain = 1.0f - lfoDepth * kAmplitudeModDepthScale * (1.0f - lfoValue);
            break;

        case LfoDestination::pulseWidth:
            pulseWidth = juce::jlimit (kMinPulseWidth, kMaxPulseWidth,
                                       basePulseWidth + lfoDepth * lfoValue * kPulseWidthModRange);
            break;
    }
}

void SynthVoice::render (float* left, float* right, int numSamples) noexcept
{
    int cursor = 0;

    while (cursor < numSamples)
    {
        const int blockLength = juce::jmin (kControlBlockSize, numSamples - cursor);

        updateLfoModulation();   // once per control block (Decision 2) - not per sample

        for (int i = 0; i < blockLength; ++i)
        {
            const float oscillatorSample = oscillator.processSample (0.0f);   // additive: input + generator(...)
            const float filteredSample   = filter.processSample (0, oscillatorSample);
            float sample = filteredSample * adsr.getNextSample();

            if (lfoDestination == LfoDestination::amplitude)
                sample *= amplitudeLfoGain;

            left[cursor + i]  = sample;
            right[cursor + i] = sample;
        }

        lfo.advance (blockLength);
        cursor += blockLength;
    }
}

void SynthVoice::reset() noexcept
{
    oscillator.reset();
    filter.reset();
    adsr.reset();
    lfo.reset();
}

} // namespace berlin
