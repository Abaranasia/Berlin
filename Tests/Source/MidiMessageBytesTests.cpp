/*
  ==============================================================================

   MidiMessageBytes tests (midi-output-routing spec, midi-output-dispatch
   Requirement 1 "JUCE-Free MIDI Byte Packing"). RED first:
   Source/midi/MidiMessageBytes.h does not exist yet, so this suite must fail
   to compile until 2.2 creates it.

   Table-driven per spec: correct status bytes for makeNoteOn (0x90|(ch-1))
   and makeNoteOff (0x80|(ch-1), release velocity 0 - a REAL note-off, not
   note-on with velocity 0); makeAllNotesOff (0xB0|(ch-1), 123, 0); channel 1
   and 16 endpoints; channel 0/17/-1 clamp via clampMidiChannel; note/velocity
   128 and -1 clamp via clampMidiData. Includes compile-time static_asserts:
   std::is_aggregate_v<MidiMessageBytes>; makeNoteOn (1, 60, 100) ==
   MidiMessageBytes { 0x90, 60, 100 } (both constexpr).

   JUCE-free, header-only in production (Source/midi/MidiMessageBytes.h) -
   this test file itself uses juce_core only for the UnitTest harness.

  ==============================================================================
*/

#include <juce_core/juce_core.h>

#include <type_traits>

#include "midi/MidiMessageBytes.h"

static_assert (std::is_aggregate_v<berlin::MidiMessageBytes>);
static_assert (berlin::makeNoteOn (1, 60, 100) == berlin::MidiMessageBytes { 0x90, 60, 100 });
static_assert (berlin::makeNoteOff (1, 60) == berlin::MidiMessageBytes { 0x80, 60, 0 });
static_assert (berlin::makeAllNotesOff (1) == berlin::MidiMessageBytes { 0xB0, 123, 0 });

class MidiMessageBytesTests final : public juce::UnitTest
{
public:
    MidiMessageBytesTests() : juce::UnitTest ("MidiMessageBytes", "Berlin") {}

    void runTest() override
    {
        beginTest ("makeNoteOn produces status 0x90|(channel-1), the given note and velocity");
        {
            expect (berlin::makeNoteOn (1, 60, 100) == berlin::MidiMessageBytes { 0x90, 60, 100 });
            expect (berlin::makeNoteOn (16, 127, 127) == berlin::MidiMessageBytes { 0x9F, 127, 127 });
        }

        beginTest ("makeNoteOff produces a REAL 0x80 status with release velocity 0, not note-on velocity 0");
        {
            const auto off = berlin::makeNoteOff (1, 60);
            expect (off == berlin::MidiMessageBytes { 0x80, 60, 0 });
            expect (off.status != berlin::makeNoteOn (1, 60, 0).status);   // distinguishable from note-on vel 0
        }

        beginTest ("makeAllNotesOff produces CC 123 (All Notes Off) with value 0 on the given channel");
        {
            expect (berlin::makeAllNotesOff (1) == berlin::MidiMessageBytes { 0xB0, 123, 0 });
            expect (berlin::makeAllNotesOff (16) == berlin::MidiMessageBytes { 0xBF, 123, 0 });
            expectEquals (berlin::kAllNotesOffController, 123);
        }

        beginTest ("channel 1 and 16 endpoints map to the correct low nibble");
        {
            expectEquals (berlin::makeNoteOn (1, 60, 100).status, static_cast<unsigned char> (0x90));
            expectEquals (berlin::makeNoteOn (16, 60, 100).status, static_cast<unsigned char> (0x9F));
        }

        beginTest ("clampMidiChannel clamps 0/17/-1 into [1, 16]");
        {
            expectEquals (berlin::clampMidiChannel (0), 1);
            expectEquals (berlin::clampMidiChannel (-1), 1);
            expectEquals (berlin::clampMidiChannel (17), 16);
            expectEquals (berlin::clampMidiChannel (1), 1);
            expectEquals (berlin::clampMidiChannel (16), 16);
        }

        beginTest ("clampMidiData clamps 128/-1 into [0, 127]");
        {
            expectEquals (berlin::clampMidiData (128), 127);
            expectEquals (berlin::clampMidiData (-1), 0);
            expectEquals (berlin::clampMidiData (0), 0);
            expectEquals (berlin::clampMidiData (127), 127);
        }

        beginTest ("makeNoteOn/makeNoteOff/makeAllNotesOff clamp out-of-range channel/note/velocity via the same helpers");
        {
            expect (berlin::makeNoteOn (0, 200, -5) == berlin::MidiMessageBytes { 0x90, 127, 0 });
            expect (berlin::makeNoteOff (17, -10) == berlin::MidiMessageBytes { 0x8F, 0, 0 });
            expect (berlin::makeAllNotesOff (0) == berlin::MidiMessageBytes { 0xB0, 123, 0 });
        }
    }
};

static MidiMessageBytesTests midiMessageBytesTests;
