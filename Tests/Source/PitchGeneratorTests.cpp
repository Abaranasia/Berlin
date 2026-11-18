/*
  ==============================================================================

   PitchGenerator tests (deterministic-generation spec). RED first:
   Source/generation/PitchGenerator.h/.cpp do not exist yet, so this suite
   fails to compile until 5.2 creates them. Covers in-range/in-scale note
   generation, the empty-range clamp fallback (nearest in-scale note outside
   the range, computed at ctor time), and its lower-note tie-break rule
   (design.md: ties resolved to the LOWER note; the fallback consumes no RNG
   draw).

  ==============================================================================
*/

#include <juce_core/juce_core.h>

#include <algorithm>
#include <cstdlib>
#include <optional>
#include <vector>

#include "core/Scale.h"
#include "generation/DeterministicRandom.h"
#include "generation/GenerationParams.h"
#include "generation/PitchGenerator.h"

class PitchGeneratorTests final : public juce::UnitTest
{
public:
    PitchGeneratorTests() : juce::UnitTest ("PitchGenerator", "Berlin") {}

    void runTest() override
    {
        beginTest ("generateNextNote returns a note in range and in scale");
        {
            const berlin::Scale scale = berlin::Scale::major (60);
            berlin::PitchGenerator generator (scale, 60, 72);
            berlin::DeterministicRandom random (12345);

            for (int i = 0; i < 50; ++i)
            {
                const int note = generator.generateNextNote (random);

                expect (note >= 60 && note <= 72);
                expect (scale.contains (note));
            }
        }

        beginTest ("same seed produces same sequence of generated notes");
        {
            const berlin::Scale scale = berlin::Scale::major (60);
            berlin::PitchGenerator generatorA (scale, 60, 72);
            berlin::PitchGenerator generatorB (scale, 60, 72);
            berlin::DeterministicRandom randomA (555);
            berlin::DeterministicRandom randomB (555);

            for (int i = 0; i < 20; ++i)
                expectEquals (generatorA.generateNextNote (randomA), generatorB.generateNextNote (randomB));
        }

        beginTest ("empty-range clamp - single out-of-scale note ties, resolves to the lower in-scale neighbour");
        {
            const berlin::Scale scale = berlin::Scale::major (60);

            // 61 (C#) is not in C major; its nearest in-scale neighbours are
            // 60 (root, one semitone below) and 62 (one semitone above) - an
            // exact tie at radius 1. design.md: ties resolve to the LOWER note.
            expect (! scale.contains (61));
            expect (scale.contains (60));
            expect (scale.contains (62));

            berlin::PitchGenerator generator (scale, 61, 61);
            berlin::DeterministicRandom random (1);

            expectEquals (generator.generateNextNote (random), 60);
        }

        beginTest ("empty-range clamp fallback consumes no RNG draw");
        {
            const berlin::Scale scale = berlin::Scale::major (60);
            berlin::PitchGenerator generator (scale, 61, 61);

            berlin::DeterministicRandom randomA (2026);
            berlin::DeterministicRandom randomB (2026);

            // Calling the fallback path repeatedly must not advance randomA's
            // draw sequence relative to an untouched randomB constructed from
            // the same seed.
            for (int i = 0; i < 5; ++i)
                generator.generateNextNote (randomA);

            for (int i = 0; i < 10; ++i)
                expectEquals (randomA.nextInt (1000), randomB.nextInt (1000));
        }

        beginTest ("constructor swaps an inverted range so low <= high");
        {
            const berlin::Scale scale = berlin::Scale::major (60);
            berlin::PitchGenerator generator (scale, 72, 60); // deliberately inverted
            berlin::DeterministicRandom random (99);

            for (int i = 0; i < 20; ++i)
            {
                const int note = generator.generateNextNote (random);

                expect (note >= 60 && note <= 72);
                expect (scale.contains (note));
            }
        }

        beginTest ("every generated note satisfies scale.contains() and range, for every catalog ScaleType");
        {
            // deterministic-generation spec: "Every generated note satisfies scale
            // membership and range for any chosen scale/root/range" - exercised
            // here across all 6 scale-aware-generation catalog ScaleTypes.
            using berlin::ScaleType;

            const ScaleType types[] = { ScaleType::minor, ScaleType::major, ScaleType::dorian,
                                         ScaleType::phrygian, ScaleType::mixolydian, ScaleType::harmonicMinor };

            for (auto type : types)
            {
                for (int pitchClass = 0; pitchClass < 12; pitchClass += 3)   // sample every 3rd pitch class
                {
                    const berlin::Scale scale = berlin::Scale::fromPitchClass (type, pitchClass);
                    berlin::PitchGenerator generator (scale, 40, 64);
                    berlin::DeterministicRandom random (1000 + pitchClass);

                    for (int i = 0; i < 20; ++i)
                    {
                        const int note = generator.generateNextNote (random);
                        expect (note >= 40 && note <= 64);
                        expect (scale.contains (note));
                    }
                }
            }
        }

        beginTest ("D Dorian, range [40, 64]: generated notes are D Dorian and in range, not the old hardcoded C minor/[36,72]");
        {
            const berlin::Scale dDorian = berlin::Scale::fromPitchClass (berlin::ScaleType::dorian, 2); // D
            berlin::PitchGenerator generator (dDorian, 40, 64);
            berlin::DeterministicRandom random (2026);

            const berlin::Scale oldHardcodedScale = berlin::Scale::minor (48);

            for (int i = 0; i < 30; ++i)
            {
                const int note = generator.generateNextNote (random);
                expect (note >= 40 && note <= 64);
                expect (dDorian.contains (note));
            }

            // Sanity: D Dorian is NOT the same pitch-class set as C minor, so this
            // scenario is actually exercising different scale data, not a
            // coincidentally-identical interval set. Note 51 (D#/Eb) is in the
            // old hardcoded C minor (root 48) but NOT in D Dorian (root 50).
            expect (oldHardcodedScale.contains (51));
            expect (! dDorian.contains (51));
        }

        // ---- musical-notes (melody-core) Phase 1: param + weight function ----

        beginTest ("MelodyStyle defaults to random, has exactly random/smooth, and is the last GenerationParams field");
        {
            const berlin::GenerationParams defaults;
            expect (defaults.melody == berlin::MelodyStyle::random);
            expectEquals (static_cast<int> (berlin::MelodyStyle::random), 0);
            expectEquals (static_cast<int> (berlin::MelodyStyle::smooth), 1);

            // Existing positional aggregate initializers (up to rangeHigh) must still compile
            // and leave melody at its default: melody is appended last.
            const berlin::GenerationParams positional { berlin::RhythmMode::euclidean, 7, 3, 0.25f, true, 24,
                                                        berlin::ScaleType::dorian, 2, 40, 64 };
            expect (positional.melody == berlin::MelodyStyle::random);
            expectEquals (positional.rangeHigh, 64);
        }

        beginTest ("smoothWeight pins exact values: proximity x median pull x tonic");
        {
            using berlin::melody::smoothWeight;

            // n = 22, previous index 0 (edge: prevOff = 21): moving to i=1 is toward the centre.
            expectEquals (smoothWeight (1, 0, 22, false), 1024);   // 16 * (22 + 2*21) * 1
            expectEquals (smoothWeight (0, 0, 22, true), 264);     // 6 * 22 * 2 (repeat, not toward the centre)
            expectEquals (smoothWeight (5, 0, 22, false), 0);      // d = 5
            expectEquals (smoothWeight (9, 0, 22, true), 0);       // d > 5

            // Window edge: d = 4 still weighs 1 * M * K.
            expectEquals (smoothWeight (4, 0, 22, false), 1 * (22 + 2 * 21) * 1);
        }

        beginTest ("smoothWeight first step (no previous) weighs by tonic only");
        {
            using berlin::melody::smoothWeight;
            expectEquals (smoothWeight (7, std::nullopt, 22, true), 2);
            expectEquals (smoothWeight (7, std::nullopt, 22, false), 1);
            expectEquals (smoothWeight (0, std::nullopt, 1, false), 1);
        }

        beginTest ("smoothWeight median pull: none at the centre, grows toward the edge, only for moves toward the centre");
        {
            using berlin::melody::smoothWeight;

            // Centre (n = 21, p = 10): prevOff = 0, so M = n for every candidate.
            expectEquals (smoothWeight (11, 10, 21, false), 16 * 21 * 1);
            expectEquals (smoothWeight (9, 10, 21, false), 16 * 21 * 1);

            // Edge (p = 0, prevOff = 20): inward candidate gets n + 2*prevOff = 61; a repeat does not.
            expectEquals (smoothWeight (1, 0, 21, false), 16 * 61 * 1);
            expectEquals (smoothWeight (0, 0, 21, false), 6 * 21 * 1);

            // Off-centre (p = 1, prevOff = 18): inward i=2 (candOff 16) pulled, outward i=0 (candOff 20) not.
            expectEquals (smoothWeight (2, 1, 21, false), 16 * (21 + 36) * 1);
            expectEquals (smoothWeight (0, 1, 21, false), 16 * 21 * 1);
        }

        beginTest ("smoothWeight n=1 is positive; largest weight and window total stay far below INT_MAX");
        {
            using berlin::melody::smoothWeight;
            expect (smoothWeight (0, 0, 1, false) > 0);
            expect (smoothWeight (0, std::nullopt, 1, true) > 0);

            int largest = 0, total = 0;

            for (int i = 0; i <= 4; ++i)
            {
                const int w = smoothWeight (i, 0, 128, true);
                largest = juce::jmax (largest, w);
                total += w;
            }

            expectEquals (largest, 16 * (128 + 2 * 127) * 2);
            expect (largest < 20000);
            expect (total < 200000);
        }

        // ---- musical-notes (melody-core) Phase 2: generateMelodyNote ----

        beginTest ("generateMelodyNote: in scale, in range, window rule (<= 4 candidate indices) and deterministic, 6 scales x 2 ranges x 20 seeds");
        {
            const berlin::ScaleType types[] = { berlin::ScaleType::minor, berlin::ScaleType::major, berlin::ScaleType::dorian,
                                                berlin::ScaleType::phrygian, berlin::ScaleType::mixolydian,
                                                berlin::ScaleType::harmonicMinor };
            const int ranges[][2] = { { 36, 72 }, { 60, 72 } };
            int totalSteps = 0;

            for (int t = 0; t < 6; ++t)
            {
                const auto scale = berlin::Scale::fromPitchClass (types[t], (t * 2) % 12);

                for (const auto& range : ranges)
                {
                    std::vector<int> candidates;

                    for (int note = range[0]; note <= range[1]; ++note)
                        if (scale.contains (note))
                            candidates.push_back (note);

                    expect (candidates.size() > 1);

                    const berlin::PitchGenerator generator (scale, range[0], range[1]);

                    for (int seed = 1; seed <= 20; ++seed)
                    {
                        berlin::DeterministicRandom randomA (seed), randomB (seed);
                        std::optional<int> previousA, previousB;
                        int previousIndex = -1;

                        for (int step = 0; step < 40; ++step)
                        {
                            const int a = generator.generateMelodyNote (randomA, previousA);
                            const int b = generator.generateMelodyNote (randomB, previousB);
                            previousA = a;
                            previousB = b;
                            ++totalSteps;

                            expectEquals (a, b);                                   // same seed, same notes
                            expect (a >= range[0] && a <= range[1]);
                            expect (scale.contains (a));

                            const auto it = std::find (candidates.begin(), candidates.end(), a);
                            expect (it != candidates.end());
                            const int index = static_cast<int> (it - candidates.begin());

                            if (previousIndex >= 0)
                                expect (std::abs (index - previousIndex) <= 4);

                            previousIndex = index;
                        }
                    }
                }
            }

            expectEquals (totalSteps, 6 * 2 * 20 * 40);   // the loops really ran
        }

        beginTest ("generateMelodyNote: empty range returns the same fallback as Random with NO draw");
        {
            const berlin::Scale scale = berlin::Scale::major (60);
            berlin::PitchGenerator generator (scale, 61, 61);   // C# only: no in-scale note

            berlin::DeterministicRandom random (2026), twin (2026), randomForRandomPath (2026);
            const int randomNote = generator.generateNextNote (randomForRandomPath);

            expectEquals (randomNote, 60);

            for (int i = 0; i < 5; ++i)
            {
                expectEquals (generator.generateMelodyNote (random, std::nullopt), randomNote);
                expectEquals (generator.generateMelodyNote (random, 60), randomNote);
            }

            for (int i = 0; i < 10; ++i)
                expectEquals (random.nextInt (1000000), twin.nextInt (1000000));   // untouched stream

            // A different empty range resolves to its own nearest note (not a constant).
            berlin::PitchGenerator upper (berlin::Scale::major (60), 66, 66);   // F# -> tie: lower F (65)
            berlin::DeterministicRandom r2 (9);
            expectEquals (upper.generateMelodyNote (r2, std::nullopt), 65);
        }

        beginTest ("generateMelodyNote: a single in-scale note is returned on every call");
        {
            const berlin::Scale scale = berlin::Scale::major (60);
            berlin::PitchGenerator generator (scale, 60, 60);
            berlin::DeterministicRandom random (77);

            expectEquals (generator.generateMelodyNote (random, std::nullopt), 60);

            for (int i = 0; i < 10; ++i)
                expectEquals (generator.generateMelodyNote (random, 60), 60);

            berlin::PitchGenerator other (scale, 64, 64);   // E, a different single candidate
            expectEquals (other.generateMelodyNote (random, std::nullopt), 64);
        }

        beginTest ("generateMelodyNote consumes exactly one draw per call (n == 1 and n > 1)");
        {
            const berlin::Scale scale = berlin::Scale::major (60);

            // n == 1: one draw even though there is nothing to choose.
            {
                berlin::PitchGenerator generator (scale, 60, 60);
                berlin::DeterministicRandom random (31337), twin (31337);
                generator.generateMelodyNote (random, std::nullopt);
                twin.nextInt (1000);

                for (int i = 0; i < 10; ++i)
                    expectEquals (random.nextInt (1000000), twin.nextInt (1000000));
            }

            // n > 1: exactly one per call, 12 calls == 12 twin draws.
            {
                berlin::PitchGenerator generator (scale, 48, 84);
                berlin::DeterministicRandom random (31337), twin (31337);
                std::optional<int> previous;

                for (int i = 0; i < 12; ++i)
                {
                    previous = generator.generateMelodyNote (random, previous);
                    twin.nextInt (1000);
                }

                for (int i = 0; i < 10; ++i)
                    expectEquals (random.nextInt (1000000), twin.nextInt (1000000));
            }
        }
    }
};

static PitchGeneratorTests pitchGeneratorTests;
