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

   tempo-delay-ui Phase 8 (internal-synth-output spec, first-ever coverage
   for this class - design.md D6): every delay/reverb field is now live
   user-adjustable while the engine runs, mirroring SynthVoice's own
   Parameters/applyParameters split (SynthVoice.h/.cpp):
     - message-thread setters clamp (SynthPatch.h bounds) and
       store(relaxed) into the `target` struct of atomics;
     - applyParameters() - audio thread, head of every process() call -
       copies them into the plain audio-thread-only members below:
         * delay TIME glides ONE-POLE per call (~100ms tau) into
           currentDelayTimeSamples, then a single delayLine.setDelay() -
           DelayLine defaults to Linear interpolation, so a fractional
           glide reads as a tape-style pitch slide, not a click;
         * FEEDBACK/MIX use a per-sample LINEAR RAMP computed once per
           call (step = (end-start)/n) - identical for every channel in
           the same block, so stereo stays phase-coherent;
         * REVERB params are change-gated against a cached copy (mirrors
           SynthVoice's ADSR gate, SynthVoice.cpp Decision 4) -
           reverb.setParameters() is a real DSP recalculation, not a
           cheap store. NOT a click risk despite the instant call: JUCE's
           own juce::Reverb::setParameters() (juce_Reverb.h) stores
           roomSize/damping/wetLevel/dryLevel into internal
           SmoothedValue<float> members with a 10ms smoothTime (set in
           setSampleRate()), so processStereo()'s getNextValue() calls
           already ramp every one of these 4 fields sample-by-sample
           regardless of how abruptly setParameters() itself is called -
           confirmed both by reading that source and by an extreme
           (roomSize/damping 0.1->0.9 + full dry->wet crossfade,
           SynthEffectsTests.cpp) discontinuity probe across 8 sine phase
           offsets finding no click beyond the steady-state baseline
           (followup-fixes review investigation - the WARNING finding
           this addresses turned out to be a false positive: only `width`/
           `freezeMode` bypass this internal smoothing, and neither is
           exposed by SynthPatch).
   kMaxDelayFeedback (SynthPatch.h) is held strictly below 1.0 so the
   feedback loop is provably stable (a geometric series) - see
   SynthEffectsTests.cpp's feedback-stability suite.

  ==============================================================================
*/

#pragma once

#include <atomic>

#include <juce_dsp/juce_dsp.h>

#include "synth/SynthPatch.h"

namespace berlin
{

class SynthEffects
{
public:
    SynthEffects() = default;

    // Message thread; allocates (DelayLine::setMaximumDelayInSamples +
    // prepare, Reverb::prepare). Seeds and clamps every live parameter.
    void prepare (const juce::dsp::ProcessSpec& spec, const SynthPatch& patch);

    // ---- MESSAGE THREAD -> atomic; clamped here (SynthPatch.h bounds).
    // Safe to call at any time, including while process() is running
    // (design.md Decision 6). Applied to the audio-thread-only members once
    // per process() call by applyParameters(). ----
    void setDelayTimeSeconds (float newDelayTimeSeconds) noexcept;
    void setDelayFeedback    (float newDelayFeedback) noexcept;
    void setDelayMix         (float newDelayMix) noexcept;
    void setReverbRoomSize   (float newReverbRoomSize) noexcept;
    void setReverbDamping    (float newReverbDamping) noexcept;
    void setReverbWetLevel   (float newReverbWetLevel) noexcept;
    void setReverbDryLevel   (float newReverbDryLevel) noexcept;

    // ---- AUDIO THREAD; allocation-, lock- and log-free ----
    void process (juce::dsp::AudioBlock<float>& block) noexcept;
    void reset() noexcept;   // hard-clears the delay line and reverb tail

private:
    void applyParameters (int numSamples) noexcept;   // audio thread; head of every process() call

    juce::dsp::DelayLine<float> delayLine;
    juce::dsp::Reverb reverb;

    double sampleRate = 0.0;   // cached by prepare(); needed for seconds<->samples + the glide coefficient

    // Message thread writes, audio thread reads (relaxed) - design.md
    // Decision 6, mirrors SynthVoice::Parameters.
    struct Parameters
    {
        std::atomic<float> delayTimeSeconds { kDefaultPatch.delayTimeSeconds };
        std::atomic<float> delayFeedback    { kDefaultPatch.delayFeedback };
        std::atomic<float> delayMix         { kDefaultPatch.delayMix };
        std::atomic<float> reverbRoomSize   { kDefaultPatch.reverbRoomSize };
        std::atomic<float> reverbDamping    { kDefaultPatch.reverbDamping };
        std::atomic<float> reverbWetLevel   { kDefaultPatch.reverbWetLevel };
        std::atomic<float> reverbDryLevel   { kDefaultPatch.reverbDryLevel };
    };

    static_assert (std::atomic<float>::is_always_lock_free,
                   "SynthEffects::Parameters requires lock-free float atomics");

    Parameters target;

    // Audio-thread-only "current" values - the per-sample ramp anchor for
    // feedback/mix (Decision 6); delay time's one-pole glide state is
    // tracked directly in SAMPLES (currentDelayTimeSamples), since that is
    // the unit delayLine.setDelay() actually consumes.
    float delayFeedback = kDefaultPatch.delayFeedback;
    float delayMix      = kDefaultPatch.delayMix;
    float delayFeedbackRampStep = 0.0f;
    float delayMixRampStep      = 0.0f;

    float currentDelayTimeSamples = 0.0f;   // one-pole glide state, in SAMPLES; snapped (not glided) by prepare()

    juce::dsp::Reverb::Parameters cachedReverbParams;   // change-gate (Decision 6)
};

} // namespace berlin
