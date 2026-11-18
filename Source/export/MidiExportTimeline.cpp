/*
  ==============================================================================

   MidiExportTimeline - out-of-line definitions (midi-export-timeline spec).

  ==============================================================================
*/

#include "MidiExportTimeline.h"

#include "core/StepCursor.h"

namespace berlin
{

bool operator== (const MidiExportEvent& lhs, const MidiExportEvent& rhs)
{
    return lhs.tick == rhs.tick && lhs.note == rhs.note && lhs.noteOn == rhs.noteOn
        && lhs.velocity == rhs.velocity;
}

MidiExportStatus buildMidiExportTimeline (const Sequence& sequence,
                                          int stepsPerBeat,
                                          int repeats,
                                          MidiExportTimeline& out)
{
    out = MidiExportTimeline();   // cleared first; left empty on any non-ok status below

    if (repeats < 1)
        return MidiExportStatus::invalidRepeatCount;         // 0 and negatives REJECTED, never clamped

    if (stepsPerBeat < 1 || kTicksPerQuarterNote % stepsPerBeat != 0)
        return MidiExportStatus::invalidStepsPerBeat;         // never rounded

    const int       ticksPerStep = kTicksPerQuarterNote / stepsPerBeat;   // exact by the guard above
    const long long totalSteps   = static_cast<long long> (repeats) * countPlayableSteps (sequence);   // skipped steps take no time
    int             pendingNote  = -1;
    int             cursor       = -1;   // same walk as SequencePlayer (core/StepCursor.h)

    for (long long k = 0; k < totalSteps; ++k)
    {
        const long long tick = k * ticksPerStep;   // absolute, never accumulated -> no drift at repeat N

        if (pendingNote >= 0)   // note-off FIRST, SAME tick (Decision 7)
            out.events.push_back ({ tick, pendingNote, false });
        pendingNote = -1;

        cursor = nextPlayableStep (sequence, cursor).index;   // loop wrap; never -1 while totalSteps > 0
        const Step& step = sequence[cursor];
        if (step.active)
        {
            out.events.push_back ({ tick, step.note, true, step.velocity });
            pendingNote = step.note;
        }
    }

    out.endTick             = totalSteps * ticksPerStep;
    out.ticksPerQuarterNote = kTicksPerQuarterNote;
    out.ticksPerStep        = ticksPerStep;

    if (pendingNote >= 0)   // TERMINAL note-off - highest-flagged risk
        out.events.push_back ({ out.endTick, pendingNote, false });

    return MidiExportStatus::ok;
}

} // namespace berlin
