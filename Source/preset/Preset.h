/*
  ==============================================================================

   Preset - the in-memory unit a named preset save/load operates on
   (preset-persistence spec, roadmap Phase 11 / preset-system). JUCE-aware
   (juce::String name, juce::int64 seed); `patch` reuses SynthPatch UNMODIFIED
   (design.md Decision 5) - only 11 of its 19 fields are ever written or read
   by PresetManager's serialization, so the 8 effects fields on a loaded
   Preset::patch always carry kDefaultPatch's values via default member init.

   scaleType/rootPitchClass/rangeLow/rangeHigh (scale-aware-generation
   design.md, schema v2): four EXPLICIT scalar fields, not a nested
   GenerationParams member - loadPreset applies them individually so a
   whole-struct GenerationParams assignment never clobbers the deliberately
   unpersisted mode/pulses/rotation/stepProbability. Defaults (minor/C/36-72)
   are what a pre-change (schema v1) preset file loads as.

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>

#include "core/Scale.h"
#include "synth/SynthPatch.h"

namespace berlin
{

struct Preset
{
    juce::String name;
    SynthPatch   patch;      // ONLY the 11 live fields are serialized; the 8 effects
                              // fields are never written or read -> always kDefaultPatch
    juce::int64  seed = 0;

    // Persisted since schema v2 (scale-aware-generation) - see the header
    // comment above for why these are explicit fields, not a GenerationParams member.
    ScaleType scaleType      = ScaleType::minor;
    int       rootPitchClass = 0;
    int       rangeLow       = 36;
    int       rangeHigh      = 72;
};

// PresetManager::fromValueTree's outcome, and MainComponent::describePresetFailure's
// input. `ok` is the only value on which `out` in fromValueTree/load is modified.
enum class PresetResult
{
    ok,
    nameInvalid,
    directoryUnavailable,
    writeFailed,
    fileNotFound,
    parseFailed,
    unsupportedVersion
};

} // namespace berlin
