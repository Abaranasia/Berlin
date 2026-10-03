/*
  ==============================================================================

   WebEditorSupport - gui-free logic used by WebEditor (webview-poc design
   D6/D8): native-function argument adapter, playhead change detection and the
   debug dev-server origin check. Links UiBridge, juce_core only.

  ==============================================================================
*/

#pragma once

#include <optional>

#include <juce_core/juce_core.h>

#include "bridge/UiBridge.h"

namespace berlin
{

// args[0] = command (string), args[1] = optional object. Never crashes; bad
// input yields {ok:false, error:<token>}. MESSAGE THREAD ONLY (UiBridge).
juce::var dispatchNativeCall (UiBridge& bridge, const juce::Array<juce::var>& args);

struct PlayheadState
{
    int step;
    bool playing;
};

// True until the first commit, then true whenever the state differs from the
// last COMMITTED one (so an unsent state stays "changed").
class PlayheadChangeDetector
{
public:
    bool hasChanged (PlayheadState state) const noexcept;
    void commit (PlayheadState state) noexcept;

private:
    bool hasCommitted = false;
    PlayheadState last { 0, false };
};

// Accepts only http://localhost:<port> or http://127.0.0.1:<port>.
std::optional<juce::String> devServerOrigin (const juce::String& env);

} // namespace berlin
