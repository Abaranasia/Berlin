/*
  ==============================================================================

   Step - a single sequencer step value type (sequencing-core spec).

   JUCE-free: standard library only. Kept as an aggregate (exactly three public
   members, no user-declared constructors/destructor/base classes) so
   static_assert(std::is_aggregate_v<Step>) below is meaningful; operator==
   is therefore a non-member so it does not disqualify the type from being an
   aggregate.

  ==============================================================================
*/

#pragma once

#include <type_traits>

namespace berlin
{

struct Step
{
    int  note   = 0;      // MIDI note number
    bool active = false;  // gate flag; false is a REST (the step still takes time)
    bool skipped = false; // true: the step is not in the playback order at all (takes no time)
};

static_assert (std::is_aggregate_v<Step>);

inline bool operator== (const Step& lhs, const Step& rhs)
{
    return lhs.note == rhs.note && lhs.active == rhs.active && lhs.skipped == rhs.skipped;
}

} // namespace berlin
