/*
  ==============================================================================

   StepCursor - the single definition of "which step plays next" (step-model).

   JUCE-free, header-only, allocation-free. SequencePlayer (audio thread) and
   MidiExportTimeline both walk a Sequence through nextPlayableStep(), so the
   exported file cannot drift from what is heard. A skipped step is not in the
   playback order at all (it takes no boundary); an inactive step is a rest
   and still takes one.

  ==============================================================================
*/

#pragma once

#include "Sequence.h"

namespace berlin
{

struct StepAdvance
{
    int  index;     // -1: the sequence is empty or every step is skipped
    bool wrapped;   // crossed from the last playable step to the first
};

// The first non-skipped step after `from`, scanning forward modulo size() and
// probing at most size() steps. from == -1 means "nothing played yet" and can
// never wrap. Precondition: -1 <= from < seq.size().
inline StepAdvance nextPlayableStep (const Sequence& seq, int from) noexcept
{
    const int size = seq.size();

    for (int probe = 1; probe <= size; ++probe)
    {
        const int index = (from + probe) % size;

        if (! seq[index].skipped)
            return { index, from >= 0 && index <= from };
    }

    return { -1, false };
}

inline int countPlayableSteps (const Sequence& seq) noexcept
{
    int count = 0;

    for (int i = 0; i < seq.size(); ++i)
        if (! seq[i].skipped)
            ++count;

    return count;
}

} // namespace berlin
