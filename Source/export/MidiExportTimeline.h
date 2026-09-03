/*
  ==============================================================================

   MidiExportTimeline - JUCE-free, tick-domain transform (midi-export-timeline
   spec, roadmap Phase 6 / midi-export).

   A Sequence, a stepsPerBeat, and a repeat count become an ordered,
   tick-stamped note-on/note-off event timeline - the sole source of musical
   correctness for the exported .mid file, mirroring SequencePlayer::process's
   note-off-before-note-on convention exactly (design.md Decision 7). This is
   a deliberate tick-domain duplicate of that algorithm; a future gate-length
   change must update both.

   JUCE-free: <vector> + core/Sequence.h only, so Tests/BerlinTests.jucer's
   juce_core-only harness can reach it completely. Definitions live in
   MidiExportTimeline.cpp so a forgotten <FILE> registration in either .jucer
   project fails loudly as an unresolved-external link error, per the
   project's ".h/.cpp for types with out-of-line definitions" convention.

   Takes NO bpm (design.md Decision 5): SMF ticks are musical, not temporal -
   tempo is metadata applied only by MidiFileWriter.

  ==============================================================================
*/

#pragma once

#include <type_traits>
#include <vector>

#include "core/Sequence.h"

namespace berlin
{

inline constexpr int kTicksPerQuarterNote = 960;   // Decision 6 - divisible by every plausible stepsPerBeat

struct MidiExportEvent
{
    long long tick   = 0;       // absolute ticks from the start of the file
    int       note   = 0;       // MIDI note number
    bool      noteOn = false;   // false => note-off
};
static_assert (std::is_aggregate_v<MidiExportEvent>);
bool operator== (const MidiExportEvent& lhs, const MidiExportEvent& rhs);   // non-member: keeps it an aggregate

struct MidiExportTimeline
{
    std::vector<MidiExportEvent> events;                  // final order; NEVER re-sorted
    long long                    endTick             = 0; // repeats * size * ticksPerStep
    int                          ticksPerQuarterNote = 0; // 0 until built
    int                          ticksPerStep        = 0; // 0 until built
};

enum class MidiExportStatus { ok, invalidRepeatCount, invalidStepsPerBeat };

// Message thread, allocating. `out` is cleared first and left EMPTY on any
// non-ok status (never partially filled). An empty or all-inactive `sequence`
// returns `ok` with no events.
MidiExportStatus buildMidiExportTimeline (const Sequence& sequence,
                                          int stepsPerBeat,
                                          int repeats,
                                          MidiExportTimeline& out);

} // namespace berlin
