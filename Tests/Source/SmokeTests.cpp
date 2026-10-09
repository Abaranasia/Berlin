/*
  ==============================================================================

   Phase 0 harness smoke test (unit-test-harness spec). Proves the BerlinTests
   runner discovers, runs, and reports juce::UnitTest suites correctly before
   any domain code exists. RED first (deliberate failure, exit code 1), then
   flipped to GREEN (exit code 0) once the red proof is confirmed.

  ==============================================================================
*/

#include <juce_core/juce_core.h>

class SmokeTests final : public juce::UnitTest
{
public:
    SmokeTests() : juce::UnitTest ("Smoke", "Berlin") {}

    void runTest() override
    {
        beginTest ("harness placeholder assertion");
        expect (true, "harness green proof - flip back to false to reproduce the RED case");
    }
};

static SmokeTests smokeTests;
