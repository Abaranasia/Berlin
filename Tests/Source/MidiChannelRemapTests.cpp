/*
  ==============================================================================

   MidiChannelRemap tests (ui-settings, midi-device-selection "Standalone Output
   Channel", design D6): remapMidiChannel rewrites channel-voice status bytes of
   a real juce::MidiBuffer in place, leaving SysEx and realtime messages and the
   sample positions untouched.

  ==============================================================================
*/

#include <juce_audio_basics/juce_audio_basics.h>

#include "midi/MidiChannelRemap.h"
#include "midi/MidiMessageBytes.h"

class MidiChannelRemapTests final : public juce::UnitTest
{
public:
    MidiChannelRemapTests() : juce::UnitTest ("MidiChannelRemap", "Berlin") {}

    void runTest() override
    {
        beginTest ("a note-on authored on channel 1 is rewritten to the chosen channel (5 and 16)");
        {
            for (const int channel : { 5, 16 })
            {
                juce::MidiBuffer buffer;
                buffer.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 12);
                buffer.addEvent (juce::MidiMessage::noteOff (1, 60), 34);

                berlin::remapMidiChannel (buffer, channel);

                std::vector<juce::MidiMessage> out;
                std::vector<int> positions;
                for (const auto meta : buffer)
                {
                    out.push_back (meta.getMessage());
                    positions.push_back (meta.samplePosition);
                }

                expectEquals (static_cast<int> (out.size()), 2);
                expect (out[0].isNoteOn());
                expectEquals (out[0].getChannel(), channel);
                expectEquals (out[0].getNoteNumber(), 60);
                expectEquals (static_cast<int> (out[0].getVelocity()), 100);
                expect (out[1].isNoteOff());
                expectEquals (out[1].getChannel(), channel);
                expectEquals (positions[0], 12);
                expectEquals (positions[1], 34);
            }
        }

        beginTest ("channel 1 is a no-op");
        {
            juce::MidiBuffer buffer;
            buffer.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 90), 0);

            berlin::remapMidiChannel (buffer, 1);

            const auto meta = *buffer.begin();
            expectEquals (meta.getMessage().getChannel(), 1);
            expectEquals (meta.getMessage().getNoteNumber(), 64);
        }

        beginTest ("control change and pitch wheel are remapped, SysEx and realtime pass through");
        {
            juce::MidiBuffer buffer;
            buffer.addEvent (juce::MidiMessage::controllerEvent (1, 123, 0), 0);
            buffer.addEvent (juce::MidiMessage::pitchWheel (1, 8192), 1);
            const juce::uint8 sysex[] = { 0xF0, 0x7D, 0x01, 0x02, 0xF7 };
            buffer.addEvent (sysex, (int) sizeof (sysex), 2);
            buffer.addEvent (juce::MidiMessage::midiClock(), 3);

            berlin::remapMidiChannel (buffer, 9);

            std::vector<juce::MidiMessage> out;
            for (const auto meta : buffer)
                out.push_back (meta.getMessage());

            expectEquals (static_cast<int> (out.size()), 4);
            expect (out[0].isController());
            expectEquals (out[0].getChannel(), 9);
            expectEquals (out[0].getControllerNumber(), berlin::kAllNotesOffController);
            expect (out[1].isPitchWheel());
            expectEquals (out[1].getChannel(), 9);
            expect (out[2].isSysEx());
            expectEquals (out[2].getRawDataSize(), 5);
            expectEquals (static_cast<int> (out[2].getRawData()[1]), 0x7D);
            expect (out[3].isMidiClock());
        }

        beginTest ("the rewrite happens in the buffer's own storage (no reallocation)");
        {
            juce::MidiBuffer buffer;
            buffer.ensureSize (256);
            buffer.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            const auto* before = (*buffer.begin()).data;

            berlin::remapMidiChannel (buffer, 6);

            expect ((*buffer.begin()).data == before);
            expectEquals ((*buffer.begin()).getMessage().getChannel(), 6);
        }

        beginTest ("an empty buffer is left empty");
        {
            juce::MidiBuffer buffer;
            berlin::remapMidiChannel (buffer, 4);
            expect (buffer.isEmpty());
        }
    }
};

static MidiChannelRemapTests midiChannelRemapTests;
