/*
  ==============================================================================

   SequencePlayer publish/adopt handoff tests (realtime-audio-wiring spec,
   generation-live-control spec). RED first: SequencePlayer::publishSequence
   does not exist yet, so this suite fails to compile until 3.3/3.4 add it.

   Covers design.md Decision 1's exact contract: publishSequence() returns
   true then false when a previous publish is still unadopted (and leaves
   `incoming` unmodified on failure); after one process() call, events come
   from the newly adopted sequence (adoption happens at the TOP of the same
   process() call, before that block's boundary loop runs); a sounding note
   yields a swap-edge note-off at offset 0 as the FIRST event, before any new
   note-on; nothing sounding produces no swap-edge note-off; the playhead and
   next note-on land on step 0 of the newly adopted sequence; adoption is
   one-shot (a second process() call with no new publish does not re-adopt
   or re-reset); a different-length sequence wraps on the NEW size, not the
   old one. All deterministic, no real audio device.

   Same convention as SequencePlayerTests.cpp / SequencePlayerStopTests.cpp:
   {bpm=60, stepsPerBeat=1} prepared at sampleRate=4.0 gives samplesPerStep
   == 4.0 exactly, so expected sample offsets are exact round numbers.

  ==============================================================================
*/

#include <juce_core/juce_core.h>

#include <atomic>
#include <vector>

#include "core/Sequence.h"
#include "core/Step.h"
#include "playback/SequencePlayer.h"
#include "playback/StepEventBuffer.h"
#include "playback/Transport.h"

namespace
{
    berlin::Sequence makeSequence (const std::vector<berlin::Step>& steps)
    {
        berlin::Sequence sequence (static_cast<int> (steps.size()));

        for (int i = 0; i < static_cast<int> (steps.size()); ++i)
            sequence[i] = steps[static_cast<size_t> (i)];

        return sequence;
    }
}

class SequencePlayerHandoffTests final : public juce::UnitTest
{
public:
    SequencePlayerHandoffTests() : juce::UnitTest ("SequencePlayerHandoff", "Berlin") {}

    void runTest() override
    {
        beginTest ("publishSequence()/process() are noexcept; atomic<bool> is always lock-free (compile-time RT-safety contract)");
        {
            berlin::Sequence sequence (1);
            berlin::SequencePlayer player (sequence, berlin::Transport (120.0, 4));
            berlin::Sequence incoming (1);
            berlin::StepEventBuffer buffer;

            static_assert (noexcept (player.publishSequence (incoming)));
            static_assert (noexcept (player.process (0, buffer)));
            static_assert (std::atomic<bool>::is_always_lock_free);
        }

        beginTest ("publishSequence() returns true then false when a previous publish is still unadopted; unadopted incoming is left unmodified");
        {
            auto sequenceA = makeSequence ({ { 60, true } });
            berlin::SequencePlayer player (sequenceA, berlin::Transport (60.0, 1));
            player.prepare (4.0);
            player.start();

            auto sequenceB = makeSequence ({ { 61, true } });
            expect (player.publishSequence (sequenceB));

            auto sequenceC = makeSequence ({ { 62, true } });
            expect (! player.publishSequence (sequenceC));

            // Unadopted publish must not modify `incoming`.
            expect (sequenceC[0] == berlin::Step { 62, true });
        }

        beginTest ("after one process() call, a sounding note yields the swap-edge note-off at offset 0 as the FIRST event, then the new sequence's step 0 note-on");
        {
            auto sequenceA = makeSequence ({ { 60, true } });
            berlin::SequencePlayer player (sequenceA, berlin::Transport (60.0, 1));
            player.prepare (4.0);
            player.start();

            berlin::StepEventBuffer buffer;
            player.process (4, buffer);   // boundary 0: step 0 note-on 60, now sounding
            expectEquals (buffer.size(), 1);
            expect (buffer[0] == berlin::StepEvent { 0, 0, 60, true });

            auto sequenceB = makeSequence ({ { 70, true }, { 71, true } });   // different length
            expect (player.publishSequence (sequenceB));

            player.process (4, buffer);   // adoption at TOP of this call: note-off 60 FIRST, then new step 0 note-on
            expectEquals (buffer.size(), 2);
            expect (buffer[0] == berlin::StepEvent { 0, 0, 60, false });
            expect (buffer[1] == berlin::StepEvent { 0, 0, 70, true });
            expectEquals (player.getPlayheadStep(), 0);
        }

        beginTest ("nothing sounding at the moment of adoption produces no swap-edge note-off");
        {
            auto sequenceA = makeSequence ({ { 60, false } });   // inactive: nothing ever sounds
            berlin::SequencePlayer player (sequenceA, berlin::Transport (60.0, 1));
            player.prepare (4.0);
            player.start();

            berlin::StepEventBuffer buffer;
            player.process (4, buffer);   // boundary 0: inactive, nothing pushed
            expectEquals (buffer.size(), 0);

            auto sequenceB = makeSequence ({ { 70, true } });
            expect (player.publishSequence (sequenceB));

            player.process (4, buffer);   // adoption: nothing was sounding -> no note-off; new step 0 note-on only
            expectEquals (buffer.size(), 1);
            expect (buffer[0] == berlin::StepEvent { 0, 0, 70, true });
            expectEquals (player.getPlayheadStep(), 0);
        }

        beginTest ("adoption is one-shot: a second process() call with no new publish does not re-adopt or re-reset the playhead");
        {
            auto sequenceA = makeSequence ({ { 60, true } });
            berlin::SequencePlayer player (sequenceA, berlin::Transport (60.0, 1));
            player.prepare (4.0);
            player.start();

            berlin::StepEventBuffer buffer;
            player.process (4, buffer);   // step 0 note-on 60

            auto sequenceB = makeSequence ({ { 70, true }, { 71, true } });
            expect (player.publishSequence (sequenceB));

            player.process (4, buffer);   // adoption: off 60, on 70 (step 0)
            expectEquals (buffer.size(), 2);
            expectEquals (player.getPlayheadStep(), 0);

            // No new publish before this call: must advance normally to step 1,
            // NOT re-adopt/re-reset back to step 0.
            player.process (4, buffer);
            expectEquals (buffer.size(), 2);
            expect (buffer[0] == berlin::StepEvent { 0, 0, 70, false });
            expect (buffer[1] == berlin::StepEvent { 0, 1, 71, true });
            expectEquals (player.getPlayheadStep(), 1);
        }

        beginTest ("a different-length sequence wraps on the NEW size, not the old one");
        {
            auto sequenceA = makeSequence ({ { 60, true } });   // old size 1
            berlin::SequencePlayer player (sequenceA, berlin::Transport (60.0, 1));
            player.prepare (4.0);
            player.start();

            berlin::StepEventBuffer buffer;
            player.process (4, buffer);   // step 0 note-on 60

            auto sequenceB = makeSequence ({ { 70, true }, { 71, true } });   // new size 2
            expect (player.publishSequence (sequenceB));

            player.process (4, buffer);   // adoption: off 60, on 70 (step 0)
            player.process (4, buffer);   // boundary 1 of new sequence: off 70, on 71 (step 1)

            // Boundary 2 must wrap to step 0 of the NEW size-2 sequence, not
            // continue past it as if the old size-1 sequence still applied.
            player.process (4, buffer);
            expectEquals (buffer.size(), 2);
            expect (buffer[0] == berlin::StepEvent { 0, 1, 71, false });
            expect (buffer[1] == berlin::StepEvent { 0, 0, 70, true });
            expectEquals (player.getPlayheadStep(), 0);
        }

        // ---- Loop-completion counter across the handoff, and isPublishPending()
        // observability (auto-evolution spec).

        beginTest ("publish/adopt does not increment loopCount; the counter never decreases across a handoff");
        {
            auto sequenceA = makeSequence ({ { 60, true }, { 62, true }, { 64, true }, { 67, true } });   // old size 4
            berlin::SequencePlayer player (sequenceA, berlin::Transport (60.0, 1));
            player.prepare (4.0);
            player.start();

            berlin::StepEventBuffer buffer;

            // Boundaries k=0..4: k=4 wraps the size-4 sequence back to step 0 -> +1.
            for (int i = 0; i < 5; ++i)
                player.process (4, buffer);
            expectEquals (player.getLoopCount(), 1);

            auto sequenceB = makeSequence ({ { 70, true }, { 71, true }, { 72, true } });   // new size 3
            expect (player.publishSequence (sequenceB));

            player.process (4, buffer);   // adoption at TOP of this call: playhead reset to 0, NOT a genuine wrap
            expectEquals (player.getLoopCount(), 1);   // unchanged by the adopt-triggered reset
            expectEquals (player.getPlayheadStep(), 0);

            // Continue on the NEW size-3 sequence: step 1, step 2, then a genuine
            // wrap back to step 0 -> exactly one more increment, never a decrease.
            player.process (4, buffer);   // step 1
            player.process (4, buffer);   // step 2
            player.process (4, buffer);   // wraps to step 0 of the new sequence -> +1
            expectEquals (player.getLoopCount(), 2);
        }

        beginTest ("isPublishPending() is false initially, true after publishSequence(), false after the adopting process()");
        {
            auto sequenceA = makeSequence ({ { 60, true } });
            berlin::SequencePlayer player (sequenceA, berlin::Transport (60.0, 1));
            player.prepare (4.0);
            player.start();

            expect (! player.isPublishPending());

            auto sequenceB = makeSequence ({ { 70, true } });
            expect (player.publishSequence (sequenceB));
            expect (player.isPublishPending());

            berlin::StepEventBuffer buffer;
            player.process (4, buffer);   // adoption happens at the TOP of this call
            expect (! player.isPublishPending());
        }

        // ---- Keep-position adopt (step-editing spec, design D1/D2).

        beginTest ("publishSequence() with PublishMode is noexcept; keepPosition keeps playhead, step counter and loopCount");
        {
            auto sequenceA = makeSequence ({ { 60, true }, { 62, true }, { 64, true }, { 67, true } });
            berlin::SequencePlayer player (sequenceA, berlin::Transport (60.0, 1));
            player.prepare (4.0);
            player.start();

            berlin::Sequence probe (1);
            static_assert (noexcept (player.publishSequence (probe, berlin::PublishMode::keepPosition)));

            berlin::StepEventBuffer buffer;
            for (int i = 0; i < 6; ++i)   // boundaries 0..5: playhead 1, one wrap
                player.process (4, buffer);

            expectEquals (player.getPlayheadStep(), 1);
            expectEquals (player.getLoopCount(), 1);

            auto edited = makeSequence ({ { 60, true }, { 62, true }, { 65, true }, { 67, true } });
            expect (player.publishSequence (edited, berlin::PublishMode::keepPosition));

            player.process (4, buffer);   // boundary 6 = step 2 of the adopted sequence, not step 0
            expectEquals (player.getPlayheadStep(), 2);
            expectEquals (player.getLoopCount(), 1);
            expectEquals (buffer.size(), 2);
            expect (buffer[0] == berlin::StepEvent { 0, 1, 62, false });   // normal boundary note-off, no extra event
            expect (buffer[1] == berlin::StepEvent { 0, 2, 65, true });
        }

        beginTest ("restart mode (explicit and default) still resets to step 0");
        {
            auto sequenceA = makeSequence ({ { 60, true }, { 62, true }, { 64, true } });
            berlin::SequencePlayer player (sequenceA, berlin::Transport (60.0, 1));
            player.prepare (4.0);
            player.start();

            berlin::StepEventBuffer buffer;
            player.process (4, buffer);
            player.process (4, buffer);   // playhead 1

            auto sequenceB = makeSequence ({ { 70, true }, { 71, true }, { 72, true } });
            expect (player.publishSequence (sequenceB, berlin::PublishMode::restart));
            player.process (4, buffer);
            expectEquals (player.getPlayheadStep(), 0);
            expect (buffer[buffer.size() - 1] == berlin::StepEvent { 0, 0, 70, true });
        }

        beginTest ("keepPosition: editing the sounding step's note emits the note-off at offset 0 first");
        {
            auto sequenceA = makeSequence ({ { 60, true }, { 62, true }, { 64, true } });
            berlin::SequencePlayer player (sequenceA, berlin::Transport (60.0, 1));
            player.prepare (4.0);
            player.start();

            berlin::StepEventBuffer buffer;
            player.process (4, buffer);
            player.process (3, buffer);   // boundaries 0 and 1 covered, step 1 (62) sounding, position 7

            auto edited = makeSequence ({ { 60, true }, { 63, true }, { 64, true } });
            expect (player.publishSequence (edited, berlin::PublishMode::keepPosition));

            player.process (0, buffer);   // no boundary in this block: only the adopt note-off
            expectEquals (buffer.size(), 1);
            expect (buffer[0] == berlin::StepEvent { 0, 1, 62, false });

            player.process (4, buffer);   // next boundary: step 2, and no second note-off for 62
            expectEquals (buffer.size(), 1);
            expect (buffer[0] == berlin::StepEvent { 1, 2, 64, true });   // boundary 2 at sample 8, position was 7
        }

        beginTest ("keepPosition: muting the sounding step emits a note-off");
        {
            auto sequenceA = makeSequence ({ { 60, true }, { 62, true } });
            berlin::SequencePlayer player (sequenceA, berlin::Transport (60.0, 1));
            player.prepare (4.0);
            player.start();

            berlin::StepEventBuffer buffer;
            player.process (3, buffer);   // step 0 (60) sounding, next boundary at sample 4

            auto edited = makeSequence ({ { 60, false }, { 62, true } });
            expect (player.publishSequence (edited, berlin::PublishMode::keepPosition));

            player.process (0, buffer);
            expectEquals (buffer.size(), 1);
            expect (buffer[0] == berlin::StepEvent { 0, 0, 60, false });
        }

        beginTest ("keepPosition: unchanged sounding step, another step edited, or nothing sounding give no event");
        {
            auto sequenceA = makeSequence ({ { 60, true }, { 62, true }, { 64, true } });
            berlin::SequencePlayer player (sequenceA, berlin::Transport (60.0, 1));
            player.prepare (4.0);
            player.start();

            berlin::StepEventBuffer buffer;
            player.process (3, buffer);   // step 0 (60) sounding, next boundary at sample 4

            auto sameContent = makeSequence ({ { 60, true }, { 62, true }, { 64, true } });
            expect (player.publishSequence (sameContent, berlin::PublishMode::keepPosition));
            player.process (0, buffer);
            expectEquals (buffer.size(), 0);

            auto otherEdited = makeSequence ({ { 60, true }, { 62, false }, { 65, true } });
            expect (player.publishSequence (otherEdited, berlin::PublishMode::keepPosition));
            player.process (0, buffer);
            expectEquals (buffer.size(), 0);

            auto silent = makeSequence ({ { 60, false }, { 62, false } });
            berlin::SequencePlayer quiet (silent, berlin::Transport (60.0, 1));
            quiet.prepare (4.0);
            quiet.start();
            quiet.process (3, buffer);   // nothing sounding

            auto edited = makeSequence ({ { 61, true }, { 62, false } });
            expect (quiet.publishSequence (edited, berlin::PublishMode::keepPosition));
            quiet.process (0, buffer);
            expectEquals (buffer.size(), 0);
        }

        beginTest ("keepPosition: a smaller adopted sequence (pendingStep >= size) is safe and emits the note-off");
        {
            auto sequenceA = makeSequence ({ { 60, true }, { 62, true }, { 64, true } });
            berlin::SequencePlayer player (sequenceA, berlin::Transport (60.0, 1));
            player.prepare (4.0);
            player.start();

            berlin::StepEventBuffer buffer;
            player.process (4, buffer);
            player.process (4, buffer);
            player.process (3, buffer);   // step 2 (64) sounding, position 11

            auto smaller = makeSequence ({ { 60, true }, { 62, true } });
            expect (player.publishSequence (smaller, berlin::PublishMode::keepPosition));

            player.process (0, buffer);
            expectEquals (buffer.size(), 1);
            expect (buffer[0] == berlin::StepEvent { 0, 2, 64, false });
        }
    }
};

static SequencePlayerHandoffTests sequencePlayerHandoffTests;
