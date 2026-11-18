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

static_assert (berlin::kDefaultVelocity == 100 && berlin::kMinVelocity == 1 && berlin::kMaxVelocity == 127);
static_assert (! berlin::isValidVelocity (0) && berlin::isValidVelocity (1)
               && berlin::isValidVelocity (127) && ! berlin::isValidVelocity (128));

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

        beginTest ("skipped defaults to false and a two-field init compiles");
        {
            const berlin::Step step { 60, true };

            expect (! step.skipped);
            expect (! berlin::Step {}.skipped);
        }

        beginTest ("equality - differing skipped compares unequal");
        {
            const berlin::Step a { 60, true, false };
            const berlin::Step b { 60, true, true };

            expect (! (a == b));
            expect (b == berlin::Step { 60, true, true });
        }

        beginTest ("velocity defaults to 100 and an existing three-field init compiles");
        {
            const berlin::Step step { 60, true, false };

            expectEquals (step.velocity, 100);
            expectEquals (berlin::Step {}.velocity, berlin::kDefaultVelocity);
            expectEquals (berlin::Step { 60, true, false, 40 }.velocity, 40);
        }

        beginTest ("equality - differing velocity compares unequal");
        {
            const berlin::Step a { 60, true, false, 40 };
            const berlin::Step b { 60, true, false, 127 };

            expect (! (a == b));
            expect (a == berlin::Step { 60, true, false, 40 });
        }

        beginTest ("no extra fields (review note)");
        {
            // C++ has no runtime member reflection, so this cannot be a real
            // assertion - it is a documented review check. Step.h's
            // static_assert(std::is_aggregate_v<Step>) enforces aggregate
            // shape; a reviewer must additionally confirm the type still
            // exposes exactly `note`, `active`, `skipped` and `velocity` and nothing
            // else (no gate length, duration, probability, or timing-offset
            // fields).
            expect (true, "Reviewer must confirm Step exposes exactly {note, active, skipped, velocity} - see comment above.");
        }
    }
};

static StepTests stepTests;
