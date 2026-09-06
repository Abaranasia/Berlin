/*
  ==============================================================================

   Preset - the in-memory unit a named preset save/load operates on
   (preset-persistence spec, roadmap Phase 11 / preset-system). JUCE-aware
   (juce::String name, juce::int64 seed); `patch` reuses SynthPatch UNMODIFIED
   (design.md Decision 5) - only 11 of its 19 fields are ever written or read
   by PresetManager's serialization, so the 8 effects fields on a loaded
   Preset::patch always carry kDefaultPatch's values via default member init.

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>

#include "synth/SynthPatch.h"

namespace berlin
{

struct Preset
{
    juce::String name;
    SynthPatch   patch;      // ONLY the 11 live fields are serialized; the 8 effects
                              // fields are never written or read -> always kDefaultPatch
    juce::int64  seed = 0;
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
