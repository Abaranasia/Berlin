#include "ui/EditorFactory.h"

#include "plugin/BerlinAudioProcessor.h"

#if BERLIN_WEB_UI
 #include "ui/WebEditor.h"
#else
 #include "plugin/BerlinAudioProcessorEditor.h"
#endif

namespace berlin
{

std::unique_ptr<juce::AudioProcessorEditor> createBerlinEditor (BerlinAudioProcessor& processor, StandaloneHost* host)
{
   #if BERLIN_WEB_UI
    return std::make_unique<WebEditor> (processor, host);
   #else
    juce::ignoreUnused (host);
    return std::make_unique<BerlinAudioProcessorEditor> (processor);
   #endif
}

} // namespace berlin
