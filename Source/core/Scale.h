/*
  ==============================================================================

   Scale - a root note plus an ordered list of semitone intervals
   (sequencing-core spec).

   JUCE-free: standard library only. The interval-table constructor is
   private; the only sanctioned scales are Scale::major() and Scale::minor()
   (design.md's rationale: enforces the settled "minor/major ONLY" scope at
   the type level - no caller can smuggle an unsanctioned scale via a public
   ctor). Definitions live in Scale.cpp so a forgotten <FILE> registration in
   either .jucer project fails loudly as an unresolved-external link error,
   per design.md's ".h/.cpp for types with out-of-line definitions" decision.

  ==============================================================================
*/

#pragma once

#include <vector>

namespace berlin
{

class Scale
{
public:
    static Scale major (int rootNote);   // {0,2,4,5,7,9,11}
    static Scale minor (int rootNote);   // natural minor {0,2,3,5,7,8,10}

    int  getRoot() const noexcept;
    int  getNumDegrees() const noexcept;
    int  getDegree (int degreeIndex) const;    // precondition: 0 <= degreeIndex < getNumDegrees()
    bool contains (int note) const noexcept;   // octave-agnostic (floored modulo, correct below root)

private:
    Scale (int rootNote, std::vector<int> semitoneIntervals);

    int root { 0 };
    std::vector<int> intervals;
};

} // namespace berlin
