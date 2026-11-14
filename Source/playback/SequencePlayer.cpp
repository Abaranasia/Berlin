/*
  ==============================================================================

   SequencePlayer - out-of-line definitions (playback-transport-clock spec,
   step-event-scheduling).

  ==============================================================================
*/

#include "SequencePlayer.h"

#include "core/StepCursor.h"

namespace berlin
{

SequencePlayer::SequencePlayer (Sequence sequenceToPlay, Transport transportToUse)
    : sequence (std::move (sequenceToPlay)), transport (std::move (transportToUse))
{
    pendingBpm.store (transport.getBpm(), std::memory_order_relaxed);
    playRequested.store (transport.isRunning(), std::memory_order_relaxed);   // D2
}

void SequencePlayer::prepare (double sampleRate) noexcept
{
    transport.prepare (sampleRate);   // internally calls Transport::reset(), restarting the step counter at 0

    // V3 fix: without this, a restart mid-loop (e.g. audio device restart)
    // left playhead stale and non-zero while the step counter restarted at
    // 0, so the next step-0 boundary would read as a false wrap.
    playhead.store (0, std::memory_order_relaxed);
    cursor = -1;   // nothing played yet: the first boundary can never be a wrap
}

void SequencePlayer::start() noexcept
{
    transport.start();
    playRequested.store (true, std::memory_order_relaxed);   // D2: keeps the request in sync with a direct call
}

void SequencePlayer::stop() noexcept
{
    transport.stop();
    playRequested.store (false, std::memory_order_relaxed);   // D2: keeps the request in sync with a direct call
}

void SequencePlayer::setPlaying (bool shouldPlay) noexcept
{
    playRequested.store (shouldPlay, std::memory_order_relaxed);
}

bool SequencePlayer::isPlayRequested() const noexcept
{
    return playRequested.load (std::memory_order_relaxed);   // D3: requested state, not the adopted one
}

void SequencePlayer::reset() noexcept
{
    transport.reset();
    pendingNote = -1;
    pendingStep = 0;
    playhead.store (0, std::memory_order_relaxed);
    cursor = -1;
}

bool SequencePlayer::publishSequence (Sequence& incoming, PublishMode mode) noexcept
{
    if (sequencePending.load (std::memory_order_acquire))
        return false;   // previous publish still unadopted: nothing published, incoming unmodified

    pendingSequence.swap (incoming);   // O(1): no alloc, no free, no lock
    pendingMode = mode;                // ordered before the release store below
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
        if (pendingMode == PublishMode::keepPosition)
        {
            // Step edit: no transport/playhead reset, loopCount untouched. Evaluate
            // the sounding step against the ADOPTED content (design D2).
            sequence.swap (pendingSequence);   // O(1) pointer swap: no alloc, no free, no lock

            // The cursor continues. Out of range, or a skipped cursor whose
            // continuation would wrap, restarts at the first playable step so
            // that re-positioning never counts as a loop (step-model D4).
            if (cursor >= sequence.size()
                || (cursor >= 0 && sequence[cursor].skipped && nextPlayableStep (sequence, cursor).wrapped))
                cursor = -1;

            if (pendingNote >= 0
                && (pendingStep >= sequence.size()
                    || sequence[pendingStep].skipped
                    || ! sequence[pendingStep].active
                    || sequence[pendingStep].note != pendingNote))
            {
                out.push ({ 0, pendingStep, pendingNote, false });   // offset 0, FIRST
                pendingNote = -1;
            }
        }
        else
        {
            if (pendingNote >= 0)
                out.push ({ 0, pendingStep, pendingNote, false });   // swap-edge note-off, offset 0, FIRST

            sequence.swap (pendingSequence);   // O(1) pointer swap: no alloc, no free, no lock
            pendingNote = -1;
            pendingStep = 0;
            transport.reset();                 // restart from step 0 (preserves running state)
            playhead.store (0, std::memory_order_relaxed);
            cursor = -1;
        }

        sequencePending.store (false, std::memory_order_release);
    }

    transport.setBpm (pendingBpm.load (std::memory_order_relaxed));   // design.md Decision 4: after adopt, before countBoundaries

    // ui-engine-api design.md D1: adopt the play/stop request ONLY ON A
    // TRANSITION. A direct start()/stop() call already matches playRequested
    // to transport's new running state (D2), so neither branch below fires
    // for it - this block only reacts to setPlaying() having been called
    // without a matching direct start()/stop().
    const bool wantsToPlay = playRequested.load (std::memory_order_relaxed);

    if (! wantsToPlay && transport.isRunning())
    {
        transport.stop();

        if (pendingNote >= 0)
            out.push ({ 0, pendingStep, pendingNote, false });   // adopted-stop note-off, offset 0

        pendingNote = -1;
    }
    else if (wantsToPlay && ! transport.isRunning())
    {
        transport.start();   // D4: resumes - position/nextStepCounter/origin are untouched by stop()/start()
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

            // Skipped steps take no boundary. All-skipped (index < 0): no event,
            // no loop, and the playhead holds. The first boundary after cursor -1
            // (prepare/reset/restart-adopt) is never a wrap.
            const StepAdvance adv = nextPlayableStep (sequence, cursor);

            if (adv.index < 0)
                continue;

            const Step& step = sequence[adv.index];

            if (step.active)
            {
                out.push ({ b.sampleOffset, adv.index, step.note, true, step.velocity });
                pendingNote = step.note;
                pendingStep = adv.index;
            }

            if (adv.wrapped)
                loopCount.store (loopCount.load (std::memory_order_relaxed) + 1, std::memory_order_relaxed);

            cursor = adv.index;
            playhead.store (cursor, std::memory_order_relaxed);
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
