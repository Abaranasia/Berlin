/*
  ==============================================================================

   SequenceBuilder - builds a seeded, mode-dispatched Sequence (roadmap Phase
   11 / vst3-au-plugin, design.md Decision 6 / D6). Moved verbatim from
   MainComponent::buildSeededSequence (MainComponent.cpp:47) so it is
   reachable by BOTH BerlinAudioProcessor (message-thread ctor-init-list
   construction, same hard-ordering requirement as before) and the
   juce_core-only test target.

   Depends on juce_core (juce::int64) plus the existing generation/ types
   (DeterministicRandom, EuclideanRhythmGenerator, RhythmGenerator,
   SkipMaskGenerator, PitchGenerator) and core/Scale.h - all already reachable
   from the juce_core-only harness. MUST take GenerationParams by const
   reference (design.md Decision 2/V5 and probability-matrices Decision 7):
   this runs before any widget/processor member exists in the caller's
   ctor-init-list, so reading live state here would be undefined behaviour,
   not just bad style.

   normalizePitchRange (scale-aware-generation design.md): the single
   chokepoint that swaps an inverted [low, high] pair, clamps to
   [kMinPitch, kMaxPitch], then widens until high - low >= kMinPitchRangeSpan.
   Called inside buildSeededSequence (covers preset-loaded and
   test-constructed params, not just the UI) and again by the editor to
   constrain the range sliders interactively.

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>

#include "core/Sequence.h"
#include "generation/GenerationParams.h"

namespace berlin
{

void normalizePitchRange (int& low, int& high) noexcept;

Sequence buildSeededSequence (juce::int64 seed, const GenerationParams& params);

} // namespace berlin
