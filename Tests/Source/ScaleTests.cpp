/*
  ==============================================================================

   Scale tests (sequencing-core spec). RED first: Source/core/Scale.h/.cpp
   do not exist yet, so this suite fails to compile until 3.2 creates them.
   Covers the major/minor factories, degree retrieval, and octave-agnostic
   containment - including notes BELOW the root, which requires floored-
   modulo arithmetic rather than C++'s truncating `%` (design.md GOTCHA).

  ==============================================================================
*/

#include <type_traits>
#include <vector>

#include <juce_core/juce_core.h>

#include "core/Scale.h"

// Compile-time proof of the private-constructor / named-factory contract:
// Scale's interval-table constructor is private, so it must not be
// accessible from outside the class - only Scale::major()/Scale::minor()
// may construct a Scale.
static_assert (! std::is_constructible_v<berlin::Scale, int, std::vector<int>>,
               "Scale's interval-table constructor must be private - only "
               "Scale::major()/Scale::minor() may construct a Scale.");

class ScaleTests final : public juce::UnitTest
{
public:
    ScaleTests() : juce::UnitTest ("Scale", "Berlin") {}

    void runTest() override
    {
        beginTest ("major scale factory - degrees match standard major pattern");
        {
            const berlin::Scale scale = berlin::Scale::major (60);

            expectEquals (scale.getRoot(), 60);
            expectEquals (scale.getNumDegrees(), 7);

            const int expected[] = { 60, 62, 64, 65, 67, 69, 71 };

            for (int i = 0; i < scale.getNumDegrees(); ++i)
                expectEquals (scale.getDegree (i), expected[i]);
        }

        beginTest ("minor scale factory - degrees match standard natural-minor pattern");
        {
            const berlin::Scale scale = berlin::Scale::minor (60);

            expectEquals (scale.getRoot(), 60);
            expectEquals (scale.getNumDegrees(), 7);

            const int expected[] = { 60, 62, 63, 65, 67, 68, 70 };

            for (int i = 0; i < scale.getNumDegrees(); ++i)
                expectEquals (scale.getDegree (i), expected[i]);
        }

        beginTest ("contains - note in scale returns true");
        {
            const berlin::Scale scale = berlin::Scale::major (60);

            expect (scale.contains (64)); // E - major 3rd above root
        }

        beginTest ("contains - note not in scale returns false");
        {
            const berlin::Scale scale = berlin::Scale::major (60);

            expect (! scale.contains (61)); // C# - not in C major
        }

        beginTest ("contains - octave-agnostic above the root");
        {
            const berlin::Scale scale = berlin::Scale::major (60);

            expect (scale.contains (64 + 12)); // E, one octave up
            expect (scale.contains (64 + 24)); // E, two octaves up
        }

        beginTest ("contains - notes BELOW the root use floored modulo, not truncating %");
        {
            const berlin::Scale scale = berlin::Scale::major (60);

            // C++'s truncating `%` gives (64 - 12 - 60) % 12 == (-8) % 12 == -8,
            // which never equals any positive interval - a naive
            // implementation would wrongly report these as not-in-scale.
            expect (scale.contains (64 - 12)); // E, one octave below root
            expect (scale.contains (64 - 24)); // E, two octaves below root
            expect (scale.contains (60 - 12)); // root itself, one octave below

            expect (! scale.contains (61 - 12)); // C#, one octave below root - still not in scale
        }
    }
};

static ScaleTests scaleTests;
