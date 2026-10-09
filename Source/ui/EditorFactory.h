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
class StandaloneHost;

// WebEditor when BERLIN_WEB_UI is defined, otherwise the legacy editor.
// `host` is the standalone shell (ui-settings D1); nullptr in the plugin. The
// WebEditor registers the settings native functions only when it is non-null;
// the legacy editor ignores it.
std::unique_ptr<juce::AudioProcessorEditor> createBerlinEditor (BerlinAudioProcessor& processor,
                                                                StandaloneHost* host = nullptr);

} // namespace berlin
