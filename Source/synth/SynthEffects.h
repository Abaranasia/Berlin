/*
  ==============================================================================

   SynthEffects - terminal delay + reverb pass over a stereo AudioBlock view
   of SynthEngine's scratch buffer (internal-synth-output spec, "Terminal
   Delay And Reverb, Bypassed By Default", roadmap Phase 8 / internal-synth,
   Phase 4).

   JUCE-aware (juce_dsp): the same documented, scoped exception SynthVoice
   and SynthEngine already use (design.md's "Scoped convention exception").

   Decision 5: prepare() performs every allocating call -
   DelayLine::setMaximumDelayInSamples + prepare, Reverb::prepare. process()
   and reset() allocate nothing.

   Bypass mechanism note (design.md Decision 6): this class has NO enabled
   flag of its own and applies its full delay+reverb chain unconditionally
   whenever process() is called. The "bypassed by default" and
   enable/disable behaviour lives entirely in SynthEngine's own
   effectsEnabled atomic + edge latch, which decides whether to call
   process() at all. juce::dsp::Reverb::setEnabled is deliberately never
   used - it defaults to true and would silently contradict "bypassed by
   default" if relied upon (verified-corrections table).

   The delay stage is a simple feedback delay line, implemented by hand over
   juce::dsp::DelayLine's push/pop primitives (DelayLine itself has no
   built-in feedback or dry/wet mix). The reverb stage's dry/wet balance is
   handled internally by juce::dsp::Reverb::process via its own Parameters.

  ==============================================================================
*/

#pragma once

#include <juce_dsp/juce_dsp.h>

#include "synth/SynthPatch.h"

namespace berlin
{

class SynthEffects
{
public:
    SynthEffects() = default;

    // Message thread; allocates (DelayLine::setMaximumDelayInSamples +
    // prepare, Reverb::prepare).
    void prepare (const juce::dsp::ProcessSpec& spec, const SynthPatch& patch);

    // ---- AUDIO THREAD; allocation-, lock- and log-free ----
    void process (juce::dsp::AudioBlock<float>& block) noexcept;
    void reset() noexcept;   // hard-clears the delay line and reverb tail

private:
    juce::dsp::DelayLine<float> delayLine;
    juce::dsp::Reverb reverb;

    float delayFeedback = 0.0f;
    float delayMix      = 0.0f;
};

} // namespace berlin
