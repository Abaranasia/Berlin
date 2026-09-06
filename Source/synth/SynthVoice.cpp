/*
  ==============================================================================

   SynthVoice - out-of-line definitions (internal-synth-voice spec, roadmap
   Phase 8 / internal-synth, Phase 2; parameter-controls Phase 9 made every
   parameter live user-adjustable - see design.md Decisions 1-4).

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
    target.waveform.store (newWaveform, std::memory_order_relaxed);
}

void SynthVoice::setCutoffHz (float newCutoffHz) noexcept
{
    target.cutoffHz.store (clampParameter (newCutoffHz, kMinCutoffHz, kMaxCutoffHz), std::memory_order_relaxed);
}

void SynthVoice::setResonance (float newResonance) noexcept
{
    target.resonance.store (clampParameter (newResonance, kMinResonance, kMaxResonance), std::memory_order_relaxed);
}

void SynthVoice::setPulseWidth (float newPulseWidth) noexcept
{
    target.pulseWidth.store (clampParameter (newPulseWidth, kMinPulseWidth, kMaxPulseWidth), std::memory_order_relaxed);
}

void SynthVoice::setAttackSeconds (float newAttackSeconds) noexcept
{
    target.attack.store (clampParameter (newAttackSeconds, kMinAttackSeconds, kMaxAttackSeconds), std::memory_order_relaxed);
}

void SynthVoice::setDecaySeconds (float newDecaySeconds) noexcept
{
    target.decay.store (clampParameter (newDecaySeconds, kMinDecaySeconds, kMaxDecaySeconds), std::memory_order_relaxed);
}

void SynthVoice::setSustain (float newSustain) noexcept
{
    target.sustain.store (clampParameter (newSustain, kMinSustain, kMaxSustain), std::memory_order_relaxed);
}

void SynthVoice::setReleaseSeconds (float newReleaseSeconds) noexcept
{
    target.release.store (clampParameter (newReleaseSeconds, kMinReleaseSeconds, kMaxReleaseSeconds), std::memory_order_relaxed);
}

void SynthVoice::setLfoRateHz (float newLfoRateHz) noexcept
{
    target.lfoRateHz.store (clampParameter (newLfoRateHz, kMinLfoRateHz, kMaxLfoRateHz), std::memory_order_relaxed);
}

void SynthVoice::setLfoDepth (float newLfoDepth) noexcept
{
    target.lfoDepth.store (clampParameter (newLfoDepth, kMinLfoDepth, kMaxLfoDepth), std::memory_order_relaxed);
}

void SynthVoice::setLfoDestination (LfoDestination newLfoDestination) noexcept
{
    target.lfoDestination.store (newLfoDestination, std::memory_order_relaxed);
}

void SynthVoice::prepare (const juce::dsp::ProcessSpec& spec, const SynthPatch& patch)
{
    const juce::dsp::ProcessSpec monoSpec { spec.sampleRate, spec.maximumBlockSize, 1 };

    maxCutoffHz = static_cast<float> (spec.sampleRate) * 0.49f;

    const float clampedCutoffHz   = clampParameter (patch.cutoffHz, kMinCutoffHz, maxCutoffHz);
    const float clampedResonance  = clampParameter (patch.resonance, kMinResonance, kMaxResonance);
    const float clampedPulseWidth = clampParameter (patch.pulseWidth, kMinPulseWidth, kMaxPulseWidth);
    const float clampedAttack     = clampParameter (patch.attack, kMinAttackSeconds, kMaxAttackSeconds);
    const float clampedDecay      = clampParameter (patch.decay, kMinDecaySeconds, kMaxDecaySeconds);
    const float clampedSustain    = clampParameter (patch.sustain, kMinSustain, kMaxSustain);
    const float clampedRelease    = clampParameter (patch.release, kMinReleaseSeconds, kMaxReleaseSeconds);
    const float clampedLfoRateHz  = clampParameter (patch.lfoRateHz, kMinLfoRateHz, kMaxLfoRateHz);
    const float clampedLfoDepth   = clampParameter (patch.lfoDepth, kMinLfoDepth, kMaxLfoDepth);

    // Seed the cross-thread atomics with the clamped patch (design.md Decision 5).
    target.waveform.store (patch.waveform, std::memory_order_relaxed);
    target.cutoffHz.store (clampedCutoffHz, std::memory_order_relaxed);
    target.resonance.store (clampedResonance, std::memory_order_relaxed);
    target.pulseWidth.store (clampedPulseWidth, std::memory_order_relaxed);
    target.attack.store (clampedAttack, std::memory_order_relaxed);
    target.decay.store (clampedDecay, std::memory_order_relaxed);
    target.sustain.store (clampedSustain, std::memory_order_relaxed);
    target.release.store (clampedRelease, std::memory_order_relaxed);
    target.lfoRateHz.store (clampedLfoRateHz, std::memory_order_relaxed);
    target.lfoDepth.store (clampedLfoDepth, std::memory_order_relaxed);
    target.lfoDestination.store (patch.lfoDestination, std::memory_order_relaxed);

    // Seed the audio-thread-only members to match, so the very first control
    // block (before applyParameters() runs) already reflects the clamped patch.
    waveform       = patch.waveform;
    pulseWidth     = clampedPulseWidth;
    basePulseWidth = clampedPulseWidth;
    baseCutoffHz   = clampedCutoffHz;
    lastAppliedCutoffHz = clampedCutoffHz;
    lfoDepth       = clampedLfoDepth;
    lfoDestination = patch.lfoDestination;
    cachedAdsrParams = { clampedAttack, clampedDecay, clampedSustain, clampedRelease };

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

    filter.prepare (monoSpec);
    filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    filter.setCutoffFrequency (clampedCutoffHz);
    filter.setResonance (clampedResonance);

    adsr.setSampleRate (spec.sampleRate);
    adsr.setParameters (cachedAdsrParams);

    lfo.prepare (spec.sampleRate);
    lfo.setRate (clampedLfoRateHz);

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

void SynthVoice::applyParameters() noexcept
{
    waveform       = target.waveform.load (std::memory_order_relaxed);
    basePulseWidth = target.pulseWidth.load (std::memory_order_relaxed);
    baseCutoffHz   = target.cutoffHz.load (std::memory_order_relaxed);

    filter.setResonance (target.resonance.load (std::memory_order_relaxed));

    const juce::ADSR::Parameters newAdsrParams
    {
        target.attack.load (std::memory_order_relaxed),
        target.decay.load (std::memory_order_relaxed),
        target.sustain.load (std::memory_order_relaxed),
        target.release.load (std::memory_order_relaxed)
    };

    // Decision 4: adsr.setParameters() calls recalculateRates(), which
    // overwrites releaseRate with a value derived from sustain - discarding
    // the envelopeVal-derived rate noteOff() computed. An unconditional call
    // every control block would silently retime an in-progress release, so
    // this is gated on an actual change, not merely a performance nicety.
    const bool adsrChanged = newAdsrParams.attack  != cachedAdsrParams.attack
                           || newAdsrParams.decay   != cachedAdsrParams.decay
                           || newAdsrParams.sustain != cachedAdsrParams.sustain
                           || newAdsrParams.release != cachedAdsrParams.release;

    if (adsrChanged)
    {
        adsr.setParameters (newAdsrParams);
        cachedAdsrParams = newAdsrParams;
    }

    lfo.setRate (target.lfoRateHz.load (std::memory_order_relaxed));
    lfoDepth       = target.lfoDepth.load (std::memory_order_relaxed);
    lfoDestination = target.lfoDestination.load (std::memory_order_relaxed);
}

void SynthVoice::updateLfoModulation() noexcept
{
    const float lfoValue = static_cast<float> (lfo.getValue());

    // Decision 3: always recompute every destination from its base value,
    // then add the LFO's delta to exactly the active one, then commit all of
    // them unconditionally. Stateless-by-construction - no edge to miss, no
    // undo to get wrong - so switching destination mid-note cannot leave a
    // parameter parked at its last modulated value (closes the Phase 8 bug).
    float frequency        = baseFrequencyHz;
    float cutoff           = baseCutoffHz;
    float pulseWidthTarget = basePulseWidth;
    amplitudeLfoGain = 1.0f;

    switch (lfoDestination)
    {
        case LfoDestination::pitch:
        {
            const float semitoneOffset = lfoDepth * lfoValue * kPitchModSemitoneRange;
            frequency = baseFrequencyHz * std::pow (2.0f, semitoneOffset / 12.0f);
            break;
        }

        case LfoDestination::cutoff:
        {
            const float octaveOffset = lfoDepth * lfoValue * kCutoffModOctaveRange;
            cutoff = baseCutoffHz * std::pow (2.0f, octaveOffset);
            break;
        }

        case LfoDestination::amplitude:
            amplitudeLfoGain = 1.0f - lfoDepth * kAmplitudeModDepthScale * (1.0f - lfoValue);
            break;

        case LfoDestination::pulseWidth:
            pulseWidthTarget = basePulseWidth + lfoDepth * lfoValue * kPulseWidthModRange;
            break;
    }

    oscillator.setFrequency (frequency, true);   // force=true (verified-corrections table); unconditional, cheap

    const float clampedCutoff = juce::jlimit (kMinCutoffHz, maxCutoffHz, cutoff);
    if (clampedCutoff != lastAppliedCutoffHz)   // lastAppliedCutoffHz keeps the std::tan off the path when nothing moved
    {
        filter.setCutoffFrequency (clampedCutoff);
        lastAppliedCutoffHz = clampedCutoff;
    }

    pulseWidth = juce::jlimit (kMinPulseWidth, kMaxPulseWidth, pulseWidthTarget);
}

void SynthVoice::render (float* left, float* right, int numSamples) noexcept
{
    int cursor = 0;

    while (cursor < numSamples)
    {
        const int blockLength = juce::jmin (kControlBlockSize, numSamples - cursor);

        applyParameters();       // once per control block (Decision 2) - not per sample
        updateLfoModulation();   // once per control block (Decision 2/3) - not per sample

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
