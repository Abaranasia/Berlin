/*
  ==============================================================================

   MidiChannelRemap - rewrites the channel of every channel-voice message in a
   juce::MidiBuffer, IN PLACE (ui-settings design D6, midi-device-selection
   "Standalone Output Channel").

   Header-only, juce_audio_basics only. The processor, the plugin's MIDI output
   and MIDI export stay on channel 1; only MidiOutputSink remaps, right before
   the bytes go to the device.

   Real-time safe: no allocation, no lock, no exception. A MidiBuffer stores its
   events in its own byte array, and iteration hands out a `const uint8*` into
   that storage. Writing the status byte through it needs a const_cast, which is
   well-defined here because the underlying storage is NOT const (we hold a
   non-const MidiBuffer&). A second buffer would allocate on the audio thread.
   SysEx, realtime messages and sample positions are untouched.

  ==============================================================================
*/

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "midi/MidiChannel.h"
#include "midi/MidiMessageBytes.h"

namespace berlin
{

inline void remapMidiChannel (juce::MidiBuffer& buffer, int channel) noexcept
{
    if (clampMidiChannel (channel) == kMidiChannel)
        return;   // events are authored on channel 1: nothing to rewrite

    for (const auto metadata : buffer)
    {
        if (metadata.numBytes <= 0)
            continue;

        auto* status = const_cast<juce::uint8*> (metadata.data);   // see the header comment: storage is non-const
        *status = remapChannelStatus (*status, channel);
    }
}

} // namespace berlin
