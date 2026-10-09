/*
  ==============================================================================

   MidiEventTranslator - StepEvent -> juce::MidiBuffer translation on a fixed
   channel and velocity (midi-output-routing spec, midi-output-dispatch
   Requirement 2 "Non-Allocating StepEvent-to-MidiBuffer Translation").

   JUCE-aware (juce_audio_basics): no automated test can reach this file
   (Tests/BerlinTests.jucer stays juce_core-only by design) - manual-gate-only,
   same accepted tradeoff as Phase 4's realtime-audio-wiring coverage gap.

   AUDIO THREAD, RT-safe: translate() clears `destination` (juce::MidiBuffer's
   own clear(), no reallocation) then pushes one 3-byte juce::MidiMessage per
   StepEvent, built FROM MidiMessageBytes per design.md Decision 1 - never
   juce::MidiMessage::noteOn/noteOff directly, so the unit-tested packing
   helper covers real shipped behaviour. Timestamp = StepEvent::sampleOffset
   UNMODIFIED - bufferToFill.startSample is NOT added here (that warning
   applies to indexing the audio buffer, not to MIDI timestamps; adding it
   here is the most likely apply-time bug per design.md). Never calls
   ensureSize inside translate() - the buffer is pre-reserved once in
   MainComponent::prepareToPlay.

   Definitions live in MidiEventTranslator.cpp so a forgotten <FILE>
   registration in Berlin.jucer fails loudly as an unresolved-external link
   error, per design.md's ".h/.cpp for types with out-of-line definitions"
   decision (same convention as SequencePlayer/Transport).

  ==============================================================================
*/

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "playback/StepEventBuffer.h"

namespace berlin
{

class MidiEventTranslator
{
public:
    static constexpr int kNoteVelocity = 100;   // Step has no velocity field until Phase 6

    explicit MidiEventTranslator (int outputChannel) noexcept;

    // AUDIO THREAD, RT-safe. destination.clear() then one 3-byte message per
    // StepEvent at timestamp = StepEvent::sampleOffset. Never calls
    // ensureSize / never grows the buffer.
    void translate (const StepEventBuffer& events, juce::MidiBuffer& destination) const noexcept;

    int getChannel() const noexcept;

private:
    const int channel;
};

} // namespace berlin
