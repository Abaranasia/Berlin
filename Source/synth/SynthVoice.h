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

   parameter-controls Phase 9: every parameter is now live user-adjustable
   while a note sounds. Message-thread setters clamp and write to the
   `target` struct of atomics (design.md Decision 2); applyParameters() -
   audio thread, head of every control block - copies them into the plain
   audio-thread-only members below. updateLfoModulation() was rewritten
   base-then-delta (Decision 3): every control block recomputes all four
   destinations from their base values and adds the LFO's delta to exactly
   the active one, so switching destination mid-note cannot leave a stale
   modulated value parked. adsr.setParameters() is gated on an actual change
   (Decision 4) - see cachedAdsrParams.

  ==============================================================================
*/

#pragma once

#include <atomic>

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
    // Seeds and clamps every live parameter (design.md Decision 5).
    void prepare (const juce::dsp::ProcessSpec& spec, const SynthPatch& patch);

    // ---- MESSAGE THREAD -> atomic; clamped here. Safe to call at any time,
    // including while a note sounds (design.md Decision 2). Applied to the
    // audio-thread-only members once per control block by applyParameters(). ----
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

    // ---- AUDIO THREAD; allocation-, lock- and log-free ----
    void noteOn (float frequencyHz) noexcept;
    void noteOff() noexcept;
    void render (float* left, float* right, int numSamples) noexcept;
    void reset() noexcept;

private:
    void applyParameters() noexcept;       // audio thread; head of every control block (Decision 2)
    void updateLfoModulation() noexcept;   // recomputed once per control block (Decision 2/3)

    juce::dsp::Oscillator<float> oscillator;
    juce::dsp::StateVariableTPTFilter<float> filter;
    juce::ADSR adsr;
    Lfo lfo;

    // Message thread writes, audio thread reads (relaxed) - design.md Decision 2.
    // Continuous parameters get no edge latch: latches exist for booleans with
    // reset semantics (SynthEngine::enabled/effectsEnabled), not for these.
    struct Parameters
    {
        std::atomic<Waveform> waveform { kDefaultPatch.waveform };
        std::atomic<float>    cutoffHz { kDefaultPatch.cutoffHz };
        std::atomic<float>    resonance { kDefaultPatch.resonance };
        std::atomic<float>    pulseWidth { kDefaultPatch.pulseWidth };
        std::atomic<float>    attack { kDefaultPatch.attack };
        std::atomic<float>    decay { kDefaultPatch.decay };
        std::atomic<float>    sustain { kDefaultPatch.sustain };
        std::atomic<float>    release { kDefaultPatch.release };
        std::atomic<float>    lfoRateHz { kDefaultPatch.lfoRateHz };
        std::atomic<float>    lfoDepth { kDefaultPatch.lfoDepth };
        std::atomic<LfoDestination> lfoDestination { kDefaultPatch.lfoDestination };
    };

    static_assert (std::atomic<float>::is_always_lock_free,
                   "SynthVoice::Parameters requires lock-free float atomics");
    static_assert (std::atomic<Waveform>::is_always_lock_free,
                   "SynthVoice::Parameters requires lock-free Waveform atomics");
    static_assert (std::atomic<LfoDestination>::is_always_lock_free,
                   "SynthVoice::Parameters requires lock-free LfoDestination atomics");

    Parameters target;

    // Live-read by the generator lambda every sample (design.md Decision 1) -
    // table-free (lookupTableNumPoints = 0), so switching waveform is a plain
    // member assignment: no re-initialise, no allocation, structurally.
    Waveform waveform = Waveform::saw;

    float pulseWidth     = 0.5f;   // live-read by the pulse generator lambda (Decision 3)
    float basePulseWidth = 0.5f;   // unmodulated patch value; pulseWidth = base + LFO offset
    float baseCutoffHz   = 4000.0f;
    float maxCutoffHz    = 20000.0f;   // ~0.49x prepared sample rate, set in prepare()
    float lastAppliedCutoffHz = 4000.0f;   // skips the std::tan-costing setCutoffFrequency() call when nothing moved
    float baseFrequencyHz = 440.0f;    // set on noteOn(); pitch LFO modulates around this
    float lfoDepth         = 0.0f;
    float amplitudeLfoGain = 1.0f;     // only applied when lfoDestination == amplitude
    LfoDestination lfoDestination = LfoDestination::pitch;

    // Cached so applyParameters() can gate adsr.setParameters() on an actual
    // change (design.md Decision 4) - an unconditional call would silently
    // retime an in-progress release via recalculateRates().
    juce::ADSR::Parameters cachedAdsrParams;
};

} // namespace berlin
