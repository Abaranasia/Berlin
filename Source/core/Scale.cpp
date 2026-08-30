/*
  ==============================================================================

   Scale - out-of-line definitions (sequencing-core spec).

  ==============================================================================
*/

#include "Scale.h"

#include <utility>

namespace berlin
{

Scale::Scale (int rootNote, std::vector<int> semitoneIntervals)
    : root (rootNote), intervals (std::move (semitoneIntervals))
{
}

Scale Scale::major (int rootNote)
{
    return Scale (rootNote, { 0, 2, 4, 5, 7, 9, 11 });
}

Scale Scale::minor (int rootNote)
{
    return Scale (rootNote, { 0, 2, 3, 5, 7, 8, 10 });
}

int Scale::getRoot() const noexcept
{
    return root;
}

int Scale::getNumDegrees() const noexcept
{
    return static_cast<int> (intervals.size());
}

int Scale::getDegree (int degreeIndex) const
{
    return root + intervals[static_cast<std::size_t> (degreeIndex)];
}

bool Scale::contains (int note) const noexcept
{
    // Floored modulo, NOT C++'s truncating `%` - this is what makes
    // containment correct for notes below the root (design.md GOTCHA).
    // C++'s `%` can return a negative result when `note < root`, and a
    // negative remainder never equals a (non-negative) interval entry.
    const int semitoneFromRoot = ((note - root) % 12 + 12) % 12;

    for (const auto interval : intervals)
        if (interval == semitoneFromRoot)
            return true;

    return false;
}

} // namespace berlin
