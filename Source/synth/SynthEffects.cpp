/*
  ==============================================================================

   SynthEffects - out-of-line definitions (internal-synth-output spec,
   roadmap Phase 8 / internal-synth, Phase 4; tempo-delay-ui Phase 8's
   atomic-setter rewrite).

  ==============================================================================
*/

#include "synth/SynthEffects.h"

#include <cmath>

namespace
{
    // ~100ms one-pole time constant for the delay-time glide (design.md
    // Decision 6) - long enough that a manual slider drag or a tempo-synced
    // recompute never clicks, short enough that it settles well within a
    // human's perception of "the knob responded".
    constexpr float kDelayGlideTauSeconds = 0.1f;
}

namespace berlin
{

// kMaxDelaySeconds now lives in SynthPatch.h (design.md D5, tempo-delay-ui
// Phase 7) - raised 2.0f -> 3.0f so every tempo-synced division is reachable
// at every legal BPM. Referenced unqualified here since this .cpp is already
// inside `namespace berlin`.

void SynthEffects::prepare (const juce::dsp::ProcessSpec& spec, const SynthPatch& patch)
{
    sampleRate = spec.sampleRate;

    delayLine.setMaximumDelayInSamples (static_cast<int> (spec.sampleRate * kMaxDelaySeconds));
    delayLine.prepare (spec);

    // Snapped (not glided) on prepare - the glide exists to smooth LIVE
    // changes while audio is flowing, not the initial seed.
    currentDelayTimeSamples = static_cast<float> (patch.delayTimeSeconds * spec.sampleRate);
    delayLine.setDelay (currentDelayTimeSamples);

    target.delayTimeSeconds.store (patch.delayTimeSeconds, std::memory_order_relaxed);
    target.delayFeedback.store (patch.delayFeedback, std::memory_order_relaxed);
    target.delayMix.store (patch.delayMix, std::memory_order_relaxed);
    target.reverbRoomSize.store (patch.reverbRoomSize, std::memory_order_relaxed);
    target.reverbDamping.store (patch.reverbDamping, std::memory_order_relaxed);
    target.reverbWetLevel.store (patch.reverbWetLevel, std::memory_order_relaxed);
    target.reverbDryLevel.store (patch.reverbDryLevel, std::memory_order_relaxed);

    delayFeedback = patch.delayFeedback;
    delayMix      = patch.delayMix;
    delayFeedbackRampStep = 0.0f;
    delayMixRampStep      = 0.0f;

    cachedReverbParams.roomSize = patch.reverbRoomSize;
    cachedReverbParams.damping  = patch.reverbDamping;
    cachedReverbParams.wetLevel = patch.reverbWetLevel;
    cachedReverbParams.dryLevel = patch.reverbDryLevel;
    reverb.setParameters (cachedReverbParams);
    reverb.prepare (spec);

    reset();
}

void SynthEffects::applyParameters (int numSamples) noexcept
{
    // ---- Delay time: one-pole glide toward the atomic target, tracked in SAMPLES ----
    const float targetSeconds = clampParameter (target.delayTimeSeconds.load (std::memory_order_relaxed),
                                                 kMinDelayTimeSeconds, kMaxDelaySeconds);
    const float targetSamples = static_cast<float> (targetSeconds * sampleRate);

    if (sampleRate > 0.0 && numSamples > 0)
    {
        const float coeff = std::exp (-static_cast<float> (numSamples)
                                       / (kDelayGlideTauSeconds * static_cast<float> (sampleRate)));
        currentDelayTimeSamples = targetSamples + (currentDelayTimeSamples - targetSamples) * coeff;
    }
    else
    {
        currentDelayTimeSamples = targetSamples;
    }

    delayLine.setDelay (currentDelayTimeSamples);

    // ---- Feedback / mix: per-sample linear ramp, step computed once per call ----
    const float feedbackEnd = clampParameter (target.delayFeedback.load (std::memory_order_relaxed),
                                               kMinDelayFeedback, kMaxDelayFeedback);
    const float mixEnd = clampParameter (target.delayMix.load (std::memory_order_relaxed),
                                          kMinDelayMix, kMaxDelayMix);

    delayFeedbackRampStep = numSamples > 0 ? (feedbackEnd - delayFeedback) / static_cast<float> (numSamples) : 0.0f;
    delayMixRampStep      = numSamples > 0 ? (mixEnd - delayMix)      / static_cast<float> (numSamples) : 0.0f;

    // ---- Reverb: change-gated against the cached copy (Decision 6, mirrors
    // SynthVoice's ADSR gate) - reverb.setParameters() is a real DSP
    // recalculation, not a cheap store, so an unconditional call every block
    // would be wasteful when nothing changed. ----
    juce::dsp::Reverb::Parameters newReverbParams;
    newReverbParams.roomSize = clampParameter (target.reverbRoomSize.load (std::memory_order_relaxed),
                                                kMinReverbRoomSize, kMaxReverbRoomSize);
    newReverbParams.damping  = clampParameter (target.reverbDamping.load (std::memory_order_relaxed),
                                                kMinReverbDamping, kMaxReverbDamping);
    newReverbParams.wetLevel = clampParameter (target.reverbWetLevel.load (std::memory_order_relaxed),
                                                kMinReverbWetLevel, kMaxReverbWetLevel);
    newReverbParams.dryLevel = clampParameter (target.reverbDryLevel.load (std::memory_order_relaxed),
                                                kMinReverbDryLevel, kMaxReverbDryLevel);

    if (newReverbParams.roomSize != cachedReverbParams.roomSize
        || newReverbParams.damping  != cachedReverbParams.damping
        || newReverbParams.wetLevel != cachedReverbParams.wetLevel
        || newReverbParams.dryLevel != cachedReverbParams.dryLevel)
    {
        reverb.setParameters (newReverbParams);
        cachedReverbParams = newReverbParams;
    }
}

void SynthEffects::process (juce::dsp::AudioBlock<float>& block) noexcept
{
    const int numSamples  = static_cast<int> (block.getNumSamples());
    const int numChannels = static_cast<int> (block.getNumChannels());

    applyParameters (numSamples);   // once per process() call (Decision 6) - not per sample

    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* data = block.getChannelPointer (static_cast<size_t> (ch));

        // Ramp is recomputed IDENTICALLY for every channel from the same
        // start/step (Decision 6), so stereo stays phase-coherent.
        float feedback = delayFeedback;
        float mix      = delayMix;

        for (int i = 0; i < numSamples; ++i)
        {
            const float inputSample   = data[i];
            const float delayedSample = delayLine.popSample (ch);

            delayLine.pushSample (ch, inputSample + delayedSample * feedback);

            data[i] = inputSample * (1.0f - mix) + delayedSample * mix;

            feedback += delayFeedbackRampStep;
            mix      += delayMixRampStep;
        }
    }

    // Commit the ramp's end value once, after every channel has walked the
    // identical ramp (not per-channel, which would double-advance it).
    delayFeedback += delayFeedbackRampStep * static_cast<float> (numSamples);
    delayMix      += delayMixRampStep      * static_cast<float> (numSamples);

    juce::dsp::ProcessContextReplacing<float> context (block);
    reverb.process (context);
}

void SynthEffects::reset() noexcept
{
    delayLine.reset();
    reverb.reset();
}

void SynthEffects::setDelayTimeSeconds (float newDelayTimeSeconds) noexcept
{
    target.delayTimeSeconds.store (clampParameter (newDelayTimeSeconds, kMinDelayTimeSeconds, kMaxDelaySeconds),
                                    std::memory_order_relaxed);
}

void SynthEffects::setDelayFeedback (float newDelayFeedback) noexcept
{
    target.delayFeedback.store (clampParameter (newDelayFeedback, kMinDelayFeedback, kMaxDelayFeedback),
                                 std::memory_order_relaxed);
}

void SynthEffects::setDelayMix (float newDelayMix) noexcept
{
    target.delayMix.store (clampParameter (newDelayMix, kMinDelayMix, kMaxDelayMix), std::memory_order_relaxed);
}

void SynthEffects::setReverbRoomSize (float newReverbRoomSize) noexcept
{
    target.reverbRoomSize.store (clampParameter (newReverbRoomSize, kMinReverbRoomSize, kMaxReverbRoomSize),
                                  std::memory_order_relaxed);
}

void SynthEffects::setReverbDamping (float newReverbDamping) noexcept
{
    target.reverbDamping.store (clampParameter (newReverbDamping, kMinReverbDamping, kMaxReverbDamping),
                                 std::memory_order_relaxed);
}

void SynthEffects::setReverbWetLevel (float newReverbWetLevel) noexcept
{
    target.reverbWetLevel.store (clampParameter (newReverbWetLevel, kMinReverbWetLevel, kMaxReverbWetLevel),
                                  std::memory_order_relaxed);
}

void SynthEffects::setReverbDryLevel (float newReverbDryLevel) noexcept
{
    target.reverbDryLevel.store (clampParameter (newReverbDryLevel, kMinReverbDryLevel, kMaxReverbDryLevel),
                                  std::memory_order_relaxed);
}

} // namespace berlin
