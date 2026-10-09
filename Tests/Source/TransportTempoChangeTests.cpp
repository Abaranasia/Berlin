/*
  ==============================================================================

   Transport tempo-change tests (playback-transport spec, Phase 2, CRITICAL).
   RED first: Transport.h exposes no setBpm()/getBpm() yet, so this suite
   must fail to compile until Phase 2.2 adds them. This is the design.md
   Decision 2/3 origin-rebase mechanism: a phase-preserving rebase of the
   boundary grid, never a naive rate substitution that would retime already-
   reported boundaries. Existing TransportTests.cpp drift suites are the
   regression gate (task 2.3) and are NOT modified here.

  ==============================================================================
*/

#include <juce_core/juce_core.h>

#include <cmath>
#include <vector>

#include "playback/Transport.h"

namespace
{
    // Drains every boundary in [position, position+numSamples) via the
    // query/commit protocol, returning their absolute sample positions in
    // emission order. Does NOT call setBpm - callers do that between drains.
    std::vector<long long> drainBoundaries (berlin::Transport& transport, int numSamples)
    {
        std::vector<long long> absolutePositions;
        const int n = transport.countBoundaries (numSamples);

        for (int i = 0; i < n; ++i)
        {
            const auto boundary = transport.getBoundary (i);
            absolutePositions.push_back (transport.getSamplePosition() + boundary.sampleOffset);
        }

        transport.advance (numSamples);
        return absolutePositions;
    }
}

class TransportTempoChangeTests final : public juce::UnitTest
{
public:
    TransportTempoChangeTests() : juce::UnitTest ("Transport.setBpm", "Berlin") {}

    void runTest() override
    {
        beginTest ("getBpm() reflects the constructor value until setBpm is called");
        {
            berlin::Transport transport (120.0, 4);
            expectEquals (transport.getBpm(), 120.0);
        }

        beginTest ("samplesPerStep == sampleRate*60/(bpm*stepsPerBeat) immediately after every setBpm");
        {
            berlin::Transport transport (120.0, 4);
            transport.prepare (44100.0);
            transport.start();

            for (const double newBpm : { 90.0, 60.0, 200.0, 40.0, 240.0 })
            {
                transport.setBpm (newBpm);
                expectEquals (transport.getBpm(), newBpm);
                expectWithinAbsoluteError (transport.getSamplesPerStep(),
                                           44100.0 * 60.0 / (newBpm * 4.0), 1.0e-9);
            }
        }

        beginTest ("mid-run setBpm: next boundary is >= position (never in the past, no negative sampleOffset)");
        {
            berlin::Transport transport (120.0, 4);
            transport.prepare (44100.0);   // samplesPerStep = 5512.5
            transport.start();

            // Advance partway into the run so a boundary has already been
            // emitted and the transport sits mid-step, not freshly reset.
            drainBoundaries (transport, 1);       // consumes k=0 at offset 0
            drainBoundaries (transport, 2000);    // partway through step 1, no boundary yet

            const long long positionBeforeChange = transport.getSamplePosition();
            const long long stepCounterBeforeChange = transport.getNextStepCounter();

            transport.setBpm (200.0);   // tempo rises mid-step

            expectEquals (transport.getNextStepCounter(), stepCounterBeforeChange);   // setBpm never mutates step counter

            const auto boundaries = drainBoundaries (transport, 20000);
            expect (! boundaries.empty(), "expected at least one boundary after the tempo change");

            for (size_t i = 0; i < boundaries.size(); ++i)
            {
                expect (boundaries[i] >= positionBeforeChange,
                        "boundary landed in the past: " + juce::String (boundaries[i])
                            + " < " + juce::String (positionBeforeChange));

                if (i > 0)
                    expect (boundaries[i] > boundaries[i - 1], "boundaries must be strictly increasing (no double)");
            }
        }

        beginTest ("post-change boundaries stay within +/-1 sample of the rebased ideal grid");
        {
            berlin::Transport transport (120.0, 4);
            transport.prepare (44100.0);
            transport.start();

            drainBoundaries (transport, 3000);   // run a bit before changing tempo

            transport.setBpm (150.0);

            const double samplesPerStepAfter = transport.getSamplesPerStep();
            std::vector<long long> boundaries;

            for (const int blockSize : { 97, 251, 512, 1000, 33, 700, 8000 })
            {
                for (auto pos : drainBoundaries (transport, blockSize))
                    boundaries.push_back (pos);
            }

            // The grid is now anchored at the rebase point: consecutive
            // boundary spacing must track samplesPerStepAfter within +/-1
            // sample (design.md's documented drift bound), even though the
            // origin itself is unobservable directly.
            for (size_t i = 1; i < boundaries.size(); ++i)
            {
                const double spacing = static_cast<double> (boundaries[i] - boundaries[i - 1]);
                const double error = spacing - samplesPerStepAfter;
                expect (std::abs (error) <= 1.0,
                        "spacing drifted by " + juce::String (error) + " at boundary " + juce::String ((int) i));
            }
        }

        beginTest ("500 successive setBpm calls (slider-drag simulation) accumulate no drift");
        {
            berlin::Transport transport (120.0, 4);
            transport.prepare (44100.0);
            transport.start();

            juce::Random rng (42);

            for (int i = 0; i < 500; ++i)
            {
                const double newBpm = 40.0 + rng.nextDouble() * 200.0;   // [40, 240)
                transport.setBpm (newBpm);
                drainBoundaries (transport, 512);   // let a bit of time pass between changes
            }

            // Drive a further run and confirm the grid is still self-consistent:
            // consecutive spacing tracks the FINAL samplesPerStep within +/-1.
            const double finalSamplesPerStep = transport.getSamplesPerStep();
            std::vector<long long> boundaries;

            for (int block = 0; block < 50; ++block)
                for (auto pos : drainBoundaries (transport, 512))
                    boundaries.push_back (pos);

            for (size_t i = 1; i < boundaries.size(); ++i)
            {
                const double spacing = static_cast<double> (boundaries[i] - boundaries[i - 1]);
                expect (std::abs (spacing - finalSamplesPerStep) <= 1.0,
                        "drift accumulated after 500 setBpm calls");
            }
        }

        beginTest ("setBpm before prepare() is safe (no crash, no divide-by-zero)");
        {
            berlin::Transport transport (120.0, 4);
            transport.setBpm (90.0);   // prepare() never called
            expectEquals (transport.getBpm(), 90.0);
            expect (! transport.isPrepared());

            transport.prepare (44100.0);
            expectWithinAbsoluteError (transport.getSamplesPerStep(), 44100.0 * 60.0 / (90.0 * 4.0), 1.0e-9);
        }

        beginTest ("setBpm(0) / setBpm(negative) is a safe no-op, never divides by zero");
        {
            berlin::Transport transport (120.0, 4);
            transport.prepare (44100.0);
            transport.start();

            const double before = transport.getSamplesPerStep();
            transport.setBpm (0.0);
            expectEquals (transport.getBpm(), 120.0);
            expectEquals (transport.getSamplesPerStep(), before);

            transport.setBpm (-50.0);
            expectEquals (transport.getBpm(), 120.0);
            expectEquals (transport.getSamplesPerStep(), before);

            transport.countBoundaries (100000);   // must not crash
            transport.advance (100000);
        }

        beginTest ("setBpm while stopped is safe and takes effect once restarted");
        {
            berlin::Transport transport (120.0, 4);
            transport.prepare (44100.0);
            transport.start();
            drainBoundaries (transport, 3000);
            transport.stop();

            transport.setBpm (180.0);   // while stopped
            expectEquals (transport.getBpm(), 180.0);

            transport.start();
            expectWithinAbsoluteError (transport.getSamplesPerStep(), 44100.0 * 60.0 / (180.0 * 4.0), 1.0e-9);

            const auto boundaries = drainBoundaries (transport, 20000);
            expect (! boundaries.empty());
        }

        beginTest ("setBpm at nextStepCounter == 0 (nothing emitted yet) is safe, no skip");
        {
            berlin::Transport transport (120.0, 4);
            transport.prepare (44100.0);
            transport.start();

            transport.setBpm (200.0);   // before any boundary has ever been drained

            const auto boundaries = drainBoundaries (transport, 20000);
            expect (! boundaries.empty());
            expect (boundaries.front() >= 0, "first post-change boundary must not be negative");
        }

        beginTest ("setBpm(newBpm == current bpm) is a cheap no-op (early return, no origin churn)");
        {
            berlin::Transport transport (120.0, 4);
            transport.prepare (44100.0);
            transport.start();
            drainBoundaries (transport, 3000);

            const double before = transport.getSamplesPerStep();
            transport.setBpm (120.0);   // same value
            expectEquals (transport.getSamplesPerStep(), before);
        }

        beginTest ("REGRESSION: existing driveAndCheckBoundaries-style drift stays exact when bpm never changes");
        {
            // Mirrors TransportTests.cpp's own suite - proves the origin
            // refactor is bit-identical (originSample=0, originStep=0
            // defaults) when setBpm is never called.
            berlin::Transport transport (120.0, 4);
            transport.prepare (44100.0);
            transport.start();

            long long boundariesSeen = 0;
            const std::vector<int> blockSizes { 97, 251, 512, 1000, 33, 5512, 1, 8000 };

            while (boundariesSeen < 200)
            {
                for (const auto blockSize : blockSizes)
                {
                    const int n = transport.countBoundaries (blockSize);

                    for (int i = 0; i < n; ++i)
                    {
                        const auto boundary = transport.getBoundary (i);
                        const long long expected = std::llround (static_cast<double> (boundary.stepCounter)
                                                                  * transport.getSamplesPerStep());
                        const long long actual = transport.getSamplePosition() + boundary.sampleOffset;
                        expectEquals (actual, expected);
                        ++boundariesSeen;
                    }

                    transport.advance (blockSize);

                    if (boundariesSeen >= 200)
                        break;
                }
            }
        }
    }
};

static TransportTempoChangeTests transportTempoChangeTests;
