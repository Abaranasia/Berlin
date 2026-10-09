/*
  ==============================================================================

   StepEvent - a single scheduled playback event value type
   (playback-transport-clock spec, step-event-scheduling).

   JUCE-free: standard library only. Kept as an aggregate (exactly four public
   members, no user-declared constructors/destructor/base classes) so
   static_assert(std::is_aggregate_v<StepEvent>) below is meaningful; operator==
   is therefore a non-member so it does not disqualify the type from being an
   aggregate. sampleOffset is relative to the START of the current audio
   block, NOT to bufferToFill.startSample - callers must add startSample
   themselves.

  ==============================================================================
*/

#pragma once

#include <type_traits>

namespace berlin
{

struct StepEvent
{
    int  sampleOffset = 0;      // samples from the START of the current block
    int  stepIndex    = 0;      // sequence step that OWNS the event (note-off carries
    int  note         = 0;      // the originating step, not the one it lands on)
    bool isNoteOn     = false;
};

static_assert (std::is_aggregate_v<StepEvent>);

inline bool operator== (const StepEvent& lhs, const StepEvent& rhs)
{
    return lhs.sampleOffset == rhs.sampleOffset
        && lhs.stepIndex == rhs.stepIndex
        && lhs.note == rhs.note
        && lhs.isNoteOn == rhs.isNoteOn;
}

} // namespace berlin
