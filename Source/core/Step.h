/*
  ==============================================================================

   Step - a single sequencer step value type (sequencing-core spec).

   JUCE-free: standard library only. Kept as an aggregate (exactly four public
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

inline constexpr int kDefaultVelocity = 100;
inline constexpr int kMinVelocity     = 1;     // 0 is a note-off on the MIDI wire
inline constexpr int kMaxVelocity     = 127;

constexpr bool isValidVelocity (int velocity) noexcept
{
    return velocity >= kMinVelocity && velocity <= kMaxVelocity;
}

struct Step
{
    int  note   = 0;      // MIDI note number
    bool active = false;  // gate flag; false is a REST (the step still takes time)
    bool skipped = false; // true: the step is not in the playback order at all (takes no time)
    int  velocity = kDefaultVelocity;   // note-on velocity 1-127; 100 is the neutral level (step-velocity)
};

static_assert (std::is_aggregate_v<Step>);

inline bool operator== (const Step& lhs, const Step& rhs)
{
    return lhs.note == rhs.note && lhs.active == rhs.active && lhs.skipped == rhs.skipped
        && lhs.velocity == rhs.velocity;
}

} // namespace berlin
