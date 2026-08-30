/*
  ==============================================================================

   SequencePlayer - out-of-line definitions (playback-transport-clock spec,
   step-event-scheduling).

  ==============================================================================
*/

#include "SequencePlayer.h"

namespace berlin
{

SequencePlayer::SequencePlayer (Sequence sequenceToPlay, Transport transportToUse)
    : sequence (std::move (sequenceToPlay)), transport (std::move (transportToUse))
{
}

void SequencePlayer::prepare (double sampleRate) noexcept
{
    transport.prepare (sampleRate);
}

void SequencePlayer::start() noexcept
{
    transport.start();
}

void SequencePlayer::stop() noexcept
{
    transport.stop();
}

void SequencePlayer::reset() noexcept
{
    transport.reset();
    pendingNote = -1;
    pendingStep = 0;
    playhead.store (0, std::memory_order_relaxed);
}

void SequencePlayer::process (int numSamples, StepEventBuffer& out) noexcept
{
    out.clear();

    if (sequence.size() > 0)
    {
        const int n = transport.countBoundaries (numSamples);

        for (int i = 0; i < n; ++i)
        {
            const StepBoundary b = transport.getBoundary (i);

            if (pendingNote >= 0)   // note-off FIRST, same offset
                out.push ({ b.sampleOffset, pendingStep, pendingNote, false });
            pendingNote = -1;

            const int stepIndex = static_cast<int> (b.stepCounter % sequence.size());   // loop wrap
            const Step& step = sequence[stepIndex];

            if (step.active)
            {
                out.push ({ b.sampleOffset, stepIndex, step.note, true });
                pendingNote = step.note;
                pendingStep = stepIndex;
            }

            playhead.store (stepIndex, std::memory_order_relaxed);
        }
    }

    transport.advance (numSamples);
}

int SequencePlayer::getPlayheadStep() const noexcept
{
    return playhead.load (std::memory_order_relaxed);
}

} // namespace berlin
