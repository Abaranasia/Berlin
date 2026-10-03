/*
  ==============================================================================

   WebEditor - opt-in WebView2 host for the PoC React UI (webview-poc,
   web-editor spec). Compiled only when BERLIN_WEB_UI is defined. Drives the
   engine solely through UiBridge (message thread) and never touches the audio
   thread. No automated test: gui code, covered by build + manual checks; the
   logic it relies on lives in WebAssets / WebEditorSupport (unit-tested).

  ==============================================================================
*/

#pragma once

#if BERLIN_WEB_UI

#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include "bridge/UiBridge.h"
#include "plugin/BerlinAudioProcessor.h"
#include "ui/WebEditorSupport.h"

namespace berlin
{

class WebEditor final : public juce::AudioProcessorEditor,
                        private juce::Timer
{
public:
    explicit WebEditor (BerlinAudioProcessor& processorToEdit);
    ~WebEditor() override;

    void resized() override;

private:
    void timerCallback() override;

    // Member order matters: the browser is destroyed first (its native
    // functions capture `bridge`).
    BerlinAudioProcessor&                      owner;
    UiBridge                                   bridge;
    PlayheadChangeDetector                     detector;
    juce::Label                                fallbackLabel;
    std::unique_ptr<juce::WebBrowserComponent> browser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WebEditor)
};

} // namespace berlin

#endif // BERLIN_WEB_UI
