/*
  ==============================================================================

   AutoEvolveSchedule - the auto-evolve re-baselining state machine (roadmap
   Phase 11 / vst3-au-plugin, design.md Decision 6 / D6). Moved verbatim from
   MainComponent's lastMutationLoopCount/dueAtLoopCount fields plus the
   arithmetic in MainComponent::timerCallback (auto-evolution spec, design.md
   Decision 7).

   JUCE-free: standard library only (plain ints). dueAtLoopCount (-1 = nothing
   due) remembers the loop-count value AT WHICH a threshold crossing became
   due; on success the baseline advances to that REMEMBERED value, not to
   whatever the loop count reads at delivery time - this keeps the cadence
   anchored to the triggering boundary even across busy-retry delays (a busy
   publish, or a mutation rejection, leaves the schedule due and
   checkDue()/markMutationSucceeded() are simply called again on a later
   tick).

   This class knows nothing about SequencePlayer, mutate(), or
   isPublishPending() - the caller (BerlinAudioProcessor::timerCallback) is
   responsible for calling checkDue() every tick, then, only when isDue() is
   true AND the caller has independently confirmed no publish is pending,
   attempting the mutation and calling markMutationSucceeded() iff it
   succeeded. A rejected/skipped attempt is simply left due for the next tick
   by doing nothing.

   Definitions live in AutoEvolveSchedule.cpp so a forgotten <FILE>
   registration in either .jucer project fails loudly as an unresolved-
   external link error, per the project's ".h/.cpp for types with out-of-line
   definitions" convention.

  ==============================================================================
*/

#pragma once

namespace berlin
{

class AutoEvolveSchedule
{
public:
    // Re-baselines to `currentLoopCount` and clears any pending due state.
    // Called when auto-evolve is (re)enabled.
    void reset (int currentLoopCount) noexcept;

    // Call once per tick, unconditionally. Marks the schedule due exactly
    // once, at the loop count where the threshold was first crossed, and
    // leaves it due (busy-retry) until markMutationSucceeded() is called -
    // repeated calls while already due do NOT move dueAtLoopCount.
    void checkDue (int loopCount, int rate) noexcept;

    bool isDue() const noexcept { return dueAtLoopCount >= 0; }

    // Call ONLY after a mutation attempt that succeeded. Re-baselines to the
    // REMEMBERED due value, not the loop count current at delivery time.
    void markMutationSucceeded() noexcept;

    int getLastMutationLoopCount() const noexcept { return lastMutationLoopCount; }
    int getDueAtLoopCount() const noexcept { return dueAtLoopCount; }

private:
    int lastMutationLoopCount { 0 };
    int dueAtLoopCount { -1 };
};

} // namespace berlin
