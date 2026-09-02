/*
  ==============================================================================

   SequencePlayer::flushPendingNoteOff tests (midi-output-routing spec,
   step-event-scheduling ADDED requirement "Flush Pending Note-Off Before
   Shutdown"). RED first: SequencePlayer::flushPendingNoteOff does not exist
   yet, so this suite must fail to compile until 1.2 adds it.

   Covers: emits exactly one note-off for a currently-sounding note at sample
   offset 0 and clears the pending-note state; is a no-op when nothing is
   sounding; is idempotent (a second call with no processing in between
   emits nothing); ordering against reset() (flush-before-reset emits,
   reset-before-flush emits nothing because reset() already discarded the
   pending-note state); no unmatched note-on across repeated
   start/process/flush/stop cycles, including a stop that lands mid-step and
   a stop with nothing sounding.

   Uses a hand-built Sequence for exact assertions, same convention as
   SequencePlayerTests.cpp. {bpm=60, stepsPerBeat=1} prepared at
   sampleRate=4.0 gives samplesPerStep == 4.0 exactly.

  ==============================================================================
*/

#include <juce_core/juce_core.h>

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

class SequencePlayerStopTests final : public juce::UnitTest
{
public:
    SequencePlayerStopTests() : juce::UnitTest ("SequencePlayerStop", "Berlin") {}

    void runTest() override
    {
        beginTest ("flushPendingNoteOff() is noexcept (compile-time RT-safety contract)");
        {
            berlin::Sequence sequence (1);
            berlin::SequencePlayer player (sequence, berlin::Transport (120.0, 4));
            berlin::StepEventBuffer buffer;

            static_assert (noexcept (player.flushPendingNoteOff (buffer)));
        }

        beginTest ("emits exactly one note-off for a currently-sounding note at sample offset 0 and clears the pending-note state");
        {
            auto sequence = makeSequence ({ { 60, true }, { 62, true } });
            berlin::SequencePlayer player (sequence, berlin::Transport (60.0, 1));
            player.prepare (4.0);
            player.start();

            berlin::StepEventBuffer buffer;
            player.process (4, buffer);   // boundary 0: step 0 note-on 60, now sounding

            const bool emitted = player.flushPendingNoteOff (buffer);

            expect (emitted);
            expectEquals (buffer.size(), 1);
            expect (buffer[0] == berlin::StepEvent { 0, 0, 60, false });

            // pending-note state cleared: a second flush is a no-op.
            const bool emittedAgain = player.flushPendingNoteOff (buffer);
            expect (! emittedAgain);
            expectEquals (buffer.size(), 0);
        }

        beginTest ("is a no-op (buffer ends cleared, nothing pushed) when nothing is sounding");
        {
            auto sequence = makeSequence ({ { 60, false } });   // inactive: nothing ever sounds
            berlin::SequencePlayer player (sequence, berlin::Transport (60.0, 1));
            player.prepare (4.0);
            player.start();

            berlin::StepEventBuffer buffer;
            player.process (4, buffer);   // boundary 0: step 0 inactive, nothing pushed

            const bool emitted = player.flushPendingNoteOff (buffer);

            expect (! emitted);
            expectEquals (buffer.size(), 0);
        }

        beginTest ("is idempotent: a second call with no processing in between emits nothing");
        {
            auto sequence = makeSequence ({ { 60, true } });
            berlin::SequencePlayer player (sequence, berlin::Transport (60.0, 1));
            player.prepare (4.0);
            player.start();

            berlin::StepEventBuffer buffer;
            player.process (4, buffer);   // boundary 0: step 0 note-on 60

            expect (player.flushPendingNoteOff (buffer));
            expectEquals (buffer.size(), 1);

            expect (! player.flushPendingNoteOff (buffer));
            expectEquals (buffer.size(), 0);
            expect (! player.flushPendingNoteOff (buffer));
            expectEquals (buffer.size(), 0);
        }

        beginTest ("ordering vs reset(): flush-before-reset emits, reset-before-flush emits nothing");
        {
            // flush-before-reset emits.
            {
                auto sequence = makeSequence ({ { 60, true } });
                berlin::SequencePlayer player (sequence, berlin::Transport (60.0, 1));
                player.prepare (4.0);
                player.start();

                berlin::StepEventBuffer buffer;
                player.process (4, buffer);   // step 0 note-on 60 sounding

                const bool emitted = player.flushPendingNoteOff (buffer);
                expect (emitted);
                expectEquals (buffer.size(), 1);
                expect (buffer[0] == berlin::StepEvent { 0, 0, 60, false });
            }

            // reset-before-flush emits nothing: reset() already discarded pendingNote.
            {
                auto sequence = makeSequence ({ { 60, true } });
                berlin::SequencePlayer player (sequence, berlin::Transport (60.0, 1));
                player.prepare (4.0);
                player.start();

                berlin::StepEventBuffer buffer;
                player.process (4, buffer);   // step 0 note-on 60 sounding

                player.reset();               // discards pendingNote

                const bool emitted = player.flushPendingNoteOff (buffer);
                expect (! emitted);
                expectEquals (buffer.size(), 0);
            }
        }

        beginTest ("no unmatched note-on across repeated start/process/flush/stop cycles, including stop mid-step and stop with nothing sounding");
        {
            auto sequence = makeSequence ({ { 60, true }, { 62, true }, { 64, true } });   // N = 3
            berlin::SequencePlayer player (sequence, berlin::Transport (60.0, 1));
            player.prepare (4.0);

            berlin::StepEventBuffer buffer;
            int noteOns = 0;
            int noteOffs = 0;

            auto tally = [&] (const berlin::StepEventBuffer& out)
            {
                for (int i = 0; i < out.size(); ++i)
                {
                    if (out[i].isNoteOn)
                        ++noteOns;
                    else
                        ++noteOffs;
                }
            };

            // Cycle 1: start, process a couple of full-step blocks, stop() (no flush), restart via flush.
            player.start();
            player.process (4, buffer);   // boundary 0: step 0 note-on
            tally (buffer);
            player.process (4, buffer);   // boundary 1: step 0 off, step 1 on
            tally (buffer);
            player.stop();                 // stop mid-note (step 1 sounding); stop() does not clear pendingNote
            if (player.flushPendingNoteOff (buffer))
                tally (buffer);

            // Cycle 2: stop with nothing sounding (flush is a no-op).
            player.reset();
            player.start();
            expect (! player.flushPendingNoteOff (buffer));   // nothing sounding yet
            expectEquals (buffer.size(), 0);

            // Cycle 3: stop mid-step (partial block, boundary not yet reached).
            player.process (2, buffer);   // half a step; no boundary crossed since samplesPerStep == 4.0 and position starts at 0... boundary 0 fires at sample 0 immediately
            tally (buffer);
            player.stop();
            if (player.flushPendingNoteOff (buffer))
                tally (buffer);

            expectEquals (noteOns, noteOffs);
        }
    }
};

static SequencePlayerStopTests sequencePlayerStopTests;
