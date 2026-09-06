/*
  ==============================================================================

   Reproducibility tests (deterministic-generation spec). End-to-end.

   The RhythmGenerator + PitchGenerator composition below was test-local only
   at the time this suite was first written (RhythmGenerator had no
   production call site). That is no longer true project-wide: since
   roadmap Phase 10 (generation-randomize), MainComponent::buildSeededSequence
   composes SkipMaskGenerator + PitchGenerator in production, driven by the
   Generate/Randomize UI (generation-live-control spec, design.md Decision
   1/2). RhythmGenerator itself, however, still has no production call site -
   see the separate SkipMaskGenerator + PitchGenerator golden further below,
   which exercises the actual production composition.

   Builds a 16-step Sequence from a root note and Scale::minor(root), seed
   12345: RhythmGenerator.generate() first decides which steps are active
   (one nextFloat() draw per step, ascending order), then PitchGenerator
   .generateNextNote() fills in a note for each active step only (one
   nextInt() draw per call, or zero if the fallback path is taken) - both
   generators draw from the SAME DeterministicRandom instance, in that order,
   per design.md's data-flow diagram. Doing this twice with two independently
   constructed DeterministicRandom(12345) instances must yield two identical
   Sequence values, since juce::Random's draw sequence is a pure function of
   its seed and call order, and both generators' draw counts are independent
   of the values drawn (RhythmGenerator: exactly one draw per step regardless
   of density; PitchGenerator: exactly one draw per active step unless the
   ctor-time fallback is used, which is itself a fixed property of the scale
   and range, not of any draw).

  ==============================================================================
*/

#include <juce_core/juce_core.h>

#include "core/Scale.h"
#include "core/Sequence.h"
#include "generation/DeterministicRandom.h"
#include "generation/PitchGenerator.h"
#include "generation/RhythmGenerator.h"
#include "generation/SkipMaskGenerator.h"

namespace
{

berlin::Sequence generateFullSequence (int numSteps, int rootNote, berlin::DeterministicRandom& random)
{
    const berlin::Scale scale = berlin::Scale::minor (rootNote);

    const berlin::RhythmGenerator rhythmGenerator (numSteps, 0.5f);
    const berlin::PitchGenerator pitchGenerator (scale, rootNote, rootNote + 24);

    // RhythmGenerator draws first (one nextFloat() per step, ascending
    // order), then PitchGenerator draws for each active step only (ascending
    // order) - this fixed composition order IS the reproducibility contract
    // exercised by this test.
    berlin::Sequence sequence = rhythmGenerator.generate (random);

    for (int i = 0; i < sequence.size(); ++i)
        if (sequence[i].active)
            sequence[i].note = pitchGenerator.generateNextNote (random);

    return sequence;
}

// Same composition order as generateFullSequence above, but with
// SkipMaskGenerator (the live app's Generate/Randomize rhythm source,
// generation-live-control) in place of RhythmGenerator - the
// RhythmGenerator golden above is untouched by this addition.
berlin::Sequence generateFullSkipMaskSequence (int numSteps, int activeSteps, int rootNote, berlin::DeterministicRandom& random)
{
    const berlin::Scale scale = berlin::Scale::minor (rootNote);

    const berlin::SkipMaskGenerator maskGenerator (numSteps, activeSteps);
    const berlin::PitchGenerator pitchGenerator (scale, rootNote, rootNote + 24);

    berlin::Sequence sequence = maskGenerator.generate (random);

    for (int i = 0; i < sequence.size(); ++i)
        if (sequence[i].active)
            sequence[i].note = pitchGenerator.generateNextNote (random);

    return sequence;
}

} // namespace

class ReproducibilityTests final : public juce::UnitTest
{
public:
    ReproducibilityTests() : juce::UnitTest ("Reproducibility", "Berlin") {}

    void runTest() override
    {
        beginTest ("same seed produces an identical 16-step Sequence end-to-end");
        {
            constexpr int numSteps = 16;
            constexpr int rootNote = 60;
            constexpr juce::int64 seed = 12345;

            berlin::DeterministicRandom randomA (seed);
            berlin::DeterministicRandom randomB (seed);

            const berlin::Sequence sequenceA = generateFullSequence (numSteps, rootNote, randomA);
            const berlin::Sequence sequenceB = generateFullSequence (numSteps, rootNote, randomB);

            expect (sequenceA == sequenceB);
        }

        beginTest ("different seeds are permitted to produce a different Sequence");
        {
            constexpr int numSteps = 16;
            constexpr int rootNote = 60;

            berlin::DeterministicRandom randomA (12345);
            berlin::DeterministicRandom randomB (54321);

            const berlin::Sequence sequenceA = generateFullSequence (numSteps, rootNote, randomA);
            const berlin::Sequence sequenceB = generateFullSequence (numSteps, rootNote, randomB);

            // Not a contract that they MUST differ, but asserting inequality
            // here (rather than equality) would be a false claim; instead,
            // confirm the comparison itself is well-formed and at least one
            // seed's Sequence is internally consistent (size matches).
            expectEquals (sequenceA.size(), numSteps);
            expectEquals (sequenceB.size(), numSteps);
        }

        beginTest ("SkipMaskGenerator + PitchGenerator: same seed produces an identical 16-step Sequence end-to-end");
        {
            constexpr int numSteps = 16;
            constexpr int activeSteps = 11;
            constexpr int rootNote = 60;
            constexpr juce::int64 seed = 12345;

            berlin::DeterministicRandom randomA (seed);
            berlin::DeterministicRandom randomB (seed);

            const berlin::Sequence sequenceA = generateFullSkipMaskSequence (numSteps, activeSteps, rootNote, randomA);
            const berlin::Sequence sequenceB = generateFullSkipMaskSequence (numSteps, activeSteps, rootNote, randomB);

            expect (sequenceA == sequenceB);
        }
    }
};

static ReproducibilityTests reproducibilityTests;
