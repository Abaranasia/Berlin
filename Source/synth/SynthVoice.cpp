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
    constexpr size_t kLookupTableSize = 128;   // saw/square/triangle only - pulse forces n = 0 (Decision 3)
    constexpr int    kControlBlockSize = 32;   // Decision 2: LFO/cutoff modulation recomputed at this rate, never per-sample

    constexpr float kPitchModSemitoneRange  = 12.0f;   // full depth = +/-1 octave around baseFrequencyHz
    constexpr float kCutoffModOctaveRange   = 2.0f;    // full depth = +/-2 octaves around baseCutoffHz
    constexpr float kAmplitudeModDepthScale = 0.5f;    // full depth = tremolo down to (1 - depth) at trough
    constexpr float kPulseWidthModRange     = 0.45f;   // full depth = +/-0.45 around basePulseWidth
    constexpr float kMinCutoffHz            = 20.0f;
    constexpr float kMinPulseWidth          = 0.05f;
    constexpr float kMaxPulseWidth          = 0.95f;
}

void SynthVoice::applyWaveform (Waveform waveform)
{
    switch (waveform)
    {
        case Waveform::saw:
            oscillator.initialise ([] (float x)
            {
                return x / juce::MathConstants<float>::pi;
            }, kLookupTableSize);
            break;

        case Waveform::square:
            oscillator.initialise ([] (float x)
            {
                return x < 0.0f ? -1.0f : 1.0f;
            }, kLookupTableSize);
            break;

        case Waveform::triangle:
            oscillator.initialise ([] (float x)
            {
                const float absX = x < 0.0f ? -x : x;
                return (2.0f / juce::MathConstants<float>::pi) * absX - 1.0f;
            }, kLookupTableSize);
            break;

        case Waveform::pulse:
            // Decision 3: lookupTableNumPoints = 0 - a lookup table (n != 0)
            // bakes the function in once at initialise time, which would
            // freeze PWM. This lambda must be re-evaluated every sample so it
            // can read the LIVE `pulseWidth` member.
            oscillator.initialise ([this] (float x)
            {
                const float edge = juce::MathConstants<float>::pi * (2.0f * pulseWidth - 1.0f);
                return x < edge ? -1.0f : 1.0f;
            }, 0);
            break;
    }
}

void SynthVoice::prepare (const juce::dsp::ProcessSpec& spec, const SynthPatch& patch)
{
    const juce::dsp::ProcessSpec monoSpec { spec.sampleRate, spec.maximumBlockSize, 1 };

    pulseWidth     = patch.pulseWidth;
    basePulseWidth = patch.pulseWidth;
    applyWaveform (patch.waveform);
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
