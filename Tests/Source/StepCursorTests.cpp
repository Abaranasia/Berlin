/*
  ==============================================================================

   StepCursor tests (step-model). nextPlayableStep / countPlayableSteps are the
   single definition of "which step plays next" shared by SequencePlayer and
   MidiExportTimeline. RED first: core/StepCursor.h does not exist yet.

  ==============================================================================
*/

#include <juce_core/juce_core.h>

#include "core/StepCursor.h"

class StepCursorTests final : public juce::UnitTest
{
public:
    StepCursorTests() : juce::UnitTest ("StepCursor", "Berlin") {}

    void runTest() override
    {
        beginTest ("from -1 the first non-skipped step is returned, not a wrap");
        {
            berlin::Sequence seq (4);
            seq[0].skipped = true;

            const auto adv = berlin::nextPlayableStep (seq, -1);

            expectEquals (adv.index, 1);
            expect (! adv.wrapped);
        }

        beginTest ("advancing walks forward and wraps past the last step");
        {
            berlin::Sequence seq (3);

            expectEquals (berlin::nextPlayableStep (seq, 0).index, 1);
            expect (! berlin::nextPlayableStep (seq, 0).wrapped);

            const auto adv = berlin::nextPlayableStep (seq, 2);
            expectEquals (adv.index, 0);
            expect (adv.wrapped);
        }

        beginTest ("a skipped step 0 wraps to the first playable step");
        {
            berlin::Sequence seq (4);
            seq[0].skipped = true;

            const auto adv = berlin::nextPlayableStep (seq, 3);

            expectEquals (adv.index, 1);
            expect (adv.wrapped);
        }

        beginTest ("skipped steps in the middle are jumped over without a wrap");
        {
            berlin::Sequence seq (5);
            seq[1].skipped = true;
            seq[2].skipped = true;

            const auto adv = berlin::nextPlayableStep (seq, 0);

            expectEquals (adv.index, 3);
            expect (! adv.wrapped);
        }

        beginTest ("a single playable step wraps on every call");
        {
            berlin::Sequence seq (4);
            seq[0].skipped = true;
            seq[1].skipped = true;
            seq[3].skipped = true;

            const auto adv = berlin::nextPlayableStep (seq, 2);

            expectEquals (adv.index, 2);
            expect (adv.wrapped);
            expectEquals (berlin::nextPlayableStep (seq, -1).index, 2);
            expect (! berlin::nextPlayableStep (seq, -1).wrapped);
        }

        beginTest ("a length-1 sequence wraps on every call after the first");
        {
            const berlin::Sequence seq (1);

            expect (! berlin::nextPlayableStep (seq, -1).wrapped);
            expect (berlin::nextPlayableStep (seq, 0).wrapped);
            expectEquals (berlin::nextPlayableStep (seq, 0).index, 0);
        }

        beginTest ("an all-skipped or empty sequence returns -1");
        {
            berlin::Sequence seq (3);
            for (int i = 0; i < seq.size(); ++i)
                seq[i].skipped = true;

            expectEquals (berlin::nextPlayableStep (seq, -1).index, -1);
            expectEquals (berlin::nextPlayableStep (seq, 1).index, -1);
            expectEquals (berlin::nextPlayableStep (berlin::Sequence {}, -1).index, -1);
        }

        beginTest ("countPlayableSteps counts non-skipped steps, inactive included");
        {
            berlin::Sequence seq (5);
            seq[0].active = true;
            seq[2].skipped = true;
            seq[4].skipped = true;

            expectEquals (berlin::countPlayableSteps (seq), 3);
            expectEquals (berlin::countPlayableSteps (berlin::Sequence {}), 0);
        }

        beginTest ("both helpers are noexcept (audio-thread contract)");
        {
            const berlin::Sequence seq (2);

            static_assert (noexcept (berlin::nextPlayableStep (seq, -1)));
            static_assert (noexcept (berlin::countPlayableSteps (seq)));
        }
    }
};

static StepCursorTests stepCursorTests;
