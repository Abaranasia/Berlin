/*
  ==============================================================================

   AutoEvolveSchedule tests (roadmap Phase 11 / vst3-au-plugin, design.md
   Decision 6 / D6). RED first: Source/generation/AutoEvolveSchedule.h does
   not exist yet, so this suite must fail to compile until the GREEN step
   creates it.

   Covers the re-baselining arithmetic MainComponent::timerCallback used to
   own directly (auto-evolution spec, design.md Decision 7): not due until the
   loop-count threshold is crossed; due is latched at the loop count AT WHICH
   the crossing happened, not re-evaluated on later ticks (busy-retry keeps
   the same dueAtLoopCount across repeated checkDue calls); a successful
   mutation re-baselines lastMutationLoopCount to the REMEMBERED due value,
   not to whatever loop count is current at that moment; reset()/stop()
   semantics for enabling/disabling auto-evolve.

  ==============================================================================
*/

#include <juce_core/juce_core.h>

#include "generation/AutoEvolveSchedule.h"

class AutoEvolveScheduleTests final : public juce::UnitTest
{
public:
    AutoEvolveScheduleTests() : juce::UnitTest ("AutoEvolveSchedule", "Berlin") {}

    void runTest() override
    {
        beginTest ("Not due before the threshold is crossed");
        {
            berlin::AutoEvolveSchedule schedule;
            schedule.reset (0);

            schedule.checkDue (3, 4);   // 3 - 0 < 4
            expect (! schedule.isDue());
        }

        beginTest ("Becomes due exactly when loopCount - lastMutationLoopCount >= rate");
        {
            berlin::AutoEvolveSchedule schedule;
            schedule.reset (0);

            schedule.checkDue (4, 4);   // 4 - 0 >= 4
            expect (schedule.isDue());
            expectEquals (schedule.getDueAtLoopCount(), 4);
        }

        beginTest ("Due is latched at the crossing loop count, not overwritten by later ticks (busy-retry)");
        {
            berlin::AutoEvolveSchedule schedule;
            schedule.reset (0);

            schedule.checkDue (4, 4);   // becomes due at loop 4
            expectEquals (schedule.getDueAtLoopCount(), 4);

            schedule.checkDue (10, 4);   // still due from before - must NOT move to 10
            expect (schedule.isDue());
            expectEquals (schedule.getDueAtLoopCount(), 4);
        }

        beginTest ("markMutationSucceeded re-baselines to the REMEMBERED due value, not the current loop count");
        {
            berlin::AutoEvolveSchedule schedule;
            schedule.reset (0);

            schedule.checkDue (4, 4);          // due at loop 4
            schedule.checkDue (10, 4);         // busy-retry tick, still due at loop 4
            schedule.markMutationSucceeded();  // succeeds on the retry, at loop 10 "current time"

            expect (! schedule.isDue());
            expectEquals (schedule.getLastMutationLoopCount(), 4);   // NOT 10
            expectEquals (schedule.getDueAtLoopCount(), -1);
        }

        beginTest ("After re-baselining, the next threshold is measured from the new baseline");
        {
            berlin::AutoEvolveSchedule schedule;
            schedule.reset (0);

            schedule.checkDue (4, 4);
            schedule.markMutationSucceeded();   // lastMutationLoopCount == 4

            schedule.checkDue (7, 4);   // 7 - 4 == 3 < 4: not due yet
            expect (! schedule.isDue());

            schedule.checkDue (8, 4);   // 8 - 4 == 4 >= 4: due
            expect (schedule.isDue());
            expectEquals (schedule.getDueAtLoopCount(), 8);
        }

        beginTest ("reset() re-baselines to the given loop count and clears any pending due state");
        {
            berlin::AutoEvolveSchedule schedule;
            schedule.reset (0);
            schedule.checkDue (4, 4);
            expect (schedule.isDue());

            schedule.reset (100);   // e.g. auto-evolve toggled off then on again
            expect (! schedule.isDue());
            expectEquals (schedule.getLastMutationLoopCount(), 100);
        }
    }
};

static AutoEvolveScheduleTests autoEvolveScheduleTests;
