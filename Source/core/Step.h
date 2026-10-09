/*
  ==============================================================================

   Step - a single sequencer step value type (sequencing-core spec).

   JUCE-free: standard library only. Kept as an aggregate (exactly two public
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
    bool active = false;  // gate flag
};

static_assert (std::is_aggregate_v<Step>);

inline bool operator== (const Step& lhs, const Step& rhs)
{
    return lhs.note == rhs.note && lhs.active == rhs.active;
}

} // namespace berlin
