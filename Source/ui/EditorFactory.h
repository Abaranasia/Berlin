/*
  ==============================================================================

   EditorFactory - creates the editor for BerlinAudioProcessor: always the
   WebEditor. Kept as a thin wrapper so BerlinAudioProcessor.cpp stays free of
   juce_gui_extra. JUCE-aware (juce_gui_basics): never linked into the headless
   BerlinTests target.

  ==============================================================================
*/

#pragma once

#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>

namespace berlin
{

class BerlinAudioProcessor;
class StandaloneHost;

// Always a WebEditor. `host` is the standalone shell (ui-settings D1); nullptr in
// the plugin. The WebEditor registers the settings native functions only when it
// is non-null.
std::unique_ptr<juce::AudioProcessorEditor> createBerlinEditor (BerlinAudioProcessor& processor,
                                                                StandaloneHost* host = nullptr);

} // namespace berlin
