/*
  ==============================================================================

   StandaloneHost - the seam between WebEditor and the standalone shell
   (ui-settings design D1). MainComponent implements it; the plugin passes
   nullptr, so the settings native functions are never registered there.

   Both calls are synchronous and MESSAGE-THREAD ONLY. JUCE-free (juce::var
   only), so WebEditor stays unaware of devices and MIDI ports.

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>

namespace berlin
{

class StandaloneHost
{
public:
    virtual ~StandaloneHost() = default;

    // `query` is void or {deviceType, device}: the device whose rates and buffer
    // sizes to list (a device that is selected in the dialog but not applied yet).
    virtual juce::var getAudioSettings (const juce::var& query) = 0;

    // Returns {ok:true} or {ok:false,error:token}; transactional.
    virtual juce::var applyAudioSettings (const juce::var& request) = 0;
};

} // namespace berlin
