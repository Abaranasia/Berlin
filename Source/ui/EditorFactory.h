/*
  ==============================================================================

   EditorFactory - the single compile-time switch between the legacy editor and
   the opt-in WebEditor (webview-poc design D6). JUCE-aware (juce_gui_basics):
   never linked into the headless BerlinTests target.

  ==============================================================================
*/

#pragma once

#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>

namespace berlin
{

class BerlinAudioProcessor;

// WebEditor when BERLIN_WEB_UI is defined, otherwise the legacy editor.
std::unique_ptr<juce::AudioProcessorEditor> createBerlinEditor (BerlinAudioProcessor& processor);

} // namespace berlin
