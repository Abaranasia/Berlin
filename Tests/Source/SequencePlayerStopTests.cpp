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

        // ---- ui-engine-api Phase 1 (D1-D4): transition-only play/stop
        // adoption via setPlaying()/isPlayRequested(). RED first: neither
        // member exists yet, so this suite must fail to compile until 1.6/1.7
        // add them.

        beginTest ("setPlaying(false) adopted mid-step 5 emits exactly one note-off at offset 0, playhead frozen");
        {
            // 8 steps, all active, one note per step (sr=4, bpm=60, spb=1 -> samplesPerStep==4.0).
            auto sequence = makeSequence ({ { 60, true }, { 61, true }, { 62, true }, { 63, true },
                                             { 64, true }, { 65, true }, { 66, true }, { 67, true } });
            berlin::SequencePlayer player (sequence, berlin::Transport (60.0, 1));
            player.prepare (4.0);
            player.start();

            berlin::StepEventBuffer buffer;

            for (int i = 0; i < 11; ++i)       // 11 x 2-sample blocks: samples [0,22) -> step 5's
                player.process (2, buffer);    // note-on emitted at boundary 20; position now 22 (2 samples into step 5)

            expectEquals (player.getPlayheadStep(), 5);

            player.setPlaying (false);

            player.process (2, buffer);
            expectEquals (buffer.size(), 1);
            expect (buffer[0] == berlin::StepEvent { 0, 5, 65, false });

            expectEquals (player.getPlayheadStep(), 5);   // frozen: no boundary adopted this block
        }

        beginTest ("isPlayRequested() reflects the requested state immediately, zero process() calls made");
        {
            berlin::Sequence sequence (1);
            berlin::SequencePlayer player (sequence, berlin::Transport (60.0, 1));

            expect (! player.isPlayRequested());   // ctor-initialised from transport.isRunning() == false

            player.setPlaying (true);
            expect (player.isPlayRequested());       // reflects the request with zero process() calls made
        }

        beginTest ("a direct stop() (no setPlaying request) still leaves pendingNote set; the following process() emits nothing");
        {
            auto sequence = makeSequence ({ { 60, true }, { 62, true } });
            berlin::SequencePlayer player (sequence, berlin::Transport (60.0, 1));
            player.prepare (4.0);
            player.start();

            berlin::StepEventBuffer buffer;
            player.process (4, buffer);   // boundary 0: step 0 note-on 60, now sounding

            player.stop();   // direct stop: transport stops synchronously; pendingNote untouched by this call

            player.process (4, buffer);   // no transition for the adopt logic to detect (transport already stopped)
            expectEquals (buffer.size(), 0);

            // The pending note-off is still recoverable via the existing flush seam.
            expect (player.flushPendingNoteOff (buffer));
            expectEquals (buffer.size(), 1);
            expect (buffer[0] == berlin::StepEvent { 0, 0, 60, false });
        }

        beginTest ("restart resumes at the next step boundary: setPlaying(true) after the stop emits exactly one event, step 6's note-on, no step 5 or step 0");
        {
            auto sequence = makeSequence ({ { 60, true }, { 61, true }, { 62, true }, { 63, true },
                                             { 64, true }, { 65, true }, { 66, true }, { 67, true } });
            berlin::SequencePlayer player (sequence, berlin::Transport (60.0, 1));
            player.prepare (4.0);
            player.start();

            berlin::StepEventBuffer buffer;

            for (int i = 0; i < 11; ++i)       // samples [0,22): step 5's note-on emitted at boundary 20
                player.process (2, buffer);

            player.setPlaying (false);
            player.process (2, buffer);        // adopts the stop: one note-off for step 5, playhead frozen at 5

            player.setPlaying (true);
            player.process (4, buffer);        // adopts the restart: resumes at step 6's original grid position

            expectEquals (buffer.size(), 1);
            expect (buffer[0] == berlin::StepEvent { 2, 6, 66, true });   // offset 2 == remainder of step 5

            for (int i = 0; i < buffer.size(); ++i)
            {
                expect (buffer[i].stepIndex != 5);
                expect (buffer[i].stepIndex != 0);
            }

            expectEquals (player.getPlayheadStep(), 6);
        }
    }
};

static SequencePlayerStopTests sequencePlayerStopTests;
