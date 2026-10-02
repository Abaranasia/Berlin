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

   tempo-delay-ui Phase 7 (design.md D5/D7): kMinBpm/kMaxBpm/kDefaultBpm and
   the delay/reverb bound constants now live here - the single canonical
   home - rather than as file-local constants scattered across
   BerlinAudioProcessor.h/SynthEffects.cpp, both of which previously defined
   local copies of some of these values as an interim measure before this
   phase landed (Phase 4's BerlinAudioProcessor::kMinBpm/kMaxBpm/kDefaultBpm
   are removed in favor of these). kMaxDelaySeconds moved from
   SynthEffects.cpp's anonymous namespace and was raised 2.0f -> 3.0f
   (design.md D5): at the kMinBpm floor the longest exposed sync division
   (half note) needs exactly 3.0s, so every division is reachable at every
   legal BPM and the clamp becomes a purely DEFENSIVE bound (Free-mode
   manual entry, untrusted preset XML), never routine Sync-mode behavior.
   kMaxDelayFeedback is held strictly below 1.0 so the delay's feedback loop
   is provably stable (a geometric series, not a runaway one) - see
   SynthEffectsTests.cpp's feedback-stability suite. delaySynced/
   delayDivision (below) ride inside SynthPatch alongside the other 8
   effects fields (design.md D7) rather than as a separate struct, so the
   whole live-adjustable sound stays one value.

  ==============================================================================
*/

#pragma once

#include <algorithm>

#include "core/TempoSync.h"

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

// tempo-control spec (design.md D7, Phase 7): the single canonical BPM range
// - BerlinAudioProcessor::setBpm clamps to these, tempoSlider's range mirrors
// them (BerlinAudioProcessorEditor).
inline constexpr double kMinBpm = 40.0, kMaxBpm = 240.0, kDefaultBpm = 120.0;

// internal-synth-output spec (design.md D5/D6, Phase 7): delay/reverb bounds.
// kMaxDelaySeconds RAISED 2.0f -> 3.0f (was SynthEffects.cpp's anonymous-
// namespace kMaxDelaySeconds) - see the file header comment above.
inline constexpr float kMinDelayTimeSeconds = 0.0f, kMaxDelaySeconds = 3.0f;
inline constexpr float kMinDelayFeedback = 0.0f,    kMaxDelayFeedback = 0.95f;   // < 1.0: provably-stable feedback loop
inline constexpr float kMinDelayMix = 0.0f,         kMaxDelayMix = 1.0f;
inline constexpr float kMinReverbRoomSize = 0.0f,   kMaxReverbRoomSize = 1.0f;
inline constexpr float kMinReverbDamping = 0.0f,    kMaxReverbDamping = 1.0f;
inline constexpr float kMinReverbWetLevel = 0.0f,   kMaxReverbWetLevel = 1.0f;
inline constexpr float kMinReverbDryLevel = 0.0f,   kMaxReverbDryLevel = 1.0f;
inline constexpr float kMinOutputLevel = 0.0f,      kMaxOutputLevel = 1.0f;   // preset-persistence bound only (Phase 11) - no live setter exists (design.md's Interfaces block)

// NaN compares false against both bounds, so a naive std::clamp passes it
// through unchanged - the one case its [lo, hi] guarantee doesn't cover.
// Found reachable via host-supplied preset/session XML ("nan" text parses to
// a real NaN) during the vst3-au-plugin review; clamp it to `lo` like any
// other out-of-range input, since NaN is exactly that: not a valid value.
constexpr float clampParameter (float v, float lo, float hi) noexcept
{
    if (v != v)
        return lo;

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

    // tempo-control spec (design.md D7, Phase 7): Free by default (matches
    // today's manual-delay-time behavior with no persisted BPM sync). When
    // true, delayTimeSeconds tracks delaySecondsFor(liveBpm, delayDivision)
    // instead of a manually-set value (BerlinAudioProcessorEditor's
    // recomputeSyncedDelayTime, Phase 10).
    bool         delaySynced   = false;
    SyncDivision delayDivision = SyncDivision::quarter;   // unused while delaySynced is false
};

inline constexpr SynthPatch kDefaultPatch {};

} // namespace berlin
