/*
  ==============================================================================

   Reproducibility tests (deterministic-generation spec). End-to-end.

   The RhythmGenerator + PitchGenerator composition below was test-local only
   at the time this suite was first written (RhythmGenerator had no
   production call site). That is no longer true project-wide: since
   roadmap Phase 10 (generation-randomize), MainComponent::buildSeededSequence
   composes SkipMaskGenerator + PitchGenerator in production, driven by the
   Generate/Randomize UI (generation-live-control spec, design.md Decision
   1/2). RhythmGenerator itself ALSO now has a production call site: since the
   probability-matrices slice, buildSeededSequence composes RhythmGenerator
   (vector ctor) + PitchGenerator when Probability mode is selected - see the
   generateFullProbabilitySequence golden further below, which exercises that
   composition. See also the separate SkipMaskGenerator + PitchGenerator
   golden further below, which exercises the Random-mode production
   composition.

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

#include <vector>

#include "core/Scale.h"
#include "core/Sequence.h"
#include "generation/DeterministicRandom.h"
#include "generation/GenerationParams.h"
#include "generation/MutationEngine.h"
#include "generation/PitchGenerator.h"
#include "generation/RhythmGenerator.h"
#include "generation/SequenceBuilder.h"
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

// Same composition order as generateFullSequence above, but using the
// vector ctor (production shape, probability-matrices) with a uniform
// probability vector in place of the scalar ctor - the RhythmGenerator
// golden above is untouched by this addition. Mirrors
// generateFullSkipMaskSequence's "replicate production composition
// test-locally" convention: this is what MainComponent::buildSeededSequence
// composes when Probability mode is selected.
berlin::Sequence generateFullProbabilitySequence (int numSteps, float stepProbability, int rootNote, berlin::DeterministicRandom& random)
{
    const berlin::Scale scale = berlin::Scale::minor (rootNote);

    const berlin::RhythmGenerator rhythmGenerator (numSteps, std::vector<float> ((std::size_t) numSteps, stepProbability));
    const berlin::PitchGenerator pitchGenerator (scale, rootNote, rootNote + 24);

    berlin::Sequence sequence = rhythmGenerator.generate (random);

    for (int i = 0; i < sequence.size(); ++i)
        if (sequence[i].active)
            sequence[i].note = pitchGenerator.generateNextNote (random);

    return sequence;
}

// Mutation-chain golden (evolution-mutation-engine spec, Manual Mutate,
// Slice 1). Same "replicate production composition test-locally" convention
// as generateFullSkipMaskSequence above: MainComponent::mutate() derives its
// per-click RNG via mutationSeed(sequenceSeed, mutationCount + 1) and applies
// exactly one transform via applyRandomTransform, chaining the result into
// the next click's input. Running that chain twice, from the same base
// Sequence and base seed, for the same number of clicks, must produce
// byte-identical Sequences - since mutationSeed is a pure function of
// (baseSeed, generation) and applyRandomTransform's draw count is a pure
// function of its inputs.
berlin::Sequence runMutationChain (const berlin::Sequence& base, juce::int64 baseSeed, int clicks)
{
    berlin::Sequence current = base;

    for (int generation = 1; generation <= clicks; ++generation)
    {
        berlin::DeterministicRandom random (berlin::MutationEngine::mutationSeed (baseSeed, generation));
        current = berlin::MutationEngine::applyRandomTransform (current, random);
    }

    return current;
}

// Byte-identical replica of buildSeededSequence's PRE-scale-aware-generation
// RhythmMode::random branch: SkipMaskGenerator(16, 11) + PitchGenerator
// (Scale::minor(48), 36, 72), composed in the same fixed order (rhythm draws
// first, then one pitch draw per active step). This is the regression golden
// scale-aware-generation's default GenerationParams must reproduce exactly.
berlin::Sequence generatePreChangeGoldenSequence (juce::int64 seed)
{
    berlin::DeterministicRandom rng (seed);

    const berlin::SkipMaskGenerator maskGenerator (berlin::kNumSteps, 11);
    berlin::Sequence sequence = maskGenerator.generate (rng);

    const berlin::PitchGenerator pitchGenerator (berlin::Scale::minor (48), 36, 72);
    for (int i = 0; i < sequence.size(); ++i)
        if (sequence[i].active)
            sequence[i].note = pitchGenerator.generateNextNote (rng);

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

        beginTest ("RhythmGenerator (vector ctor) + PitchGenerator: same seed produces an identical 16-step Sequence end-to-end");
        {
            constexpr int numSteps = 16;
            constexpr float stepProbability = 0.5f;
            constexpr int rootNote = 60;
            constexpr juce::int64 seed = 12345;

            berlin::DeterministicRandom randomA (seed);
            berlin::DeterministicRandom randomB (seed);

            const berlin::Sequence sequenceA = generateFullProbabilitySequence (numSteps, stepProbability, rootNote, randomA);
            const berlin::Sequence sequenceB = generateFullProbabilitySequence (numSteps, stepProbability, rootNote, randomB);

            expect (sequenceA == sequenceB);
        }

        beginTest ("mutation chain: same base Sequence, seed, and click count reproduce an identical Sequence");
        {
            constexpr int numSteps = 16;
            constexpr int activeSteps = 11;
            constexpr int rootNote = 60;
            constexpr juce::int64 seed = 4242;
            constexpr int clicks = 5;

            berlin::DeterministicRandom baseRandom (seed);
            const berlin::Sequence base = generateFullSkipMaskSequence (numSteps, activeSteps, rootNote, baseRandom);

            const berlin::Sequence resultA = runMutationChain (base, seed, clicks);
            const berlin::Sequence resultB = runMutationChain (base, seed, clicks);

            expect (resultA == resultB);
        }

        beginTest ("Default GenerationParams reproduce pre-change output byte-identically for the same seed");
        {
            // scale-aware-generation's primary regression guard (design.md):
            // ScaleType::minor + rootPitchClass=0 + range[36,72] must be
            // LITERALLY equivalent to the pre-change hardcoded Scale::minor(48)/[36,72].
            constexpr juce::int64 seed = 777777;

            const berlin::GenerationParams defaultParams;   // every field at its default

            const berlin::Sequence golden = generatePreChangeGoldenSequence (seed);
            const berlin::Sequence actual = berlin::buildSeededSequence (seed, defaultParams);

            expect (actual == golden);
        }

        beginTest ("Same seed + same scale/root/range reproduces identically; changing them alters pitch only, not rhythm");
        {
            constexpr juce::int64 seed = 909090;

            berlin::GenerationParams params;
            params.scaleType      = berlin::ScaleType::dorian;
            params.rootPitchClass = 2;    // D
            params.rangeLow       = 40;
            params.rangeHigh      = 64;

            const berlin::Sequence a = berlin::buildSeededSequence (seed, params);
            const berlin::Sequence b = berlin::buildSeededSequence (seed, params);
            expect (a == b);

            berlin::GenerationParams changed = params;
            changed.scaleType      = berlin::ScaleType::phrygian;
            changed.rootPitchClass = 9;   // A
            changed.rangeLow       = 45;
            changed.rangeHigh      = 69;

            const berlin::Sequence c = berlin::buildSeededSequence (seed, changed);

            expectEquals (c.size(), a.size());

            bool anyNoteDiffers = false;

            for (int i = 0; i < a.size(); ++i)
            {
                expect (a[i].active == c[i].active);   // rhythm (active mask) is unchanged by a pitch-only parameter change

                if (a[i].active && a[i].note != c[i].note)
                    anyNoteDiffers = true;
            }

            expect (anyNoteDiffers);   // pitch DID change - proves this isn't a trivially-identical comparison
        }
    }
};

static ReproducibilityTests reproducibilityTests;
