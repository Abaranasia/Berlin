/*
  ==============================================================================

   Playback timing tests (playback-transport-clock spec, Phase 5). This is
   the falsifiable proof behind the proposal's "Generated sequence plays
   correctly" deliverable. No RED/GREEN split is possible here - same
   situation as core-sequencing-model's Phase 7 Reproducibility suite: this
   suite composes already-green Transport and SequencePlayer (Phases 3-4)
   with no new header to be missing, so there is no compile-time failure to
   prove first. If any assertion below fails, the bug is in Transport.cpp or
   SequencePlayer.cpp, not a missing type - fix there, do not add a new file
   to satisfy this phase.

   Drives SequencePlayer::process across many simulated audio callbacks with
   irregular, varying block sizes (including some shorter than one step),
   summing block lengths into an absolute running sample position and
   rebasing each emitted StepEvent's sampleOffset against it (sampleOffset is
   relative to the START of the block that produced it, per StepEvent.h's own
   documented contract - not to any prior block). Each boundary k's rebased
   absolute timestamp is checked against llround(k * samplesPerStep) - the
   same drift-free absolute-position formula Transport.cpp uses internally
   (Transport.h/.cpp, design.md's drift decision).

   Two of the three suites below use an ALL-ACTIVE Sequence so every boundary
   produces a fixed, predictable event count (1 event at k=0 - no pending
   note-off yet; 2 events, note-off then note-on, for every k>=1) - this makes
   correlating "which emitted event belongs to which boundary k" exact and
   mechanical, letting the test assert on the absolute-position arithmetic in
   isolation. The third suite uses a mixed active/inactive Sequence to prove
   the full end-to-end composition (including step gating and the loop wrap)
   still produces correctly-timed, correctly-ordered events under irregular
   blocks - this is the scenario closest to MainComponent's real usage.

   Per Phase 3's actual TransportTests result (tasks.md 3.3's Confirmed note:
   "landed exactly on llround(k * samplesPerStep) with zero error observed"),
   the assertions below check for EXACT equality, not merely the spec's
   stated +/-1 sample tolerance - this is a stronger, falsifiable claim than
   the spec strictly requires, consistent with what Phase 3 actually
   observed for the same absolute-position formula.

  ==============================================================================
*/

#include <juce_core/juce_core.h>

#include <cmath>
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

    struct TimedEvent
    {
        long long absoluteSample = 0;
        int       stepIndex      = 0;
        int       note           = 0;
        bool      isNoteOn       = false;
    };
}

class PlaybackTimingTests final : public juce::UnitTest
{
public:
    PlaybackTimingTests() : juce::UnitTest ("PlaybackTiming", "Berlin") {}

    void runTest() override
    {
        beginTest ("absolute event timestamps land exactly on round(k * samplesPerStep) across irregular "
                   "block sizes, including sub-step blocks, spanning multiple full loop wraps");
        {
            // N = 5, all active: every boundary k=0 emits exactly 1 event
            // (note-on, no pending note-off yet); every boundary k>=1 emits
            // exactly 2 (note-off then note-on) - this fixed, predictable
            // event count per boundary is what makes correlating emitted
            // events to boundary index k exact and mechanical below.
            auto sequence = makeSequence ({ { 60, true }, { 62, true }, { 64, true }, { 65, true }, { 67, true } });
            berlin::SequencePlayer player (sequence, berlin::Transport (120.0, 4));
            player.prepare (44100.0);   // samplesPerStep == 5512.5 (matches TransportTests' own precedent value)
            player.start();

            constexpr double samplesPerStep = 5512.5;
            const std::vector<int> blockSizes { 97, 251, 512, 1000, 33, 5512, 1, 8000 };   // several shorter than one step
            constexpr long long numBoundariesWanted = 30;   // N=5 -> spans 6 full loop wraps

            long long globalBoundary   = 0;
            long long absolutePosition = 0;
            berlin::StepEventBuffer buffer;

            while (globalBoundary < numBoundariesWanted)
            {
                for (const auto blockSize : blockSizes)
                {
                    player.process (blockSize, buffer);
                    expect (! buffer.hasOverflowed());

                    int eventIndex = 0;

                    while (eventIndex < buffer.size())
                    {
                        const long long expectedAbsolute = std::llround (static_cast<double> (globalBoundary) * samplesPerStep);

                        if (globalBoundary == 0)
                        {
                            const auto& event = buffer[eventIndex];
                            expect (event.isNoteOn, "boundary 0 must be a note-on (no pending note-off yet)");
                            expectEquals (absolutePosition + event.sampleOffset, expectedAbsolute);
                            eventIndex += 1;
                        }
                        else
                        {
                            const auto& off = buffer[eventIndex];
                            const auto& on  = buffer[eventIndex + 1];

                            expect (! off.isNoteOn, "expected note-off first at boundary " + juce::String (globalBoundary));
                            expect (on.isNoteOn, "expected note-on second at boundary " + juce::String (globalBoundary));
                            expectEquals (absolutePosition + off.sampleOffset, expectedAbsolute);
                            expectEquals (absolutePosition + on.sampleOffset, expectedAbsolute);
                            eventIndex += 2;
                        }

                        ++globalBoundary;
                    }

                    absolutePosition += blockSize;

                    if (globalBoundary >= numBoundariesWanted)
                        break;
                }
            }

            expect (globalBoundary >= numBoundariesWanted);
            expect (globalBoundary > static_cast<long long> (sequence.size()) * 2,
                    "must span at least one full loop wrap");
        }

        beginTest ("cumulative timing error over a thousands-of-steps, mixed-block-size run stays exactly "
                   "zero (structurally bounded by the absolute-position design, not merely small)");
        {
            // N = 7, all active, for the same fixed-event-count-per-boundary
            // reasoning as the suite above, driven far enough (5000
            // boundaries) to be a genuine long-run drift check, not just a
            // handful of steps - mirrors TransportTests' own long-run block
            // set applied at the SequencePlayer composition level.
            auto sequence = makeSequence ({ { 60, true }, { 62, true }, { 64, true }, { 65, true },
                                             { 67, true }, { 69, true }, { 71, true } });
            berlin::SequencePlayer player (sequence, berlin::Transport (120.0, 4));
            player.prepare (44100.0);   // samplesPerStep == 5512.5
            player.start();

            constexpr double samplesPerStep = 5512.5;
            const std::vector<int> blockSizes { 512, 1024, 2000, 4096, 700 };
            constexpr long long numBoundariesWanted = 5000;

            long long globalBoundary   = 0;
            long long absolutePosition = 0;
            long long maxAbsError      = 0;
            berlin::StepEventBuffer buffer;

            while (globalBoundary < numBoundariesWanted)
            {
                for (const auto blockSize : blockSizes)
                {
                    player.process (blockSize, buffer);
                    expect (! buffer.hasOverflowed());

                    int eventIndex = 0;

                    while (eventIndex < buffer.size())
                    {
                        const long long expectedAbsolute   = std::llround (static_cast<double> (globalBoundary) * samplesPerStep);
                        const int       eventsThisBoundary = (globalBoundary == 0) ? 1 : 2;

                        for (int e = 0; e < eventsThisBoundary; ++e)
                        {
                            const auto& event          = buffer[eventIndex + e];
                            const long long actual     = absolutePosition + event.sampleOffset;
                            const long long error      = actual - expectedAbsolute;
                            const long long absError   = error < 0 ? -error : error;

                            if (absError > maxAbsError)
                                maxAbsError = absError;
                        }

                        eventIndex += eventsThisBoundary;
                        ++globalBoundary;
                    }

                    absolutePosition += blockSize;

                    if (globalBoundary >= numBoundariesWanted)
                        break;
                }
            }

            expect (globalBoundary >= numBoundariesWanted);
            expectEquals (maxAbsError, (long long) 0);
        }

        beginTest ("mixed active/inactive sequence emits correctly-timed, correctly-ordered note-on/note-off "
                   "events across the loop wrap under irregular block sizes");
        {
            // N = 4, one inactive step (index 1) - the full end-to-end
            // composition under test: step gating (Requirement "Inactive
            // Steps and Empty Sequence Emit Nothing"), note-off-before-
            // note-on ordering at shared offsets, and the loop wrap
            // (Requirement "Continuous Emission Across the Loop Wrap"),
            // all under irregular, varying, sub-step block sizes - not just
            // the clean all-active case above.
            auto sequence = makeSequence ({ { 60, true }, { 0, false }, { 64, true }, { 67, true } });
            berlin::SequencePlayer player (sequence, berlin::Transport (60.0, 1));
            player.prepare (4.0);   // samplesPerStep == 4.0 exactly - exact expected timestamps below
            player.start();

            const std::vector<int> blockSizes { 1, 2, 1, 3, 2, 4, 1, 5, 3, 2, 6, 4, 3, 2, 1 };   // sums to 40
            long long absolutePosition = 0;
            berlin::StepEventBuffer buffer;
            std::vector<TimedEvent> observed;

            for (const auto blockSize : blockSizes)
            {
                player.process (blockSize, buffer);
                expect (! buffer.hasOverflowed());

                for (int i = 0; i < buffer.size(); ++i)
                    observed.push_back ({ absolutePosition + buffer[i].sampleOffset,
                                           buffer[i].stepIndex, buffer[i].note, buffer[i].isNoteOn });

                absolutePosition += blockSize;
            }

            // Hand-derived from the sequence above against Transport's
            // documented boundary formula (samplesPerStep == 4.0, so
            // boundary k lands at exactly 4*k): step 1 is inactive, so its
            // boundary contributes only the PRIOR step's note-off, no
            // note-on. Spans k=0..9, i.e. two full loop wraps (at k=4, k=8).
            const std::vector<TimedEvent> expected {
                {  0, 0, 60, true  },
                {  4, 0, 60, false },
                {  8, 2, 64, true  },
                { 12, 2, 64, false },
                { 12, 3, 67, true  },
                { 16, 3, 67, false },
                { 16, 0, 60, true  },
                { 20, 0, 60, false },
                { 24, 2, 64, true  },
                { 28, 2, 64, false },
                { 28, 3, 67, true  },
                { 32, 3, 67, false },
                { 32, 0, 60, true  },
                { 36, 0, 60, false },
            };

            expectEquals (static_cast<int> (observed.size()), static_cast<int> (expected.size()));

            const int n = juce::jmin (static_cast<int> (observed.size()), static_cast<int> (expected.size()));

            for (int i = 0; i < n; ++i)
            {
                const auto& o = observed[static_cast<size_t> (i)];
                const auto& x = expected[static_cast<size_t> (i)];

                expectEquals (o.absoluteSample, x.absoluteSample);
                expectEquals (o.stepIndex, x.stepIndex);
                expectEquals (o.note, x.note);
                expect (o.isNoteOn == x.isNoteOn, "isNoteOn mismatch at event " + juce::String (i));
            }
        }
    }
};

static PlaybackTimingTests playbackTimingTests;
