/*
  ==============================================================================

   StepEventBuffer - a fixed-capacity, non-allocating sink for StepEvents
   (playback-transport-clock spec, step-event-scheduling).

   JUCE-free: standard library only. Backed by a std::array sized to the
   worst documented case (see design.md's capacity-64 decision) so push()
   never allocates on the audio thread. push() returns false and latches
   hasOverflowed() when full; already-stored events are left unchanged.
   clear() resets both size() to 0 and hasOverflowed() to false.

  ==============================================================================
*/

#pragma once

#include <array>

#include "playback/StepEvent.h"

namespace berlin
{

class StepEventBuffer
{
public:
    static constexpr int capacity = 64;

    void clear() noexcept
    {
        count = 0;
        overflowed = false;
    }

    bool push (const StepEvent& event) noexcept
    {
        if (count >= capacity)
        {
            overflowed = true;
            return false;
        }

        events[static_cast<size_t> (count)] = event;
        ++count;
        return true;
    }

    int size() const noexcept
    {
        return count;
    }

    bool hasOverflowed() const noexcept
    {
        return overflowed;
    }

    const StepEvent& operator[] (int index) const noexcept
    {
        return events[static_cast<size_t> (index)];
    }

private:
    std::array<StepEvent, capacity> events {};
    int  count { 0 };
    bool overflowed { false };
};

} // namespace berlin
