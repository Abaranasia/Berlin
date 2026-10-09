/*
  ==============================================================================

   AutoEvolveSchedule - out-of-line definitions (roadmap Phase 11 /
   vst3-au-plugin, design.md Decision 6 / D6).

  ==============================================================================
*/

#include "AutoEvolveSchedule.h"

namespace berlin
{

void AutoEvolveSchedule::reset (int currentLoopCount) noexcept
{
    lastMutationLoopCount = currentLoopCount;
    dueAtLoopCount = -1;
}

void AutoEvolveSchedule::checkDue (int loopCount, int rate) noexcept
{
    if (dueAtLoopCount < 0 && loopCount - lastMutationLoopCount >= rate)
        dueAtLoopCount = loopCount;
}

void AutoEvolveSchedule::markMutationSucceeded() noexcept
{
    lastMutationLoopCount = dueAtLoopCount;
    dueAtLoopCount = -1;
}

} // namespace berlin
