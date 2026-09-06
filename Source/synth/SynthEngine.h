/*
  ==============================================================================

   SynthEngine - owns the single SynthVoice, the stereo scratch buffer, and
   the synth-enable atomic; wires the StepEventBuffer stream into rendered
   audio (internal-synth-output spec, roadmap Phase 8 / internal-synth,
   Phase 3).

   JUCE-aware (juce_dsp, juce_audio_basics): the same documented, scoped
   exception SynthVoice already uses (design.md's "Scoped convention
   exception").

   Decision 1: owns exactly one SynthVoice by value - no pool, no stealing.
   Decision 4: `scratch` is a stereo AudioBuffer<float> the voice renders
   into starting at index 0; only the terminal addFrom in render() applies
   `startSample`, translating scratch's origin-0 space into the caller's
   `destination` buffer. Fixed-size: numSamples is CLAMPED to
   scratch.getNumSamples() (with a jassert), never resized on the audio
   thread.
   Decision 5: prepare() performs every allocating call (scratch.setSize,
   voice.prepare); render()/reset()/setEnabled() allocate nothing.
   Decision 6 (synth-enable half only - effectsEnabled lands in Phase 4):
   `enabled` is a std::atomic<bool> written from the message thread and read
   once per render() call with relaxed ordering. A private audio-thread-only
   `wasEnabled` edge latch resets the voice exactly once on the true->false
   edge, so a disabled synth cannot leave a sustaining envelope (drone).
   Decision 7: note-off is note-matched against `currentNote` - a stray or
   duplicated note-off cannot cut a note it did not originate.
   Decision 8: reset() hard-silences the voice and zeroes scratch, and (as of
   Phase 4) also clears the delay/reverb tail - the full hard-reset. It is
   called from MainComponent::releaseResources rather than replaying a
   flush event.

   Phase 4: owns a SynthEffects member (terminal delay+reverb) and a second
   independent atomic, effectsEnabled, with its own private
   wasEffectsEnabled edge latch (Decision 6's other half). render()'s
   terminal stage becomes `effectsEnabled ? effects.process(scratchBlock) :
   (dry, unchanged)`. On the effectsEnabled true->false edge, effects.reset()
   is called exactly once so no new processed tail is introduced and no
   click/glitch is audible at the transition. juce::dsp::Reverb::setEnabled
   is deliberately never used as the bypass mechanism (verified-corrections
   table / Decision 6's stated rationale).

  ==============================================================================
*/

#pragma once

#include <atomic>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include "playback/StepEventBuffer.h"
#include "synth/SynthEffects.h"
#include "synth/SynthPatch.h"
#include "synth/SynthVoice.h"

namespace berlin
{

class SynthEngine
{
public:
    SynthEngine() = default;

    // Message thread; allocates (scratch.setSize, voice.prepare, effects.prepare).
    void prepare (const juce::dsp::ProcessSpec& spec);

    // ---- AUDIO THREAD; allocation-, lock- and log-free ----
    void render (const StepEventBuffer& events,
                 juce::AudioBuffer<float>& destination,
                 int startSample, int numSamples) noexcept;
    void reset() noexcept;

    // Message thread -> atomic.
    void setEnabled (bool shouldBeEnabled) noexcept;
    void setEffectsEnabled (bool shouldBeEnabled) noexcept;

    // Message thread -> voice's atomic Parameters (parameter-controls Phase 9).
    // Thin one-line forwarders mirroring SynthVoice's setter signatures verbatim,
    // so MainComponent talks only to `synth`, never directly to the voice it owns.
    void setWaveform       (Waveform newWaveform) noexcept;
    void setCutoffHz       (float newCutoffHz) noexcept;
    void setResonance      (float newResonance) noexcept;
    void setPulseWidth     (float newPulseWidth) noexcept;
    void setAttackSeconds  (float newAttackSeconds) noexcept;
    void setDecaySeconds   (float newDecaySeconds) noexcept;
    void setSustain        (float newSustain) noexcept;
    void setReleaseSeconds (float newReleaseSeconds) noexcept;
    void setLfoRateHz      (float newLfoRateHz) noexcept;
    void setLfoDepth       (float newLfoDepth) noexcept;
    void setLfoDestination (LfoDestination newLfoDestination) noexcept;

private:
    SynthVoice voice;
    SynthEffects effects;
    juce::AudioBuffer<float> scratch;

    std::atomic<bool> enabled { true };
    bool wasEnabled = true;   // audio-thread-only edge latch (Decision 6)

    std::atomic<bool> effectsEnabled { false };
    bool wasEffectsEnabled = false;   // audio-thread-only edge latch (Decision 6, FX half)

    int currentNote = -1;     // note last turned on; note-off is matched against this (Decision 7)
};

} // namespace berlin
