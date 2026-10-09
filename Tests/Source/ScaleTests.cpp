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

        beginTest ("fromPitchClass(minor, 0) is degree-for-degree equal to Scale::minor(48)");
        {
            // scale-aware-generation design.md: kPitchClassAnchor == 48, so the
            // default construction is LITERALLY Scale::minor(48) - this is the
            // byte-identical-output regression anchor.
            const berlin::Scale fromCatalog = berlin::Scale::fromPitchClass (berlin::ScaleType::minor, 0);
            const berlin::Scale reference   = berlin::Scale::minor (48);

            expectEquals (fromCatalog.getRoot(), reference.getRoot());
            expectEquals (fromCatalog.getNumDegrees(), reference.getNumDegrees());

            for (int i = 0; i < reference.getNumDegrees(); ++i)
                expectEquals (fromCatalog.getDegree (i), reference.getDegree (i));
        }

        beginTest ("fromPitchClass interval sets match the documented catalog for all 6 ScaleTypes");
        {
            using berlin::ScaleType;

            auto checkIntervals = [this] (ScaleType type, const std::vector<int>& expectedIntervals)
            {
                const berlin::Scale scale = berlin::Scale::fromPitchClass (type, 0);
                expectEquals (scale.getNumDegrees(), (int) expectedIntervals.size());

                for (int i = 0; i < scale.getNumDegrees(); ++i)
                    expectEquals (scale.getDegree (i) - scale.getRoot(), expectedIntervals[(std::size_t) i]);
            };

            checkIntervals (ScaleType::minor,         { 0, 2, 3, 5, 7, 8, 10 });
            checkIntervals (ScaleType::major,          { 0, 2, 4, 5, 7, 9, 11 });
            checkIntervals (ScaleType::dorian,         { 0, 2, 3, 5, 7, 9, 10 });
            checkIntervals (ScaleType::phrygian,       { 0, 1, 3, 5, 7, 8, 10 });
            checkIntervals (ScaleType::mixolydian,     { 0, 2, 4, 5, 7, 9, 10 });
            checkIntervals (ScaleType::harmonicMinor,  { 0, 2, 3, 5, 7, 8, 11 });
        }

        beginTest ("fromPitchClass anchors the root at kPitchClassAnchor + pitchClass, floored-mod 12");
        {
            const berlin::Scale scale = berlin::Scale::fromPitchClass (berlin::ScaleType::major, 2); // D
            expectEquals (scale.getRoot(), berlin::kPitchClassAnchor + 2);

            // Floored-mod: pitchClass 14 (== 2 mod 12) must anchor identically to pitchClass 2.
            const berlin::Scale wrapped = berlin::Scale::fromPitchClass (berlin::ScaleType::major, 14);
            expectEquals (wrapped.getRoot(), berlin::kPitchClassAnchor + 2);

            // Floored-mod: a negative pitchClass wraps forward, never truncates toward 0.
            const berlin::Scale negative = berlin::Scale::fromPitchClass (berlin::ScaleType::major, -1);
            expectEquals (negative.getRoot(), berlin::kPitchClassAnchor + 11);
        }
    }
};

static ScaleTests scaleTests;
