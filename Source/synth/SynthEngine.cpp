/*
  ==============================================================================

   SynthEngine - out-of-line definitions (internal-synth-output spec, roadmap
   Phase 8 / internal-synth, Phase 3).

  ==============================================================================
*/

#include "synth/SynthEngine.h"

namespace berlin
{

void SynthEngine::prepare (const juce::dsp::ProcessSpec& spec)
{
    scratch.setSize (2, static_cast<int> (spec.maximumBlockSize));
    voice.prepare (spec, kDefaultPatch);
    effects.prepare (spec, kDefaultPatch);

    reset();
}

void SynthEngine::render (const StepEventBuffer& events,
                           juce::AudioBuffer<float>& destination,
                           int startSample, int numSamples) noexcept
{
    jassert (numSamples <= scratch.getNumSamples());
    numSamples = juce::jmin (numSamples, scratch.getNumSamples());

    if (numSamples <= 0)
        return;

    const bool enabledNow = enabled.load (std::memory_order_relaxed);

    if (! enabledNow)
    {
        if (wasEnabled)
        {
            voice.reset();
            wasEnabled = false;
        }

        return;
    }

    wasEnabled = true;

    float* left  = scratch.getWritePointer (0);
    float* right = scratch.getWritePointer (1);

    int cursor = 0;

    for (int i = 0; i < events.size(); ++i)
    {
        const StepEvent& event = events[i];

        if (event.sampleOffset < cursor || event.sampleOffset >= numSamples)
            continue;   // outside this render window (already passed, or belongs to a later block)

        const int segmentLength = event.sampleOffset - cursor;
        if (segmentLength > 0)
        {
            voice.render (left + cursor, right + cursor, segmentLength);
            cursor = event.sampleOffset;
        }

        if (event.isNoteOn)
        {
            voice.noteOn (static_cast<float> (juce::MidiMessage::getMidiNoteInHertz (event.note)));
            currentNote = event.note;
        }
        else if (event.note == currentNote)
        {
            voice.noteOff();
            currentNote = -1;
        }
        // else: note-off for a note this voice is not currently sounding - ignored (Decision 7)
    }

    if (cursor < numSamples)
        voice.render (left + cursor, right + cursor, numSamples - cursor);

    const bool effectsEnabledNow = effectsEnabled.load (std::memory_order_relaxed);

    if (effectsEnabledNow)
    {
        wasEffectsEnabled = true;

        juce::dsp::AudioBlock<float> scratchBlock (scratch);
        auto activeBlock = scratchBlock.getSubBlock (0, static_cast<size_t> (numSamples));
        effects.process (activeBlock);
    }
    else if (wasEffectsEnabled)
    {
        // true->false edge: hard-clear the tail exactly once so no new
        // processed tail is introduced and no click/glitch is audible at
        // the transition (internal-synth-output: Terminal Delay And
        // Reverb).
        effects.reset();
        wasEffectsEnabled = false;
    }

    const int numDestinationChannels = juce::jmin (destination.getNumChannels(), scratch.getNumChannels());
    for (int ch = 0; ch < numDestinationChannels; ++ch)
        destination.addFrom (ch, startSample, scratch, ch, 0, numSamples);
}

void SynthEngine::reset() noexcept
{
    voice.reset();
    effects.reset();
    scratch.clear();
    currentNote = -1;
    wasEnabled = enabled.load (std::memory_order_relaxed);
    wasEffectsEnabled = effectsEnabled.load (std::memory_order_relaxed);
}

void SynthEngine::setEnabled (bool shouldBeEnabled) noexcept
{
    enabled.store (shouldBeEnabled, std::memory_order_relaxed);
}

void SynthEngine::setEffectsEnabled (bool shouldBeEnabled) noexcept
{
    effectsEnabled.store (shouldBeEnabled, std::memory_order_relaxed);
}

} // namespace berlin
