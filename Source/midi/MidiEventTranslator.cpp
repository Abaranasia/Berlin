/*
  ==============================================================================

   MidiEventTranslator - out-of-line definitions (midi-output-routing spec,
   midi-output-dispatch).

  ==============================================================================
*/

#include "MidiEventTranslator.h"

#include "midi/MidiMessageBytes.h"

namespace berlin
{

MidiEventTranslator::MidiEventTranslator (int outputChannel) noexcept
    : channel (clampMidiChannel (outputChannel))
{
}

void MidiEventTranslator::translate (const StepEventBuffer& events, juce::MidiBuffer& destination) const noexcept
{
    destination.clear();

    for (int i = 0; i < events.size(); ++i)
    {
        const StepEvent& event = events[i];

        const MidiMessageBytes bytes = event.isNoteOn
            ? makeNoteOn (channel, event.note, kNoteVelocity)
            : makeNoteOff (channel, event.note);

        destination.addEvent (juce::MidiMessage (bytes.status, bytes.data1, bytes.data2),
                              event.sampleOffset);
    }
}

int MidiEventTranslator::getChannel() const noexcept
{
    return channel;
}

} // namespace berlin
