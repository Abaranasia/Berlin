/*
  ==============================================================================

   MidiExportTimeline tests (midi-export-timeline spec, Phase 6 / midi-export).
   RED first: Source/export/MidiExportTimeline.h does not exist yet, so this
   suite must fail to compile until 1.2 creates it.

   Covers: exact-integer tick derivation (ticksPerStep == 240 @ stepsPerBeat=4,
   every step boundary n*ticksPerStep); no drift across repeats > 1;
   consecutive active steps sharing a boundary tick with note-off before
   note-on; full event list non-decreasing in tick; terminal note-off at
   endTick when the last step is active, none when inactive; single-step
   all-active + repeats=1 yields exactly one on/off pair; repeats 1/2/4
   produce contiguous non-overlapping passes with no seam gap/duplicate;
   invalid repeats/stepsPerBeat rejection; inactive-step silence; degenerate
   (empty / all-inactive) sequences; and an equivalence check against live
   SequencePlayer playback (task 1.5).

   Hand-built Sequence per case, same convention as SequencePlayerTests.cpp.

  ==============================================================================
*/

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

#include <algorithm>
#include <cmath>
#include <type_traits>
#include <vector>

#include "GateStreamSupport.h"
#include "core/Sequence.h"
#include "core/Step.h"
#include "export/MidiExportTimeline.h"
#include "export/MidiFileWriter.h"
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

    // Walks `events` maintaining a sounding-note set; returns true iff every
    // note-on has exactly one later note-off on the same pitch and the set is
    // empty at the end (no note left hanging).
    bool everyNoteOnIsClosed (const std::vector<berlin::MidiExportEvent>& events)
    {
        std::vector<int> sounding;

        for (const auto& e : events)
        {
            if (e.noteOn)
            {
                sounding.push_back (e.note);
            }
            else
            {
                auto it = std::find (sounding.begin(), sounding.end(), e.note);
                if (it == sounding.end())
                    return false;   // note-off with no matching sounding note-on
                sounding.erase (it);
            }
        }

        return sounding.empty();
    }

    // Writes `timeline` through MidiFileWriter and returns the note-on velocities
    // (and, via `offVelocities`, the note-off ones) read back from the file bytes.
    std::vector<int> writtenVelocities (const berlin::MidiExportTimeline& timeline, bool noteOns)
    {
        juce::MemoryOutputStream stream;
        const berlin::MidiFileWriter writer (1);
        writer.writeTo (timeline, 120.0, stream);

        juce::MidiFile file;
        juce::MemoryInputStream input (stream.getData(), stream.getDataSize(), false);
        file.readFrom (input);

        std::vector<int> velocities;

        for (auto* event : *file.getTrack (0))
        {
            if (noteOns && event->message.isNoteOn())
                velocities.push_back (event->message.getVelocity());
            else if (! noteOns && event->message.isNoteOff())
                velocities.push_back (event->message.getVelocity());
        }

        return velocities;
    }

    bool isNonDecreasingInTick (const std::vector<berlin::MidiExportEvent>& events)
    {
        for (std::size_t i = 1; i < events.size(); ++i)
            if (events[i].tick < events[i - 1].tick)
                return false;
        return true;
    }

    // No note-on ever precedes a same-tick note-off: a note-on immediately
    // followed by a note-off at the SAME tick would be the wrong order.
    bool noteOffPrecedesNoteOnAtSharedTick (const std::vector<berlin::MidiExportEvent>& events)
    {
        for (std::size_t i = 1; i < events.size(); ++i)
            if (events[i].tick == events[i - 1].tick && events[i - 1].noteOn && ! events[i].noteOn)
                return false;
        return true;
    }
}

static_assert (std::is_aggregate_v<berlin::MidiExportEvent>);
static_assert (berlin::kTicksPerQuarterNote % 4 == 0);

class MidiExportTimelineTests final : public juce::UnitTest
{
public:
    MidiExportTimelineTests() : juce::UnitTest ("MidiExportTimeline", "Berlin") {}

    void runTest() override
    {
        beginTest ("ticksPerStep is exactly 240 at stepsPerBeat = 4, and step boundaries land on exact multiples");
        {
            auto sequence = makeSequence ({ { 60, true }, { 62, true }, { 64, true }, { 67, true } });
            berlin::MidiExportTimeline out;

            const auto status = berlin::buildMidiExportTimeline (sequence, 4, 1, out);

            expect (status == berlin::MidiExportStatus::ok);
            expectEquals (out.ticksPerStep, 240);
            expectEquals (out.ticksPerQuarterNote, berlin::kTicksPerQuarterNote);

            // Step boundaries: 0, 240, 480, 720 for steps 0..3.
            expect (out.events[0] == berlin::MidiExportEvent { 0, 60, true });
            expect (out.events[1] == berlin::MidiExportEvent { 240, 60, false });
            expect (out.events[2] == berlin::MidiExportEvent { 240, 62, true });
            expect (out.events[3] == berlin::MidiExportEvent { 480, 62, false });
            expect (out.events[4] == berlin::MidiExportEvent { 480, 64, true });
            expect (out.events[5] == berlin::MidiExportEvent { 720, 64, false });
            expect (out.events[6] == berlin::MidiExportEvent { 720, 67, true });
        }

        beginTest ("no accumulated drift across repeats > 1: every repeat's boundaries are exact multiples of ticksPerStep");
        {
            auto sequence = makeSequence ({ { 60, true }, { 62, true } });
            berlin::MidiExportTimeline out;

            const auto status = berlin::buildMidiExportTimeline (sequence, 4, 3, out);

            expect (status == berlin::MidiExportStatus::ok);

            for (const auto& e : out.events)
                expectEquals (e.tick % out.ticksPerStep, (long long) 0);

            expectEquals (out.endTick, (long long) 3 * 2 * 240);
        }

        beginTest ("consecutive active steps share a boundary tick with note-off ordered before note-on");
        {
            auto sequence = makeSequence ({ { 60, true }, { 62, true } });
            berlin::MidiExportTimeline out;

            berlin::buildMidiExportTimeline (sequence, 4, 1, out);

            // Boundary tick 240 shared by step 0's note-off and step 1's note-on.
            expect (out.events[1] == berlin::MidiExportEvent { 240, 60, false });
            expect (out.events[2] == berlin::MidiExportEvent { 240, 62, true });
        }

        beginTest ("full event list is non-decreasing in tick across multiple active steps and repeats");
        {
            auto sequence = makeSequence ({ { 60, true }, { 0, false }, { 64, true }, { 67, true } });
            berlin::MidiExportTimeline out;

            berlin::buildMidiExportTimeline (sequence, 4, 3, out);

            expect (isNonDecreasingInTick (out.events));
            expect (noteOffPrecedesNoteOnAtSharedTick (out.events));
        }

        beginTest ("terminal note-off: last step active gets a closing note-off at endTick, no note-on left unmatched");
        {
            auto sequence = makeSequence ({ { 60, true }, { 62, true } });
            berlin::MidiExportTimeline out;

            berlin::buildMidiExportTimeline (sequence, 4, 1, out);

            expectEquals (out.endTick, (long long) 480);
            expect (out.events.back() == berlin::MidiExportEvent { 480, 62, false });
            expect (everyNoteOnIsClosed (out.events));
        }

        beginTest ("terminal note-off: last step inactive emits no event at endTick");
        {
            auto sequence = makeSequence ({ { 60, true }, { 0, false } });
            berlin::MidiExportTimeline out;

            berlin::buildMidiExportTimeline (sequence, 4, 1, out);

            expectEquals (out.endTick, (long long) 480);
            for (const auto& e : out.events)
                expect (e.tick != out.endTick);
            expect (everyNoteOnIsClosed (out.events));
        }

        beginTest ("single-step all-active sequence with repeats = 1 yields exactly one on/off pair");
        {
            auto sequence = makeSequence ({ { 60, true } });
            berlin::MidiExportTimeline out;

            berlin::buildMidiExportTimeline (sequence, 4, 1, out);

            expectEquals ((int) out.events.size(), 2);
            expect (out.events[0] == berlin::MidiExportEvent { 0, 60, true });
            expect (out.events[1] == berlin::MidiExportEvent { 240, 60, false });
        }

        beginTest ("repeats 1, 2, 4 produce contiguous non-overlapping passes with no seam gap or duplicate");
        {
            auto sequence = makeSequence ({ { 60, true }, { 62, true }, { 64, true } });

            berlin::MidiExportTimeline one, two, four;
            berlin::buildMidiExportTimeline (sequence, 4, 1, one);
            berlin::buildMidiExportTimeline (sequence, 4, 2, two);
            berlin::buildMidiExportTimeline (sequence, 4, 4, four);

            // Every step is active, so every step contributes exactly one
            // note-on and one note-off (either merged into the next step's
            // boundary or, for the very last step, the terminal note-off) -
            // total event count is always 2 * totalSteps regardless of how
            // repeats are internally grouped.
            expectEquals ((int) one.events.size(), 2 * 1 * 3);
            expectEquals ((int) two.events.size(), 2 * 2 * 3);
            expectEquals ((int) four.events.size(), 2 * 4 * 3);

            expectEquals (two.endTick, (long long) 2 * 3 * 240);
            expectEquals (four.endTick, (long long) 4 * 3 * 240);

            expect (isNonDecreasingInTick (two.events));
            expect (isNonDecreasingInTick (four.events));
            expect (everyNoteOnIsClosed (two.events));
            expect (everyNoteOnIsClosed (four.events));

            // Seam check: the last step of repeat 1 (index 2, tick 720) and the
            // first step of repeat 2 (tick 720) must produce neither a gap nor a
            // duplicated event - exactly one note-off then one note-on at tick 720.
            int atSeam = 0;
            for (const auto& e : two.events)
                if (e.tick == 720)
                    ++atSeam;
            expectEquals (atSeam, 2);
        }

        beginTest ("invalid repeat count (0 or negative) is rejected, out.events left empty");
        {
            auto sequence = makeSequence ({ { 60, true } });

            berlin::MidiExportTimeline outZero;
            outZero.events.push_back ({ 1, 2, true });   // pre-seed to prove it gets cleared
            const auto statusZero = berlin::buildMidiExportTimeline (sequence, 4, 0, outZero);
            expect (statusZero == berlin::MidiExportStatus::invalidRepeatCount);
            expect (outZero.events.empty());

            berlin::MidiExportTimeline outNeg;
            const auto statusNeg = berlin::buildMidiExportTimeline (sequence, 4, -1, outNeg);
            expect (statusNeg == berlin::MidiExportStatus::invalidRepeatCount);
            expect (outNeg.events.empty());
        }

        beginTest ("invalid stepsPerBeat (0, negative, or non-divisor of 960) is rejected, never rounded");
        {
            auto sequence = makeSequence ({ { 60, true } });

            berlin::MidiExportTimeline outZero;
            expect (berlin::buildMidiExportTimeline (sequence, 0, 1, outZero) == berlin::MidiExportStatus::invalidStepsPerBeat);
            expect (outZero.events.empty());

            berlin::MidiExportTimeline outNeg;
            expect (berlin::buildMidiExportTimeline (sequence, -1, 1, outNeg) == berlin::MidiExportStatus::invalidStepsPerBeat);
            expect (outNeg.events.empty());

            berlin::MidiExportTimeline outSeven;
            expect (berlin::buildMidiExportTimeline (sequence, 7, 1, outSeven) == berlin::MidiExportStatus::invalidStepsPerBeat);
            expect (outSeven.events.empty());
        }

        beginTest ("inactive step is silent - no event emitted for it");
        {
            auto sequence = makeSequence ({ { 60, true }, { 0, false }, { 64, true } });
            berlin::MidiExportTimeline out;

            berlin::buildMidiExportTimeline (sequence, 4, 1, out);

            // Step 0 on, step 0 off / step 2 has no preceding pending note issue
            // since step 1 is inactive: off(60)@240 then on(64)@480, no event at
            // step 1's boundary carries note 0.
            for (const auto& e : out.events)
                expect (e.note != 0);
        }

        beginTest ("empty Sequence() yields ok, zero events, endTick == 0");
        {
            const berlin::Sequence empty;
            berlin::MidiExportTimeline out;

            const auto status = berlin::buildMidiExportTimeline (empty, 4, 5, out);

            expect (status == berlin::MidiExportStatus::ok);
            expect (out.events.empty());
            expectEquals (out.endTick, (long long) 0);
        }

        beginTest ("all-inactive sequence yields ok, zero events, correct endTick");
        {
            auto sequence = makeSequence ({ { 60, false }, { 62, false }, { 64, false } });
            berlin::MidiExportTimeline out;

            const auto status = berlin::buildMidiExportTimeline (sequence, 4, 2, out);

            expect (status == berlin::MidiExportStatus::ok);
            expect (out.events.empty());
            expectEquals (out.endTick, (long long) 2 * 3 * 240);
        }

        beginTest ("equivalence with live playback: export order/pitches match SequencePlayer::process + flushPendingNoteOff over one pass");
        {
            auto sequence = makeSequence ({ { 60, true }, { 62, true }, { 0, false }, { 64, true } });

            // Drive SequencePlayer at a sample rate where samplesPerStep is
            // integral: bpm=60, stepsPerBeat=4 -> samplesPerStep = sampleRate*60/(60*4)
            // = sampleRate/4. sampleRate = 4 -> samplesPerStep == 1.0 exactly.
            berlin::SequencePlayer player (sequence, berlin::Transport (60.0, 4));
            player.prepare (4.0);
            player.start();

            struct Ordered { int note; bool noteOn; };
            std::vector<Ordered> live;

            berlin::StepEventBuffer buffer;
            for (int i = 0; i < 4; ++i)   // one full pass, 4 steps @ 1 sample/step
            {
                player.process (1, buffer);
                for (int j = 0; j < buffer.size(); ++j)
                    live.push_back ({ buffer[j].note, buffer[j].isNoteOn });
            }
            // Flush the terminal pending note-off the same way the real export
            // timeline emits its terminal note-off.
            if (player.flushPendingNoteOff (buffer))
                for (int j = 0; j < buffer.size(); ++j)
                    live.push_back ({ buffer[j].note, buffer[j].isNoteOn });

            berlin::MidiExportTimeline exported;
            berlin::buildMidiExportTimeline (sequence, 4, 1, exported);

            std::vector<Ordered> fromExport;
            for (const auto& e : exported.events)
                fromExport.push_back ({ e.note, e.noteOn });

            expectEquals ((int) fromExport.size(), (int) live.size());
            for (std::size_t i = 0; i < live.size(); ++i)
            {
                expectEquals (fromExport[i].note, live[i].note);
                expect (fromExport[i].noteOn == live[i].noteOn);
            }
        }

        beginTest ("a skipped step consumes zero ticks: the timeline contains only the non-skipped steps");
        {
            auto sequence = makeSequence ({ { 60, true }, { 62, true, true }, { 64, true } });
            berlin::MidiExportTimeline out;

            const auto status = berlin::buildMidiExportTimeline (sequence, 4, 2, out);

            expect (status == berlin::MidiExportStatus::ok);
            expectEquals (out.endTick, (long long) 2 * 2 * 240);   // repeats * playable steps * ticksPerStep

            // 60, 64, 60, 64 on ticks 0, 240, 480, 720 with note-offs at the next boundary.
            expectEquals ((int) out.events.size(), 8);
            expect (out.events[0] == berlin::MidiExportEvent { 0, 60, true });
            expect (out.events[1] == berlin::MidiExportEvent { 240, 60, false });
            expect (out.events[2] == berlin::MidiExportEvent { 240, 64, true });
            expect (out.events[7] == berlin::MidiExportEvent { 960, 64, false });

            for (const auto& e : out.events)
                expect (e.note != 62);
        }

        beginTest ("all-skipped sequence yields ok, zero events, endTick == 0");
        {
            auto sequence = makeSequence ({ { 60, true, true }, { 62, true, true } });
            berlin::MidiExportTimeline out;

            const auto status = berlin::buildMidiExportTimeline (sequence, 4, 3, out);

            expect (status == berlin::MidiExportStatus::ok);
            expect (out.events.empty());
            expectEquals (out.endTick, (long long) 0);
        }

        beginTest ("equivalence with live playback: skip/rest mix over two repeats matches player offsets as ticks");
        {
            auto sequence = makeSequence ({ { 60, true, true }, { 62, true }, { 0, false }, { 64, true }, { 65, true, true }, { 67, true } });

            // bpm=60, stepsPerBeat=4 @ sampleRate 4 -> one sample per step, 240 ticks per sample.
            berlin::SequencePlayer player (sequence, berlin::Transport (60.0, 4));
            player.prepare (4.0);
            player.start();

            struct Ordered { long long tick; int note; bool noteOn; };
            std::vector<Ordered> live;

            berlin::StepEventBuffer buffer;
            for (int i = 0; i < 2 * 4; ++i)   // two passes of the 4 playable steps
            {
                player.process (1, buffer);
                for (int j = 0; j < buffer.size(); ++j)
                    live.push_back ({ (long long) (i + buffer[j].sampleOffset) * 240, buffer[j].note, buffer[j].isNoteOn });
            }

            if (player.flushPendingNoteOff (buffer))
                live.push_back ({ (long long) 2 * 4 * 240, buffer[0].note, buffer[0].isNoteOn });

            berlin::MidiExportTimeline exported;
            berlin::buildMidiExportTimeline (sequence, 4, 2, exported);

            expectEquals ((int) exported.events.size(), (int) live.size());
            for (std::size_t i = 0; i < live.size() && i < exported.events.size(); ++i)
            {
                expectEquals (exported.events[i].tick, live[i].tick);
                expectEquals (exported.events[i].note, live[i].note);
                expect (exported.events[i].noteOn == live[i].noteOn);
            }
        }

        beginTest ("note-ons carry the step velocity and note-offs keep the default");
        {
            auto sequence = makeSequence ({ { 60, true, false, 40 }, { 62, true, false, 127 }, { 64, false } });
            berlin::MidiExportTimeline out;

            expect (berlin::buildMidiExportTimeline (sequence, 4, 1, out) == berlin::MidiExportStatus::ok);

            expectEquals ((int) out.events.size(), 4);
            expect (out.events[0] == berlin::MidiExportEvent { 0, 60, true, 40 });
            expect (out.events[1] == berlin::MidiExportEvent { 240, 60, false });
            expect (out.events[2] == berlin::MidiExportEvent { 240, 62, true, 127 });
            expect (out.events[3] == berlin::MidiExportEvent { 480, 62, false });
        }

        beginTest ("a default sequence exports every note-on at velocity 100");
        {
            auto sequence = makeSequence ({ { 60, true }, { 62, true }, { 64, true } });
            berlin::MidiExportTimeline out;

            expect (berlin::buildMidiExportTimeline (sequence, 4, 2, out) == berlin::MidiExportStatus::ok);

            int noteOns = 0;

            for (const auto& e : out.events)
                if (e.noteOn)
                {
                    expectEquals (e.velocity, 100);
                    ++noteOns;
                }

            expectEquals (noteOns, 6);
        }

        beginTest ("MidiFileWriter writes each event's own velocity, clamps note-ons to 1-127, and note-offs stay 0");
        {
            auto sequence = makeSequence ({ { 60, true, false, 40 }, { 62, true, false, 127 }, { 64, true } });
            berlin::MidiExportTimeline out;
            expect (berlin::buildMidiExportTimeline (sequence, 4, 1, out) == berlin::MidiExportStatus::ok);

            const auto onVelocities = writtenVelocities (out, true);
            expectEquals ((int) onVelocities.size(), 3);
            expectEquals (onVelocities[0], 40);
            expectEquals (onVelocities[1], 127);
            expectEquals (onVelocities[2], 100);

            for (int offVelocity : writtenVelocities (out, false))
                expectEquals (offVelocity, 0);

            auto outOfRange = out;
            outOfRange.events[0].velocity = 0;      // would be a note-off on the wire
            outOfRange.events[4].velocity = 200;
            const auto clamped = writtenVelocities (outOfRange, true);
            expectEquals ((int) clamped.size(), 3);   // still three note-ons
            expectEquals (clamped[0], 1);
            expectEquals (clamped[1], 127);
            expectEquals (clamped[2], 127);
        }

        // ---- step-gate: gate-scaled note-off ticks

        beginTest ("gate places the note-off at gate percent of the step: 25/50/75/100 at 240 ticks per step give 60/120/180/240");
        {
            const int gates[] = { 25, 50, 75, 100 };
            const long long offTicks[] = { 60, 120, 180, 240 };

            for (int g = 0; g < 4; ++g)
            {
                auto sequence = makeSequence ({ { 60, true, false, 100, gates[g] } });
                berlin::MidiExportTimeline out;
                expect (berlin::buildMidiExportTimeline (sequence, 4, 1, out) == berlin::MidiExportStatus::ok);

                expect (out.events == std::vector<berlin::MidiExportEvent> { { 0, 60, true }, { offTicks[g], 60, false } },
                        "gate " + juce::String (gates[g]));
                expectEquals ((int) out.endTick, 240);
            }
        }

        beginTest ("a gate that rounds to zero ticks still lasts one tick, including the one-tick-per-step clamp");
        {
            const int stepsPerBeat[] = { 96, 960, 960 };   // 10, 1 and 1 ticks per step
            const int gates[]        = { 1,  1,   99 };

            for (int k = 0; k < 3; ++k)
            {
                auto sequence = makeSequence ({ { 60, true, false, 100, gates[k] } });
                berlin::MidiExportTimeline out;
                expect (berlin::buildMidiExportTimeline (sequence, stepsPerBeat[k], 1, out) == berlin::MidiExportStatus::ok);

                expectEquals ((int) out.events.size(), 2);
                expectEquals ((int) out.events[1].tick, 1);
                expect (! out.events[1].noteOn);
            }
        }

        beginTest ("mixed gates across repeats keep the list non-decreasing with the note-off before a shared-tick note-on");
        {
            auto sequence = makeSequence ({ { 60, true, false, 100, 50 }, { 62, true }, { 64, true, false, 100, 75 }, { 65, false } });
            berlin::MidiExportTimeline out;
            expect (berlin::buildMidiExportTimeline (sequence, 4, 3, out) == berlin::MidiExportStatus::ok);

            expectEquals ((int) out.events.size(), 18);   // 3 repeats x 3 active steps x on/off

            for (size_t i = 1; i < out.events.size(); ++i)
            {
                expect (out.events[i].tick >= out.events[i - 1].tick);

                if (out.events[i].tick == out.events[i - 1].tick)
                    expect (! (out.events[i - 1].noteOn && ! out.events[i].noteOn),
                            "a note-on must not precede a note-off at a shared tick");
            }

            expect (everyNoteOnIsClosed (out.events));
        }

        beginTest ("a short last step has its single note-off inside the step, no terminal note-off, and an unchanged grid length");
        {
            auto sequence = makeSequence ({ { 60, true }, { 62, true, false, 100, 50 } });
            berlin::MidiExportTimeline out;
            expect (berlin::buildMidiExportTimeline (sequence, 4, 1, out) == berlin::MidiExportStatus::ok);

            expect (out.events == std::vector<berlin::MidiExportEvent> { { 0, 60, true },   { 240, 60, false },
                                                                         { 240, 62, true }, { 360, 62, false } });
            expectEquals ((int) out.endTick, 480);   // gate does not change the grid
            expect (everyNoteOnIsClosed (out.events));

            auto full = makeSequence ({ { 60, true }, { 62, true } });
            berlin::MidiExportTimeline legacy;
            expect (berlin::buildMidiExportTimeline (full, 4, 1, legacy) == berlin::MidiExportStatus::ok);
            expect (legacy.events.back() == berlin::MidiExportEvent { 480, 62, false });   // gate 100 keeps the terminal off
        }

        beginTest ("skipped and inactive steps with a short gate emit nothing and leave the other ticks unchanged");
        {
            auto gated   = makeSequence ({ { 60, true }, { 62, false, false, 100, 25 }, { 64, true, true, 100, 25 }, { 65, true } });
            auto neutral = makeSequence ({ { 60, true }, { 62, false },                 { 64, true, true },          { 65, true } });

            berlin::MidiExportTimeline a, b;
            expect (berlin::buildMidiExportTimeline (gated, 4, 2, a) == berlin::MidiExportStatus::ok);
            expect (berlin::buildMidiExportTimeline (neutral, 4, 2, b) == berlin::MidiExportStatus::ok);

            expectEquals ((int) a.events.size(), 8);   // 2 repeats x 2 active steps x on/off
            expect (a.events == b.events);
            expectEquals ((int) a.endTick, (int) b.endTick);
        }

        beginTest ("sample/tick parity: the live gate length and the exported gate length agree for every gate 1..99");
        {
            // 96 kHz, 60 BPM, 4 steps per beat: 24000 samples and 240 ticks per step
            for (int gate = 1; gate <= 99; ++gate)
            {
                auto sequence = makeSequence ({ { 60, true, false, 100, gate } });

                berlin::SequencePlayer player (sequence, berlin::Transport (60.0, 4));
                player.prepare (96000.0);
                player.start();

                long long position = 0;
                bool ok = true;
                const auto live = drain (player, position, 24000, 24000, ok);

                berlin::MidiExportTimeline out;
                expect (berlin::buildMidiExportTimeline (sequence, 4, 1, out) == berlin::MidiExportStatus::ok);

                if (live.size() != 2 || out.events.size() != 2)
                {
                    expect (false, "gate " + juce::String (gate) + " must give one note-on and one note-off in both domains");
                    continue;
                }

                const long long samples = live[1].sampleOffset - live[0].sampleOffset;
                const long long ticks   = out.events[1].tick - out.events[0].tick;

                expectEquals ((int) samples, 240 * gate);
                expectEquals ((int) ticks, (int) std::llround (samples * 240.0 / 24000.0));
            }
        }
    }
};

static MidiExportTimelineTests midiExportTimelineTests;
