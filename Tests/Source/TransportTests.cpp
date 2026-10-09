/*
  ==============================================================================

   Transport tests (playback-transport-clock spec, Phase 3). RED first:
   Source/playback/Transport.h does not exist yet, so this suite must fail to
   compile until 3.2 creates it. Covers samples-per-step derivation, the
   query/commit boundary protocol across irregular block sizes, drift-free
   long-run accuracy, degenerate (bpm<=0, stepsPerBeat<=0, unprepared,
   stopped) inputs, and unbounded boundary counting (Transport has no
   loop-wrap/playhead concept - that is SequencePlayer's job in Phase 4, not
   tested here).

   REVIEW-ONLY CHECKLIST (not a runtime UnitTest assertion, per spec.md's
   "No runtime BPM mutation API" requirement and tasks.md 3.1): inspect
   Transport's public interface (Source/playback/Transport.h, once created)
   and confirm no setter/method exists that mutates bpm after construction.
   Verified by code review at task 3.3, not by a compile/runtime assertion -
   mirrors the non-runtime-testable pattern already used for the audio-
   callback allocation check (design.md's Review-only gates).

  ==============================================================================
*/

#include <juce_core/juce_core.h>

#include <cmath>
#include <vector>

#include "playback/Transport.h"

namespace
{
    // Drives `transport` over `blockSizes` (cycled until `numBoundariesWanted`
    // boundaries have been observed), asserting each boundary's absolute
    // sample position lands within +/-1 of round(k * samplesPerStep). Mirrors
    // SequencePlayer's documented process() ordering: query, then commit.
    void driveAndCheckBoundaries (juce::UnitTest& test, berlin::Transport& transport,
                                  const std::vector<int>& blockSizes, long long numBoundariesWanted)
    {
        long long boundariesSeen = 0;

        while (boundariesSeen < numBoundariesWanted)
        {
            for (const auto blockSize : blockSizes)
            {
                const int n = transport.countBoundaries (blockSize);

                for (int i = 0; i < n; ++i)
                {
                    const auto boundary = transport.getBoundary (i);
                    const long long expected = std::llround (static_cast<double> (boundary.stepCounter)
                                                              * transport.getSamplesPerStep());
                    const long long actualAbsolute = transport.getSamplePosition() + boundary.sampleOffset;
                    const long long error = actualAbsolute - expected;
                    const long long absError = error < 0 ? -error : error;

                    test.expect (absError <= 1,
                                 "boundary " + juce::String (boundary.stepCounter)
                                     + " drifted by " + juce::String (error));
                    ++boundariesSeen;
                }

                transport.advance (blockSize);

                if (boundariesSeen >= numBoundariesWanted)
                    break;
            }
        }
    }
}

class TransportTests final : public juce::UnitTest
{
public:
    TransportTests() : juce::UnitTest ("Transport", "Berlin") {}

    void runTest() override
    {
        beginTest ("default configuration (bpm=120, stepsPerBeat=4) prepares to the documented samplesPerStep");
        {
            // Transport exposes no direct bpm getter (design.md's interface
            // has none - only the effect of bpm is observable), so the
            // documented default bpm=120 is verified indirectly via
            // prepare()'s effect on samplesPerStep.
            berlin::Transport transport (120.0, 4);
            transport.prepare (44100.0);

            expectEquals (transport.getSamplesPerStep(), 5512.5);
        }

        beginTest ("samplesPerStep == 5512.5 at {sampleRate=44100, bpm=120, stepsPerBeat=4}");
        {
            berlin::Transport transport (120.0, 4);
            transport.prepare (44100.0);

            expectEquals (transport.getSamplesPerStep(), 5512.5);
            expect (transport.isPrepared());
        }

        beginTest ("bpm <= 0 never divides by zero or crashes");
        {
            berlin::Transport zero (0.0, 4);
            zero.prepare (44100.0);
            expect (! zero.isPrepared());

            berlin::Transport negative (-10.0, 4);
            negative.prepare (44100.0);
            expect (! negative.isPrepared());

            negative.start();
            expectEquals (negative.countBoundaries (100000), 0);
            negative.advance (100000);   // must not crash
            expectEquals (negative.countBoundaries (100000), 0);
        }

        beginTest ("stepsPerBeat <= 0 never divides by zero or crashes");
        {
            berlin::Transport transport (120.0, 0);
            transport.prepare (44100.0);
            expect (! transport.isPrepared());

            transport.start();
            transport.advance (100000);   // must not crash
            expectEquals (transport.countBoundaries (100000), 0);
        }

        beginTest ("advance() reports no boundary when block is shorter than samplesPerStep");
        {
            berlin::Transport transport (120.0, 4);
            transport.prepare (44100.0);   // samplesPerStep = 5512.5
            transport.start();

            // Boundary 0 fires at absolute sample 0 (the first step begins
            // immediately at playback start) - consume it first so this
            // scenario tests the NEXT boundary (k=1, at sample 5512.5), not
            // the always-immediate boundary 0.
            expectEquals (transport.countBoundaries (1), 1);
            transport.advance (1);

            expectEquals (transport.countBoundaries (100), 0);
            transport.advance (100);
            expectEquals (transport.countBoundaries (100), 0);
        }

        beginTest ("irregular advance() calls land each boundary within +/-1 of round(k * samplesPerStep)");
        {
            berlin::Transport transport (120.0, 4);
            transport.prepare (44100.0);
            transport.start();

            driveAndCheckBoundaries (*this, transport, { 97, 251, 512, 1000, 33, 5512, 1, 8000 }, 200);
        }

        beginTest ("long-run (thousands-of-steps) drift check stays within +/-1 of ideal across the whole run");
        {
            berlin::Transport transport (120.0, 4);
            transport.prepare (44100.0);
            transport.start();

            driveAndCheckBoundaries (*this, transport, { 512, 1024, 2000, 4096, 700 }, 5000);
        }

        beginTest ("stopped transport reports zero boundaries");
        {
            berlin::Transport transport (120.0, 4);
            transport.prepare (44100.0);
            // never started - default running state is stopped

            expectEquals (transport.countBoundaries (100000), 0);
            transport.advance (100000);
            expectEquals (transport.getSamplePosition(), (long long) 0);   // advance is a no-op when stopped

            transport.start();
            transport.advance (10000);
            transport.stop();

            expectEquals (transport.countBoundaries (100000), 0);
        }

        beginTest ("unprepared transport (prepare never called) is safe and reports zero boundaries");
        {
            berlin::Transport transport (120.0, 4);
            // prepare() never called

            expect (! transport.isPrepared());
            expectEquals (transport.countBoundaries (100000), 0);

            transport.start();
            expectEquals (transport.countBoundaries (100000), 0);
            transport.advance (100000);   // must not crash
            expectEquals (transport.countBoundaries (100000), 0);
        }

        beginTest ("boundary counting keeps increasing indefinitely across many blocks, with no wraparound");
        {
            berlin::Transport transport (120.0, 4);
            transport.prepare (44100.0);
            transport.start();

            long long totalBoundaries = 0;

            for (int block = 0; block < 3000; ++block)
            {
                const int n = transport.countBoundaries (4096);
                totalBoundaries += n;
                transport.advance (4096);
            }

            // Sequence-length-agnostic: Transport counts absolute boundaries
            // only. If it wrapped at any fixed count (e.g. 16, matching a
            // typical loop length), totalBoundaries would be far smaller.
            // SequencePlayer (Phase 4), not Transport, owns the wrap.
            expect (totalBoundaries > 2000);
            expectEquals (transport.getNextStepCounter(), totalBoundaries);
        }
    }
};

static TransportTests transportTests;
