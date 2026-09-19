/*
  ==============================================================================

   BerlinPluginMain - the plugin entry point JUCE's juce_audio_plugin_client
   module requires (roadmap Phase 11 / vst3-au-plugin, design.md D7). Compiled
   ONLY into the plugin target (BerlinPlugin.jucer's `audioplug` project) -
   never into the standalone `guiapp` (Berlin.jucer, which has its own
   Source/Main.cpp) or the headless test target.

  ==============================================================================
*/

#include "BerlinAudioProcessor.h"

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new berlin::BerlinAudioProcessor();
}
