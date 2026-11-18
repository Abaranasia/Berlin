/*
  ==============================================================================

   SequenceBuilder tests (roadmap Phase 11 / vst3-au-plugin, design.md
   Decision 6 / D6). RED first: Source/generation/SequenceBuilder.h does not
   exist yet, so this suite must fail to compile until the GREEN step creates
   it.

   Covers: buildSeededSequence is deterministic per RhythmMode - the SAME seed
   (and same mode/pulses/rotation/stepProbability) MUST produce a bit-identical
   Sequence across repeated calls, for every RhythmMode (random/euclidean/
   probability). This is the exact behavior MainComponent::buildSeededSequence
   already had; the move to Source/generation/ must not change it.

  ==============================================================================
*/

#include <juce_core/juce_core.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <optional>
#include <vector>

#include "core/Scale.h"
#include "generation/DeterministicRandom.h"
#include "generation/EuclideanRhythmGenerator.h"
#include "generation/GenerationParams.h"
#include "generation/PitchGenerator.h"
#include "generation/RhythmGenerator.h"
#include "generation/SequenceBuilder.h"
#include "generation/SkipMaskGenerator.h"

namespace
{
using berlin::GenerationParams;
using berlin::MelodyStyle;
using berlin::RhythmMode;

const RhythmMode kAllModes[] = { RhythmMode::random, RhythmMode::euclidean, RhythmMode::probability };

// Independent re-statement of the builder's draw order (rhythm, then one pitch
// pick per active step) so the Smooth/Random paths are checked against a model
// rather than against themselves. `smooth` selects generateMelodyNote with the
// proximity reference carried across rests; otherwise the pre-change loop.
berlin::Sequence referenceSequence (juce::int64 seed, const GenerationParams& params, bool smooth)
{
    const int length = std::clamp (params.length, 1, berlin::kMaxSteps);
    berlin::DeterministicRandom rng (seed);

    auto sequence = [&]
    {
        switch (params.mode)
        {
            case RhythmMode::euclidean:
                return berlin::EuclideanRhythmGenerator (length, params.pulses, params.rotation).generate();
            case RhythmMode::probability:
                return berlin::RhythmGenerator (length, std::vector<float> ((std::size_t) length, params.stepProbability)).generate (rng);
            case RhythmMode::random:
            default:
                return berlin::SkipMaskGenerator (length, berlin::randomActiveCount (length)).generate (rng);
        }
    }();

    int low = params.rangeLow, high = params.rangeHigh;
    berlin::normalizePitchRange (low, high);
    const berlin::PitchGenerator pitch (berlin::Scale::fromPitchClass (params.scaleType, params.rootPitchClass), low, high);

    std::optional<int> previous;

    for (int i = 0; i < sequence.size(); ++i)
    {
        if (! sequence[i].active)
            continue;

        if (smooth)
            previous = sequence[i].note = pitch.generateMelodyNote (rng, previous);
        else
            sequence[i].note = pitch.generateNextNote (rng);
    }

    return sequence;
}

std::vector<int> activeNotes (const berlin::Sequence& sequence)
{
    std::vector<int> notes;

    for (int i = 0; i < sequence.size(); ++i)
        if (sequence[i].active)
            notes.push_back (sequence[i].note);

    return notes;
}

struct MelodyStats
{
    double meanInterval = 0.0, leapFraction = 0.0, meanDevFromMedian = 0.0, tonicShare = 0.0, firstNoteTonicShare = 0.0;
    int intervals = 0, notes = 0, seedsWithFirstNonTonic = 0;
};

// Aggregated over the whole seed set (never per-seed): intervals are absolute
// semitone distances between consecutive active notes; a leap is > 5 semitones.
MelodyStats measure (MelodyStyle style, int seedCount, int length, double medianPitch, int rootPitchClass)
{
    long long intervalSum = 0, leaps = 0, intervals = 0, notes = 0, tonics = 0, firstTonics = 0, firstNonTonics = 0;
    double deviationSum = 0.0;

    for (int seed = 1; seed <= seedCount; ++seed)
    {
        GenerationParams params;
        params.length = length;
        params.melody = style;
        const auto active = activeNotes (berlin::buildSeededSequence (seed, params));

        for (std::size_t i = 0; i < active.size(); ++i)
        {
            ++notes;
            deviationSum += std::abs (active[i] - medianPitch);
            const bool tonic = active[i] % 12 == rootPitchClass;
            tonics += tonic ? 1 : 0;

            if (i == 0)
            {
                firstTonics += tonic ? 1 : 0;
                firstNonTonics += tonic ? 0 : 1;
            }
            else
            {
                const int interval = std::abs (active[i] - active[i - 1]);
                intervalSum += interval;
                leaps += interval > 5 ? 1 : 0;
                ++intervals;
            }
        }
    }

    MelodyStats stats;
    stats.intervals = static_cast<int> (intervals);
    stats.notes = static_cast<int> (notes);
    stats.meanInterval = static_cast<double> (intervalSum) / static_cast<double> (intervals);
    stats.leapFraction = static_cast<double> (leaps) / static_cast<double> (intervals);
    stats.meanDevFromMedian = deviationSum / static_cast<double> (notes);
    stats.tonicShare = static_cast<double> (tonics) / static_cast<double> (notes);
    stats.firstNoteTonicShare = static_cast<double> (firstTonics) / static_cast<double> (seedCount);
    stats.seedsWithFirstNonTonic = static_cast<int> (firstNonTonics);
    return stats;
}
} // namespace

class SequenceBuilderTests final : public juce::UnitTest
{
public:
    SequenceBuilderTests() : juce::UnitTest ("SequenceBuilder", "Berlin") {}

    void runTest() override
    {
        constexpr juce::int64 seed = 424242;

        auto paramsFor = [] (berlin::RhythmMode mode, int pulses, int rotation, float stepProbability)
        {
            berlin::GenerationParams params;
            params.mode            = mode;
            params.pulses          = pulses;
            params.rotation        = rotation;
            params.stepProbability = stepProbability;
            return params;
        };

        beginTest ("RhythmMode::random: same seed produces an identical Sequence");
        {
            const auto params = paramsFor (berlin::RhythmMode::random, 5, 0, 0.5f);
            const auto a = berlin::buildSeededSequence (seed, params);
            const auto b = berlin::buildSeededSequence (seed, params);
            expect (a == b);
        }

        beginTest ("RhythmMode::euclidean: same seed/pulses/rotation produces an identical Sequence");
        {
            const auto params = paramsFor (berlin::RhythmMode::euclidean, 7, 3, 0.5f);
            const auto a = berlin::buildSeededSequence (seed, params);
            const auto b = berlin::buildSeededSequence (seed, params);
            expect (a == b);
        }

        beginTest ("RhythmMode::probability: same seed/stepProbability produces an identical Sequence");
        {
            const auto params = paramsFor (berlin::RhythmMode::probability, 5, 0, 0.35f);
            const auto a = berlin::buildSeededSequence (seed, params);
            const auto b = berlin::buildSeededSequence (seed, params);
            expect (a == b);
        }

        beginTest ("Different seeds produce different Sequences for RhythmMode::random");
        {
            const auto params = paramsFor (berlin::RhythmMode::random, 5, 0, 0.5f);
            const auto a = berlin::buildSeededSequence (seed, params);
            const auto b = berlin::buildSeededSequence (seed + 1, params);
            expect (! (a == b));
        }

        beginTest ("Euclidean pulses/rotation change is reflected even with the same seed");
        {
            const auto a = berlin::buildSeededSequence (seed, paramsFor (berlin::RhythmMode::euclidean, 5, 0, 0.5f));
            const auto b = berlin::buildSeededSequence (seed, paramsFor (berlin::RhythmMode::euclidean, 9, 2, 0.5f));
            expect (! (a == b));
        }

        // ---- variable length (step-model) ----

        beginTest ("randomActiveCount keeps the 11-of-16 ratio and never drops below 1");
        {
            expectEquals (berlin::randomActiveCount (16), 11);
            expectEquals (berlin::randomActiveCount (1), 1);
            expectEquals (berlin::randomActiveCount (64), 44);
        }

        beginTest ("every mode builds a Sequence of exactly params.length steps, clamped to 1..kMaxSteps");
        {
            for (auto mode : { berlin::RhythmMode::random, berlin::RhythmMode::euclidean, berlin::RhythmMode::probability })
            {
                for (int length : { 1, 8, 24, 64 })
                {
                    auto params = paramsFor (mode, 5, 0, 0.5f);
                    params.length = length;
                    expectEquals (berlin::buildSeededSequence (seed, params).size(), length);
                }

                auto params = paramsFor (mode, 5, 0, 0.5f);
                params.length = 0;
                expectEquals (berlin::buildSeededSequence (seed, params).size(), 1);
                params.length = 500;
                expectEquals (berlin::buildSeededSequence (seed, params).size(), berlin::kMaxSteps);
            }
        }

        beginTest ("random mode at length 64 has randomActiveCount active steps");
        {
            auto params = paramsFor (berlin::RhythmMode::random, 5, 0, 0.5f);
            params.length = 64;
            const auto seq = berlin::buildSeededSequence (seed, params);

            int active = 0;
            for (int i = 0; i < seq.size(); ++i)
                if (seq[i].active)
                    ++active;

            expectEquals (active, 44);
        }

        beginTest ("euclidean pulses above the length are clamped and rotation wraps within it");
        {
            auto params = paramsFor (berlin::RhythmMode::euclidean, 20, 0, 0.5f);
            params.length = 8;
            const auto seq = berlin::buildSeededSequence (seed, params);

            for (int i = 0; i < seq.size(); ++i)
                expect (seq[i].active);

            params.pulses   = 3;
            params.rotation = 11;   // == 3 within length 8
            auto rotated = params;
            rotated.rotation = 3;
            expect (berlin::buildSeededSequence (seed, params) == berlin::buildSeededSequence (seed, rotated));
        }

        beginTest ("generation never sets skipped");
        {
            for (auto mode : { berlin::RhythmMode::random, berlin::RhythmMode::euclidean, berlin::RhythmMode::probability })
            {
                auto params = paramsFor (mode, 5, 0, 0.5f);
                params.length = 32;
                const auto seq = berlin::buildSeededSequence (seed, params);

                for (int i = 0; i < seq.size(); ++i)
                    expect (! seq[i].skipped);
            }
        }

        beginTest ("length defaults to 16 and an explicit 16 matches the default output");
        {
            const berlin::GenerationParams defaults;
            expectEquals (defaults.length, berlin::kNumSteps);

            for (auto mode : { berlin::RhythmMode::random, berlin::RhythmMode::euclidean, berlin::RhythmMode::probability })
            {
                auto explicit16 = paramsFor (mode, 5, 0, 0.5f);
                explicit16.length = 16;
                expect (berlin::buildSeededSequence (seed, paramsFor (mode, 5, 0, 0.5f))
                        == berlin::buildSeededSequence (seed, explicit16));
            }
        }

        // ---- normalizePitchRange (scale-aware-generation design.md invariant) ----

        beginTest ("normalizePitchRange swaps an inverted pair so low <= high");
        {
            int low = 72, high = 36;
            berlin::normalizePitchRange (low, high);
            expect (low <= high);
            expectEquals (low, 36);
            expectEquals (high, 72);
        }

        beginTest ("normalizePitchRange clamps out-of-[0,127] bounds");
        {
            int low = -20, high = 200;
            berlin::normalizePitchRange (low, high);
            expect (low >= berlin::kMinPitch);
            expect (high <= berlin::kMaxPitch);
            expectEquals (low, 0);
            expectEquals (high, 127);
        }

        beginTest ("normalizePitchRange widens a sub-octave span upward (away from the ceiling) to at least kMinPitchRangeSpan");
        {
            int low = 60, high = 61;   // 1-semitone span, nowhere near the 127 ceiling
            berlin::normalizePitchRange (low, high);
            expect (high - low >= berlin::kMinPitchRangeSpan);
            expectEquals (low, 60);    // low must not move - widening happens upward here
            expectEquals (high, 72);
        }

        beginTest ("normalizePitchRange at the 127 ceiling widens downward, not upward past the ceiling");
        {
            int low = 125, high = 127;   // pinned at the ceiling, 2-semitone span
            berlin::normalizePitchRange (low, high);
            expectEquals (high, 127);   // high must not move past the ceiling
            expect (high - low >= berlin::kMinPitchRangeSpan);
            expect (low < 125);   // widened by moving low DOWN, not high up
        }

        // ---- musical-notes (melody-core) Phase 2: Smooth in the builder ----

        beginTest ("Smooth: deterministic across two builds in every rhythm mode, and matches the draw-order reference model");
        {
            for (const auto mode : kAllModes)
            {
                auto params = paramsFor (mode, 5, 2, 0.5f);
                params.melody = MelodyStyle::smooth;

                const auto a = berlin::buildSeededSequence (seed, params);
                expect (a == berlin::buildSeededSequence (seed, params));
                expect (a == referenceSequence (seed, params, true));   // rhythm first, then one pick per active step
                expect (! activeNotes (a).empty());
            }
        }

        beginTest ("Random (default melody) is byte-identical to the pre-change loop in all 3 rhythm modes, seeds 1..50");
        {
            for (const auto mode : kAllModes)
                for (int s = 1; s <= 50; ++s)
                {
                    const auto params = paramsFor (mode, 5, 2, 0.5f);
                    expect (berlin::buildSeededSequence (s, params) == referenceSequence (s, params, false));
                }
        }

        beginTest ("Smooth: rests keep the proximity reference (index step <= 4 across rests); inactive notes are untouched");
        {
            // Euclidean 4 pulses of 32 steps: long rests between active steps.
            auto params = paramsFor (RhythmMode::euclidean, 4, 0, 0.5f);
            params.length = 32;
            auto smoothParams = params;
            smoothParams.melody = MelodyStyle::smooth;

            const auto scale = berlin::Scale::fromPitchClass (params.scaleType, params.rootPitchClass);
            std::vector<int> candidates;

            for (int note = params.rangeLow; note <= params.rangeHigh; ++note)
                if (scale.contains (note))
                    candidates.push_back (note);

            int pairs = 0;

            for (int s = 1; s <= 30; ++s)
            {
                const auto smooth = berlin::buildSeededSequence (s, smoothParams);
                const auto random = berlin::buildSeededSequence (s, params);
                int previousIndex = -1, restsBetween = 0;
                bool sawRestGap = false;

                for (int i = 0; i < smooth.size(); ++i)
                {
                    if (! smooth[i].active)
                    {
                        expectEquals (smooth[i].note, random[i].note);   // never written for rests
                        ++restsBetween;
                        continue;
                    }

                    const int index = static_cast<int> (std::find (candidates.begin(), candidates.end(), smooth[i].note) - candidates.begin());
                    expect (index < static_cast<int> (candidates.size()));

                    if (previousIndex >= 0)
                    {
                        expect (std::abs (index - previousIndex) <= 4);
                        sawRestGap = sawRestGap || restsBetween > 0;
                        ++pairs;
                    }

                    previousIndex = index;
                    restsBetween = 0;
                }

                expect (sawRestGap);   // the scenario really spans rests
            }

            expect (pairs > 0);
        }

        beginTest ("Smooth: length 1 gives one in-scale in-range active note; Euclidean pulses 0 builds with no active steps and no pitch draw");
        {
            auto one = paramsFor (RhythmMode::random, 5, 0, 0.5f);
            one.length = 1;
            one.melody = MelodyStyle::smooth;
            const auto scale = berlin::Scale::fromPitchClass (one.scaleType, one.rootPitchClass);

            for (int s = 1; s <= 20; ++s)
            {
                const auto sequence = berlin::buildSeededSequence (s, one);
                expectEquals (sequence.size(), 1);
                expect (sequence[0].active);
                expect (sequence[0].note >= one.rangeLow && sequence[0].note <= one.rangeHigh);
                expect (scale.contains (sequence[0].note));
            }

            auto none = paramsFor (RhythmMode::euclidean, 0, 0, 0.5f);
            none.melody = MelodyStyle::smooth;
            const auto smooth = berlin::buildSeededSequence (seed, none);
            none.melody = MelodyStyle::random;

            expect (activeNotes (smooth).empty());
            expectEquals (smooth.size(), berlin::kNumSteps);
            expect (smooth == berlin::buildSeededSequence (seed, none));   // nothing written either way
        }

        beginTest ("Smooth differs from Random in pitch across the seed set");
        {
            int differing = 0;

            for (int s = 1; s <= 20; ++s)
            {
                GenerationParams random, smooth;
                smooth.melody = MelodyStyle::smooth;
                const auto a = berlin::buildSeededSequence (s, random), b = berlin::buildSeededSequence (s, smooth);

                for (int i = 0; i < a.size(); ++i)
                    differing += (a[i].active && a[i].note != b[i].note) ? 1 : 0;
            }

            expect (differing > 0);
        }

        beginTest ("Melody changes pitch only: active/skipped/velocity/gatePercent/length equal, seeds 1..50 x 3 rhythm modes");
        {
            int compared = 0, pitchDiffs = 0;

            for (const auto mode : kAllModes)
                for (int s = 1; s <= 50; ++s)
                {
                    auto randomParams = paramsFor (mode, 5, 2, 0.5f);
                    auto smoothParams = randomParams;
                    smoothParams.melody = MelodyStyle::smooth;
                    const auto a = berlin::buildSeededSequence (s, randomParams), b = berlin::buildSeededSequence (s, smoothParams);

                    expectEquals (a.size(), b.size());

                    for (int i = 0; i < a.size(); ++i)
                    {
                        expect (a[i].active == b[i].active);
                        expect (a[i].skipped == b[i].skipped);
                        expectEquals (a[i].velocity, b[i].velocity);
                        expectEquals (a[i].gatePercent, b[i].gatePercent);
                        ++compared;

                        if (! a[i].active)
                            expectEquals (a[i].note, b[i].note);
                        else if (a[i].note != b[i].note)
                            ++pitchDiffs;
                    }
                }

            expectEquals (compared, 3 * 50 * berlin::kNumSteps);
            expect (pitchDiffs > 0);
        }

        beginTest ("Smooth statistics vs Random (seeds 1..200, length 64, default params): smaller intervals, fewer leaps, closer to the median, more tonic");
        {
            const GenerationParams defaults;
            const auto scale = berlin::Scale::fromPitchClass (defaults.scaleType, defaults.rootPitchClass);
            std::vector<int> candidates;

            for (int note = defaults.rangeLow; note <= defaults.rangeHigh; ++note)
                if (scale.contains (note))
                    candidates.push_back (note);

            const auto n = candidates.size();
            const double median = (n % 2 == 1) ? candidates[n / 2] : 0.5 * (candidates[n / 2 - 1] + candidates[n / 2]);
            const double uniformTonicShare = static_cast<double> (std::count_if (candidates.begin(), candidates.end(),
                                                 [&] (int c) { return c % 12 == defaults.rootPitchClass; }))
                                           / static_cast<double> (n);

            const auto random = measure (MelodyStyle::random, 200, 64, median, defaults.rootPitchClass);
            const auto smooth = measure (MelodyStyle::smooth, 200, 64, median, defaults.rootPitchClass);

            logMessage ("melody stats (seeds 1..200, length 64): intervals random=" + juce::String (random.intervals)
                        + " smooth=" + juce::String (smooth.intervals));
            logMessage ("meanAbsInterval  random=" + juce::String (random.meanInterval, 3) + " smooth=" + juce::String (smooth.meanInterval, 3));
            logMessage ("leapFraction     random=" + juce::String (random.leapFraction, 4) + " smooth=" + juce::String (smooth.leapFraction, 4));
            logMessage ("meanDevFromMedian random=" + juce::String (random.meanDevFromMedian, 3) + " smooth=" + juce::String (smooth.meanDevFromMedian, 3)
                        + " (median pitch " + juce::String (median, 1) + ")");
            logMessage ("tonicShare       random=" + juce::String (random.tonicShare, 4) + " smooth=" + juce::String (smooth.tonicShare, 4)
                        + " uniform=" + juce::String (uniformTonicShare, 4));
            logMessage ("firstNoteTonic   random=" + juce::String (random.firstNoteTonicShare, 4) + " smooth=" + juce::String (smooth.firstNoteTonicShare, 4)
                        + " seedsWithNonTonicFirst(smooth)=" + juce::String (smooth.seedsWithFirstNonTonic));

            expect (random.intervals > 1000 && smooth.intervals > 1000);   // the sets really are populated
            expect (2.0 * smooth.meanInterval < random.meanInterval);
            expect (smooth.leapFraction < random.leapFraction);
            expect (smooth.meanDevFromMedian < random.meanDevFromMedian);
            expect (smooth.tonicShare > random.tonicShare);
            expect (smooth.tonicShare > uniformTonicShare);
            expect (smooth.tonicShare >= 1.2 * uniformTonicShare);   // a measurable pull, not a negligible one
            expect (smooth.firstNoteTonicShare > uniformTonicShare);
            expect (smooth.seedsWithFirstNonTonic > 0);
        }

        beginTest ("normalizePitchRange is a no-op for an already-valid range");
        {
            int low = 36, high = 72;
            berlin::normalizePitchRange (low, high);
            expectEquals (low, 36);
            expectEquals (high, 72);
        }
    }
};

static SequenceBuilderTests sequenceBuilderTests;
