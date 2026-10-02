/*
  ==============================================================================

   UiBridge - a single validated command surface (ui-bridge spec, design.md
   D9-D13) that lets a UI (present or future, e.g. a WebView) drive the
   engine without touching BerlinAudioProcessor internals directly. Exactly
   15 commands, MESSAGE THREAD ONLY.

   Headless-safe: only forward-declares BerlinAudioProcessor, so this header
   is reachable from the juce_core-only test target the same way
   BerlinAudioProcessor.h itself is. Depends on juce_core only (juce::var,
   juce::String).

   dispatch(command, args) parses and validates EVERY argument into locals
   before calling any BerlinAudioProcessor mutator (design.md's Technical
   Approach) - a rejected call is guaranteed to leave state unchanged. Result
   shape: {ok:true, error:"", snapshot} on success, {ok:false, error:<token>}
   (no snapshot key) on failure - see the ui-bridge spec's "Uniform Result
   Shape" / "Fixed Error Tokens" requirements for the exact token set.

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>

namespace berlin
{

class BerlinAudioProcessor;

class UiBridge
{
public:
    explicit UiBridge (BerlinAudioProcessor& processorToControl) noexcept;

    // MESSAGE THREAD ONLY. See the file header above and the ui-bridge spec
    // for the full command/argument/error contract.
    juce::var dispatch (const juce::String& command, const juce::var& args);

    // MESSAGE THREAD ONLY. Fresh read of the full engine state - see the
    // ui-bridge spec's "Snapshot Contents" requirement for the exact key set.
    juce::var snapshot() const;

private:
    BerlinAudioProcessor& processor;
};

} // namespace berlin
