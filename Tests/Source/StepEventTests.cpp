/*
  ==============================================================================

   StepEvent tests (playback-transport-clock spec, Phase 1). RED first:
   Source/playback/StepEvent.h does not exist yet, so this suite must fail to
   compile until 1.2 creates it. Covers construction/field read-back, the
   non-member operator==, and the aggregate-shape contract via a compile-time
   static_assert, matching the pattern established for Step.h in
   core-sequencing-model Phase 1.

  ==============================================================================
*/

#include <juce_core/juce_core.h>

#include <type_traits>

#include "playback/StepEvent.h"

static_assert (std::is_aggregate_v<berlin::StepEvent>);

class StepEventTests final : public juce::UnitTest
{
public:
    StepEventTests() : juce::UnitTest ("StepEvent", "Berlin") {}

    void runTest() override
    {
        beginTest ("construct a StepEvent and read back each field");
        {
            const berlin::StepEvent event { 128, 3, 60, true };

            expectEquals (event.sampleOffset, 128);
            expectEquals (event.stepIndex, 3);
            expectEquals (event.note, 60);
            expect (event.isNoteOn);
        }

        beginTest ("default-constructed StepEvent has documented zero/false defaults");
        {
            const berlin::StepEvent event {};

            expectEquals (event.sampleOffset, 0);
            expectEquals (event.stepIndex, 0);
            expectEquals (event.note, 0);
            expect (! event.isNoteOn);
        }

        beginTest ("equality - identical StepEvents compare equal");
        {
            const berlin::StepEvent a { 128, 3, 60, true };
            const berlin::StepEvent b { 128, 3, 60, true };

            expect (a == b);
        }

        beginTest ("equality - differing sampleOffset compares unequal");
        {
            const berlin::StepEvent a { 128, 3, 60, true };
            const berlin::StepEvent b { 256, 3, 60, true };

            expect (! (a == b));
        }

        beginTest ("equality - differing stepIndex compares unequal");
        {
            const berlin::StepEvent a { 128, 3, 60, true };
            const berlin::StepEvent b { 128, 4, 60, true };

            expect (! (a == b));
        }

        beginTest ("equality - differing note compares unequal");
        {
            const berlin::StepEvent a { 128, 3, 60, true };
            const berlin::StepEvent b { 128, 3, 61, true };

            expect (! (a == b));
        }

        beginTest ("equality - differing isNoteOn compares unequal");
        {
            const berlin::StepEvent a { 128, 3, 60, true };
            const berlin::StepEvent b { 128, 3, 60, false };

            expect (! (a == b));
        }
    }
};

static StepEventTests stepEventTests;
