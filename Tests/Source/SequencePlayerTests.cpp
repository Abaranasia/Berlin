/*
  ==============================================================================

   SequencePlayer tests (playback-transport-clock spec, Phase 4). RED first:
   Source/playback/SequencePlayer.h does not exist yet, so this suite must
   fail to compile until 4.2 creates it. Covers note-off-before-note-on
   ordering at a shared sample offset, inactive/empty/all-inactive silence,
   continuity across the loop wrap (step-event-scheduling's "Continuous
   Emission Across the Loop Wrap" and "Step Position Wraps Indefinitely"
   requirements), emitted note-on values matching the seeded Sequence in
   order across irregular block sizes (including some shorter than one
   step), and StepEventBuffer overflow at extreme step density (drop-and-
   flag, no crash). Uses a fixed hand-built Sequence, not a generated one,
   so assertions are exact.

   Boundary k=0 fires at absolute sample 0 (Transport's documented
   immediate-first-step behaviour, confirmed in Phase 3's TransportTests) -
   step 0's note-on (if active) is expected on the VERY FIRST process() call
   at sampleOffset 0, with no initial one-step delay. Most scenarios below
   use {bpm=60, stepsPerBeat=1} prepared at sampleRate=4.0, giving an exact
   samplesPerStep of 4.0 samples/step so expected sample offsets are exact
   round numbers rather than requiring an independent llround() re-derivation
   in the test.

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

class SequencePlayerTests final : public juce::UnitTest
{
public:
    SequencePlayerTests() : juce::UnitTest ("SequencePlayer", "Berlin") {}

    void runTest() override
    {
        beginTest ("process() is noexcept (compile-time RT-safety contract)");
        {
            berlin::Sequence sequence (1);
            berlin::SequencePlayer player (sequence, berlin::Transport (120.0, 4));
            berlin::StepEventBuffer buffer;

            static_assert (noexcept (player.process (0, buffer)));
        }

        beginTest ("note-off for an ending step is emitted before the note-on of the following step at a shared sample offset");
        {
            auto sequence = makeSequence ({ { 60, true }, { 62, true } });
            berlin::SequencePlayer player (sequence, berlin::Transport (60.0, 1));
            player.prepare (4.0);   // samplesPerStep == 4.0 exactly
            player.start();

            berlin::StepEventBuffer buffer;

            // Block 1 covers boundary k=0 only (sample 0): step 0's note-on, no prior note-off.
            player.process (4, buffer);
            expectEquals (buffer.size(), 1);
            expect (buffer[0] == berlin::StepEvent { 0, 0, 60, true });

            // Block 2 covers boundary k=1 (sample 4): step 0's note-off THEN step 1's note-on, same offset.
            player.process (4, buffer);
            expectEquals (buffer.size(), 2);
            expect (buffer[0] == berlin::StepEvent { 0, 0, 60, false });
            expect (buffer[1] == berlin::StepEvent { 0, 1, 62, true });
        }

        beginTest ("inactive step emits nothing");
        {
            auto sequence = makeSequence ({ { 60, true }, { 0, false }, { 64, true } });
            berlin::SequencePlayer player (sequence, berlin::Transport (60.0, 1));
            player.prepare (4.0);
            player.start();

            berlin::StepEventBuffer buffer;

            player.process (4, buffer);   // boundary 0: step 0 note-on
            expectEquals (buffer.size(), 1);
            expect (buffer[0] == berlin::StepEvent { 0, 0, 60, true });

            player.process (4, buffer);   // boundary 1: step 0 note-off; step 1 inactive -> no note-on
            expectEquals (buffer.size(), 1);
            expect (buffer[0] == berlin::StepEvent { 0, 0, 60, false });

            player.process (4, buffer);   // boundary 2: no pending note-off (step 1 was inactive); step 2 note-on
            expectEquals (buffer.size(), 1);
            expect (buffer[0] == berlin::StepEvent { 0, 2, 64, true });
        }

        beginTest ("empty sequence emits no events across any number of processed blocks");
        {
            const berlin::Sequence empty;   // size() == 0
            berlin::SequencePlayer player (empty, berlin::Transport (60.0, 1));
            player.prepare (4.0);
            player.start();

            berlin::StepEventBuffer buffer;

            for (int i = 0; i < 50; ++i)
            {
                player.process (4, buffer);
                expectEquals (buffer.size(), 0);
            }
        }

        beginTest ("sequence where every step is inactive emits no events across any number of processed blocks");
        {
            auto sequence = makeSequence ({ { 60, false }, { 62, false }, { 64, false } });
            berlin::SequencePlayer player (sequence, berlin::Transport (60.0, 1));
            player.prepare (4.0);
            player.start();

            berlin::StepEventBuffer buffer;

            for (int i = 0; i < 50; ++i)
            {
                player.process (4, buffer);
                expectEquals (buffer.size(), 0);
            }
        }

        beginTest ("wrap from step N-1 to step 0 has no timing gap and no duplicated or dropped event");
        {
            auto sequence = makeSequence ({ { 60, true }, { 62, true }, { 64, true } });   // N = 3
            berlin::SequencePlayer player (sequence, berlin::Transport (60.0, 1));
            player.prepare (4.0);
            player.start();

            berlin::StepEventBuffer buffer;

            player.process (4, buffer);   // boundary 0 (step 0): note-on 60
            player.process (4, buffer);   // boundary 1 (step 1): off 60, on 62
            player.process (4, buffer);   // boundary 2 (step 2): off 62, on 64
            player.process (4, buffer);   // boundary 3 wraps to step 0: off 64, on 60 - the wrap point

            expectEquals (buffer.size(), 2);
            expect (buffer[0] == berlin::StepEvent { 0, 2, 64, false });
            expect (buffer[1] == berlin::StepEvent { 0, 0, 60, true });
            expectEquals (player.getPlayheadStep(), 0);
        }

        beginTest ("getPlayheadStep() wraps to 0 on completing Sequence::size() steps and keeps wrapping indefinitely");
        {
            auto sequence = makeSequence ({ { 60, true }, { 62, true }, { 64, true } });   // N = 3
            berlin::SequencePlayer player (sequence, berlin::Transport (60.0, 1));
            player.prepare (4.0);
            player.start();

            berlin::StepEventBuffer buffer;
            std::vector<int> observedPlayhead;

            for (int i = 0; i < 12; ++i)   // 4 full loops of 3 steps
            {
                player.process (4, buffer);
                observedPlayhead.push_back (player.getPlayheadStep());
            }

            const std::vector<int> expected { 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2 };
            expect (observedPlayhead == expected);
        }

        beginTest ("emitted note-on values match the seeded sequence's note fields in order, across irregular block sizes including some shorter than one step");
        {
            auto sequence = makeSequence ({ { 60, true }, { 62, true }, { 64, true }, { 67, true } });   // N = 4
            berlin::SequencePlayer player (sequence, berlin::Transport (60.0, 1));
            player.prepare (4.0);   // samplesPerStep == 4.0
            player.start();

            std::vector<int> noteOnsInOrder;
            berlin::StepEventBuffer buffer;

            // Irregular, varying block sizes; several are shorter than one step (samplesPerStep == 4).
            const std::vector<int> blockSizes { 1, 1, 2, 3, 1, 5, 2, 4, 6, 1, 1, 2 };   // sums to 29 samples

            for (const auto blockSize : blockSizes)
            {
                player.process (blockSize, buffer);

                for (int i = 0; i < buffer.size(); ++i)
                    if (buffer[i].isNoteOn)
                        noteOnsInOrder.push_back (buffer[i].note);
            }

            // Boundaries k=0..7 (llround(k*4) < 29) fire across the run, cycling through the 4-step sequence twice.
            const std::vector<int> expected { 60, 62, 64, 67, 60, 62, 64, 67 };
            expect (noteOnsInOrder == expected);
        }

        beginTest ("StepEventBuffer overflow at extreme step-density is dropped and flagged, no crash");
        {
            auto sequence = makeSequence ({ { 60, true }, { 62, true } });
            berlin::SequencePlayer player (sequence, berlin::Transport (60.0, 1));
            player.prepare (1.0);   // sampleRate=1 -> samplesPerStep = 1*60/(60*1) = 1.0, the minimum prepared value
            player.start();

            berlin::StepEventBuffer buffer;

            // 200 boundaries in one block at samplesPerStep == 1.0 -> up to 200 events, far exceeding capacity 64.
            player.process (200, buffer);

            expect (buffer.hasOverflowed());
            expectEquals (buffer.size(), berlin::StepEventBuffer::capacity);
        }
    }
};

static SequencePlayerTests sequencePlayerTests;
