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
    pendingBpm.store (transport.getBpm(), std::memory_order_relaxed);
}

void SequencePlayer::prepare (double sampleRate) noexcept
{
    transport.prepare (sampleRate);   // internally calls Transport::reset(), restarting the step counter at 0

    // V3 fix: without this, a restart mid-loop (e.g. audio device restart)
    // left playhead stale and non-zero while the step counter restarted at
    // 0, so the next step-0 boundary would read as a false wrap.
    playhead.store (0, std::memory_order_relaxed);
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

void SequencePlayer::setBpm (double newBpm) noexcept
{
    pendingBpm.store (newBpm, std::memory_order_relaxed);
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

    transport.setBpm (pendingBpm.load (std::memory_order_relaxed));   // design.md Decision 4: after adopt, before countBoundaries

    if (sequence.size() > 0)
    {
        const int n = transport.countBoundaries (numSamples);

        // Hoisted AFTER the adopt block above: on a post-adopt call this reads
        // 0 (adopt already zeroed playhead before this loop runs), which
        // correctly excludes the post-adopt step-0 boundary from counting as
        // a genuine wrap (design.md Decision 1 / V2).
        int previousStep = playhead.load (std::memory_order_relaxed);

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

            if (stepIndex == 0 && previousStep != 0)   // genuine wrap; post-adopt reads 0 -> excluded
                loopCount.store (loopCount.load (std::memory_order_relaxed) + 1, std::memory_order_relaxed);

            playhead.store (stepIndex, std::memory_order_relaxed);
            previousStep = stepIndex;
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

int SequencePlayer::getLoopCount() const noexcept
{
    return loopCount.load (std::memory_order_relaxed);
}

bool SequencePlayer::isPublishPending() const noexcept
{
    return sequencePending.load (std::memory_order_acquire);
}

} // namespace berlin
