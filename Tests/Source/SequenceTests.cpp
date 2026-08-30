/*
  ==============================================================================

   Sequence tests (sequencing-core spec). RED first: Source/core/Sequence.h/.cpp
   do not exist yet, so this suite fails to compile until 2.2 creates them.
   Covers construction size/default steps, resize grow (padding) and shrink
   (truncation), and indexed read/write access.

  ==============================================================================
*/

#include <juce_core/juce_core.h>

#include "core/Sequence.h"

class SequenceTests final : public juce::UnitTest
{
public:
    SequenceTests() : juce::UnitTest ("Sequence", "Berlin") {}

    void runTest() override
    {
        beginTest ("construct with numSteps - size and default-constructed steps");
        {
            const berlin::Sequence seq (4);

            expectEquals (seq.size(), 4);

            for (int i = 0; i < seq.size(); ++i)
                expect (seq[i] == berlin::Step {});
        }

        beginTest ("default construction - empty sequence");
        {
            const berlin::Sequence seq;

            expectEquals (seq.size(), 0);
        }

        beginTest ("resize grows - existing steps preserved, new steps default");
        {
            berlin::Sequence seq (4);
            seq[1] = berlin::Step { 60, true };

            seq.resize (16);

            expectEquals (seq.size(), 16);
            expect (seq[1] == berlin::Step { 60, true });

            for (int i = 4; i < seq.size(); ++i)
                expect (seq[i] == berlin::Step {});
        }

        beginTest ("resize shrinks - truncates to new size");
        {
            berlin::Sequence seq (16);
            seq[15] = berlin::Step { 72, true };

            seq.resize (4);

            expectEquals (seq.size(), 4);
        }

        beginTest ("indexed access - write affects only that index");
        {
            berlin::Sequence seq (4);
            seq[2] = berlin::Step { 60, true };

            expect (seq[0] == berlin::Step {});
            expect (seq[1] == berlin::Step {});
            expect (seq[2] == berlin::Step { 60, true });
            expect (seq[3] == berlin::Step {});
        }
    }
};

static SequenceTests sequenceTests;
