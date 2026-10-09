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

Scale Scale::fromPitchClass (ScaleType type, int pitchClass)
{
    // Floored modulo, not truncating `%` - a negative pitchClass must wrap
    // forward (e.g. -1 -> 11), consistent with Scale::contains's own GOTCHA.
    const int wrapped = ((pitchClass % 12) + 12) % 12;
    const int root = kPitchClassAnchor + wrapped;

    switch (type)
    {
        case ScaleType::minor:         return Scale (root, { 0, 2, 3, 5, 7, 8, 10 });
        case ScaleType::major:         return Scale (root, { 0, 2, 4, 5, 7, 9, 11 });
        case ScaleType::dorian:        return Scale (root, { 0, 2, 3, 5, 7, 9, 10 });
        case ScaleType::phrygian:      return Scale (root, { 0, 1, 3, 5, 7, 8, 10 });
        case ScaleType::mixolydian:    return Scale (root, { 0, 2, 4, 5, 7, 9, 10 });
        case ScaleType::harmonicMinor: return Scale (root, { 0, 2, 3, 5, 7, 8, 11 });
    }

    return Scale (root, { 0, 2, 3, 5, 7, 8, 10 });   // unreachable: every enumerator handled above
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
