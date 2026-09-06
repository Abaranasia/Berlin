/*
  ==============================================================================

   SynthPatch - the sound definition for the internal synth's single voice
   (internal-synth-voice spec, roadmap Phase 9 / parameter-controls). One
   constexpr default patch seeds every live-adjustable parameter; every field
   below is now live user-adjustable while a note sounds via SynthVoice's
   message-thread setters (see design.md's Interfaces block) - this struct
   itself remains the immutable default/seed, not a UI-backed patch system.

   JUCE-free: standard library only, per design.md's scoped convention
   exception guardrail (a) - SynthPatch stays JUCE-free even though
   SynthVoice/SynthEngine are JUCE-aware, because its fields are pure data
   with no JUCE equivalent worth pulling the dependency in for.

   `pulseWidth` and `lfoDestination` are read here even though the LFO itself
   lands in Phase 4 - the patch shape is fixed for the whole feature so all
   fields exist from the start, unwired fields simply unused until then.

  ==============================================================================
*/

#pragma once

#include <algorithm>

namespace berlin
{

enum class Waveform { saw, square, pulse, triangle };
enum class LfoDestination { pitch, cutoff, amplitude, pulseWidth };

// Ranges/tapers pinned by design.md Decision 5. Setters clamp on the message
// thread to these bounds; the voice additionally clamps cutoff to the
// sample-rate-dependent maxCutoffHz = 0.49 * sr.
inline constexpr float kMinCutoffHz = 20.0f,        kMaxCutoffHz = 20000.0f;
inline constexpr float kMinResonance = 0.7071068f,  kMaxResonance = 8.0f;
inline constexpr float kMinPulseWidth = 0.05f,      kMaxPulseWidth = 0.95f;
inline constexpr float kMinAttackSeconds = 0.001f,  kMaxAttackSeconds = 4.0f;
inline constexpr float kMinDecaySeconds = 0.001f,   kMaxDecaySeconds = 4.0f;
inline constexpr float kMinSustain = 0.0f,          kMaxSustain = 1.0f;
inline constexpr float kMinReleaseSeconds = 0.005f, kMaxReleaseSeconds = 8.0f;
inline constexpr float kMinLfoRateHz = 0.05f,       kMaxLfoRateHz = 20.0f;
inline constexpr float kMinLfoDepth = 0.0f,         kMaxLfoDepth = 1.0f;

constexpr float clampParameter (float v, float lo, float hi) noexcept
{
    return std::clamp (v, lo, hi);
}

struct SynthPatch
{
    Waveform waveform = Waveform::saw;

    float cutoffHz   = 4000.0f;
    float resonance  = 0.7071068f;   // 1 / sqrt(2) - no resonance peaking (Butterworth)
    float pulseWidth = 0.5f;         // only meaningful for Waveform::pulse; also an LFO destination target

    float attack  = 0.01f;
    float decay   = 0.15f;
    float sustain = 0.7f;
    float release = 0.2f;

    float lfoRateHz               = 4.0f;
    float lfoDepth                = 0.0f;   // 0 = off; per-destination scaling applied where wired (Phase 4)
    LfoDestination lfoDestination = LfoDestination::pitch;

    float delayTimeSeconds = 0.3f;
    float delayFeedback    = 0.3f;
    float delayMix         = 0.35f;  // audible mix once SynthEngine::effectsEnabled is true - the
                                     // bypass-by-default gate lives on the engine's atomic (design.md
                                     // Decision 6), not on this send level

    float reverbRoomSize = 0.5f;
    float reverbDamping  = 0.5f;
    float reverbWetLevel = 0.3f;     // audible mix once SynthEngine::effectsEnabled is true (see above)
    float reverbDryLevel = 1.0f;

    float outputLevel = 0.8f;
};

inline constexpr SynthPatch kDefaultPatch {};

} // namespace berlin
