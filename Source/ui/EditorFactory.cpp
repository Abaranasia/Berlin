#include "ui/EditorFactory.h"

#include "plugin/BerlinAudioProcessor.h"
#include "ui/WebEditor.h"

namespace berlin
{

std::unique_ptr<juce::AudioProcessorEditor> createBerlinEditor (BerlinAudioProcessor& processor, StandaloneHost* host)
{
    return std::make_unique<WebEditor> (processor, host);
}

} // namespace berlin
