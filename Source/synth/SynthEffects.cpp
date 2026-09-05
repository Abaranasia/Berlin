/*
  ==============================================================================

   SynthEffects - out-of-line definitions (internal-synth-output spec,
   roadmap Phase 8 / internal-synth, Phase 4).

  ==============================================================================
*/

#include "synth/SynthEffects.h"

namespace berlin
{

namespace
{
    constexpr float kMaxDelaySeconds = 2.0f;   // headroom above kDefaultPatch's 0.3s delay time
}

void SynthEffects::prepare (const juce::dsp::ProcessSpec& spec, const SynthPatch& patch)
{
    delayLine.setMaximumDelayInSamples (static_cast<int> (spec.sampleRate * kMaxDelaySeconds));
    delayLine.prepare (spec);
    delayLine.setDelay (static_cast<float> (patch.delayTimeSeconds * spec.sampleRate));

    delayFeedback = patch.delayFeedback;
    delayMix      = patch.delayMix;

    juce::dsp::Reverb::Parameters reverbParams;
    reverbParams.roomSize = patch.reverbRoomSize;
    reverbParams.damping  = patch.reverbDamping;
    reverbParams.wetLevel = patch.reverbWetLevel;
    reverbParams.dryLevel = patch.reverbDryLevel;
    reverb.setParameters (reverbParams);
    reverb.prepare (spec);

    reset();
}

void SynthEffects::process (juce::dsp::AudioBlock<float>& block) noexcept
{
    const int numSamples  = static_cast<int> (block.getNumSamples());
    const int numChannels = static_cast<int> (block.getNumChannels());

    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* data = block.getChannelPointer (static_cast<size_t> (ch));

        for (int i = 0; i < numSamples; ++i)
        {
            const float inputSample   = data[i];
            const float delayedSample = delayLine.popSample (ch);

            delayLine.pushSample (ch, inputSample + delayedSample * delayFeedback);

            data[i] = inputSample * (1.0f - delayMix) + delayedSample * delayMix;
        }
    }

    juce::dsp::ProcessContextReplacing<float> context (block);
    reverb.process (context);
}

void SynthEffects::reset() noexcept
{
    delayLine.reset();
    reverb.reset();
}

} // namespace berlin
