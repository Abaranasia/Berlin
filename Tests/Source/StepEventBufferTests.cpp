/*
  ==============================================================================

   StepEventBuffer tests (playback-transport-clock spec, Phase 2). RED first:
   Source/playback/StepEventBuffer.h does not exist yet, so this suite must
   fail to compile until 2.2 creates it. Covers push-within-capacity ordering,
   drop-and-flag behaviour at the fixed capacity of 64 (leaving already-stored
   events unchanged and latching hasOverflowed()), and clear() resetting both
   size() and hasOverflowed().

  ==============================================================================
*/

#include <juce_core/juce_core.h>

#include "playback/StepEventBuffer.h"

class StepEventBufferTests final : public juce::UnitTest
{
public:
    StepEventBufferTests() : juce::UnitTest ("StepEventBuffer", "Berlin") {}

    void runTest() override
    {
        beginTest ("push within capacity retains order");
        {
            berlin::StepEventBuffer buffer;

            expect (buffer.push ({ 0, 0, 60, true }));
            expect (buffer.push ({ 4, 1, 62, true }));
            expect (buffer.push ({ 8, 1, 62, false }));

            expectEquals (buffer.size(), 3);
            expect (buffer[0] == berlin::StepEvent { 0, 0, 60, true });
            expect (buffer[1] == berlin::StepEvent { 4, 1, 62, true });
            expect (buffer[2] == berlin::StepEvent { 8, 1, 62, false });
            expect (! buffer.hasOverflowed());
        }

        beginTest ("push at full capacity drops the excess event, leaves stored events unchanged, and latches hasOverflowed()");
        {
            berlin::StepEventBuffer buffer;

            for (int i = 0; i < berlin::StepEventBuffer::capacity; ++i)
                expect (buffer.push ({ i, i, i, true }));

            expectEquals (buffer.size(), berlin::StepEventBuffer::capacity);
            expect (! buffer.hasOverflowed());

            const bool accepted = buffer.push ({ 999, 999, 999, false });

            expect (! accepted);
            expectEquals (buffer.size(), berlin::StepEventBuffer::capacity);
            expect (buffer.hasOverflowed());

            for (int i = 0; i < berlin::StepEventBuffer::capacity; ++i)
                expect (buffer[i] == berlin::StepEvent { i, i, i, true });
        }

        beginTest ("clear() resets size() to 0 and hasOverflowed() to false");
        {
            berlin::StepEventBuffer buffer;

            for (int i = 0; i < berlin::StepEventBuffer::capacity + 1; ++i)
                buffer.push ({ i, i, i, true });

            expect (buffer.hasOverflowed());
            expectEquals (buffer.size(), berlin::StepEventBuffer::capacity);

            buffer.clear();

            expectEquals (buffer.size(), 0);
            expect (! buffer.hasOverflowed());
        }
    }
};

static StepEventBufferTests stepEventBufferTests;
