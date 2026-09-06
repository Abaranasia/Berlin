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

bool SequencePlayer::publishSequence (Sequence& incoming) noexcept
{
    if (sequencePending.load (std::memory_order_acquire))
        return false;   // previous publish still unadopted: nothing published, incoming unmodified

    pendingSequence.swap (incoming);   // O(1): no alloc, no free, no lock
    sequencePending.store (true, std::memory_order_release);
    return true;
}

void SequencePlayer::process (int numSamples, StepEventBuffer& out) noexcept
{
    out.clear();

    if (sequencePending.load (std::memory_order_acquire))
    {
        if (pendingNote >= 0)
            out.push ({ 0, pendingStep, pendingNote, false });   // swap-edge note-off, offset 0, FIRST

        sequence.swap (pendingSequence);   // O(1) pointer swap: no alloc, no free, no lock
        pendingNote = -1;
        pendingStep = 0;
        transport.reset();                 // restart from step 0 (preserves running state)
        playhead.store (0, std::memory_order_relaxed);
        sequencePending.store (false, std::memory_order_release);
    }

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

bool SequencePlayer::flushPendingNoteOff (StepEventBuffer& out) noexcept
{
    out.clear();

    if (pendingNote < 0)
        return false;

    out.push ({ 0, pendingStep, pendingNote, false });
    pendingNote = -1;
    return true;
}

int SequencePlayer::getPlayheadStep() const noexcept
{
    return playhead.load (std::memory_order_relaxed);
}

} // namespace berlin
