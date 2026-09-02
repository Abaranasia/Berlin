/*
  ==============================================================================

   MidiMessageBytes - JUCE-free constexpr MIDI byte packing
   (midi-output-routing spec, midi-output-dispatch Requirement 1).

   Header-only, standard library only. Aggregate (exactly three public
   members, no user-declared constructors/destructor/base classes) so
   static_assert(std::is_aggregate_v<MidiMessageBytes>) is meaningful;
   operator== is therefore a non-member so it does not disqualify the type
   from being an aggregate.

   ON THE REAL PATH, NOT A PARALLEL IMPLEMENTATION (design.md Decision 1):
   MidiEventTranslator and MidiOutputSink build juce::MidiMessage
   (status, data1, data2) FROM these bytes, so these unit tests cover real
   shipped behaviour instead of dead code. Being constexpr, most assertions
   here are compile-time static_asserts.

   makeNoteOff produces a REAL 0x80 status with release velocity 0 - NOT
   note-on with velocity 0. The manual-gate monitor must be able to
   distinguish them.

  ==============================================================================
*/

#pragma once

#include <type_traits>

namespace berlin
{

struct MidiMessageBytes
{
    unsigned char status = 0, data1 = 0, data2 = 0;
};

static_assert (std::is_aggregate_v<MidiMessageBytes>);

constexpr bool operator== (const MidiMessageBytes& lhs, const MidiMessageBytes& rhs) noexcept
{
    return lhs.status == rhs.status && lhs.data1 == rhs.data1 && lhs.data2 == rhs.data2;
}

inline constexpr int kAllNotesOffController = 123;

constexpr int clampMidiChannel (int channel) noexcept
{
    return channel < 1 ? 1 : (channel > 16 ? 16 : channel);
}

constexpr int clampMidiData (int value) noexcept
{
    return value < 0 ? 0 : (value > 127 ? 127 : value);
}

constexpr MidiMessageBytes makeNoteOn (int channel, int note, int velocity) noexcept
{
    return { static_cast<unsigned char> (0x90 | (clampMidiChannel (channel) - 1)),
             static_cast<unsigned char> (clampMidiData (note)),
             static_cast<unsigned char> (clampMidiData (velocity)) };
}

constexpr MidiMessageBytes makeNoteOff (int channel, int note) noexcept
{
    // A real note-off status (0x80), release velocity 0 - distinguishable from
    // a note-on with velocity 0, which some devices/monitors treat identically
    // to a note-off but which is a different wire message.
    return { static_cast<unsigned char> (0x80 | (clampMidiChannel (channel) - 1)),
             static_cast<unsigned char> (clampMidiData (note)),
             0 };
}

constexpr MidiMessageBytes makeAllNotesOff (int channel) noexcept
{
    return { static_cast<unsigned char> (0xB0 | (clampMidiChannel (channel) - 1)),
             static_cast<unsigned char> (kAllNotesOffController),
             0 };
}

} // namespace berlin
