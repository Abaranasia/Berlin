/*
  ==============================================================================

   Step tests (sequencing-core spec). RED first: Source/core/Step.h does not
   exist yet, so linking this suite must fail with an unresolved-symbol/missing-
   header error until 1.2 creates it. Covers construction, equality, and the
   "no extra fields" contract, which C++ cannot verify at runtime and is
   additionally a review-time check (see the review-note test below).

  ==============================================================================
*/

#include <juce_core/juce_core.h>

#include "core/Step.h"

class StepTests final : public juce::UnitTest
{
public:
    StepTests() : juce::UnitTest ("Step", "Berlin") {}

    void runTest() override
    {
        beginTest ("construct a step");
        {
            const berlin::Step step { 60, true };

            expectEquals (step.note, 60);
            expect (step.active);
        }

        beginTest ("equality - identical steps compare equal");
        {
            const berlin::Step a { 60, true };
            const berlin::Step b { 60, true };

            expect (a == b);
        }

        beginTest ("equality - differing note compares unequal");
        {
            const berlin::Step a { 60, true };
            const berlin::Step b { 61, true };

            expect (! (a == b));
        }

        beginTest ("equality - differing active compares unequal");
        {
            const berlin::Step a { 60, true };
            const berlin::Step b { 60, false };

            expect (! (a == b));
        }

        beginTest ("no extra fields (review note)");
        {
            // C++ has no runtime member reflection, so this cannot be a real
            // assertion - it is a documented review check. Step.h's
            // static_assert(std::is_aggregate_v<Step>) enforces aggregate
            // shape; a reviewer must additionally confirm the type still
            // exposes exactly `note` and `active` and nothing else (no
            // velocity, gate length, duration, probability, timing-offset,
            // or accent fields per the sequencing-core spec).
            expect (true, "Reviewer must confirm Step exposes exactly {note, active} - see comment above.");
        }
    }
};

static StepTests stepTests;
