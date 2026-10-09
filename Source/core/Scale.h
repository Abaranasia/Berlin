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

// ScaleType - the named scale catalog (scale-aware-generation design.md D1).
// Ordinals are UI/setSelectedId inputs only; persistence uses name strings
// (PresetManager::scaleNames()), so ordinals may be reordered only with the
// same care already applied to RhythmMode.
enum class ScaleType { minor, major, dorian, phrygian, mixolydian, harmonicMinor };

// Anchors Scale::fromPitchClass's root at MIDI note 48 (C3) + pitchClass, so
// that fromPitchClass(minor, 0) is LITERALLY Scale::minor(48) - the
// byte-identical-output regression anchor (scale-aware-generation design.md).
inline constexpr int kPitchClassAnchor = 48;

class Scale
{
public:
    static Scale major (int rootNote);   // {0,2,4,5,7,9,11}
    static Scale minor (int rootNote);   // natural minor {0,2,3,5,7,8,10}

    // pitchClass is floored-mod 12 (0 = C, ... 11 = B); root anchors at
    // kPitchClassAnchor + wrapped pitchClass (scale-aware-generation design.md).
    static Scale fromPitchClass (ScaleType type, int pitchClass);

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
