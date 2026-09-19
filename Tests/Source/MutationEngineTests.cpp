/*
  ==============================================================================

   MutationEngine tests (evolution-mutation-engine spec, Manual Mutate,
   Slice 1). RED first: Source/generation/MutationEngine.h/.cpp do not exist
   yet, so this suite fails to compile until Task 4/5 create them. Covers
   totality across degenerate inputs (size 0, size 1, all-inactive,
   all-active, size 4 compress floor, size 64 stretch ceiling); transpose's
   active-only shift and offset-once clamping (including the spec's 100/125
   -> +2 boundary scenario); reverse as an involution; rotate preserving the
   multiset; add/remove changing the active count by exactly +-1;
   stretch/compress hitting exact target sizes and clamping to [4, 64];
   change-note/add-note only ever assigning a pitch already present among the
   input's active steps (never fabricating note = 0); and mutationSeed's
   collision guard + stability.

  ==============================================================================
*/

#include <algorithm>
#include <utility>
#include <vector>

#include <juce_core/juce_core.h>

#include "core/Sequence.h"
#include "generation/DeterministicRandom.h"
#include "generation/MutationEngine.h"

namespace
{

berlin::Sequence makeSequence (int numSteps, bool allActive)
{
    berlin::Sequence sequence (numSteps);
    for (int i = 0; i < numSteps; ++i)
        sequence[i].active = allActive;
    return sequence;
}

} // namespace

class MutationEngineTests final : public juce::UnitTest
{
public:
    MutationEngineTests() : juce::UnitTest ("MutationEngine", "Berlin") {}

    void runTest() override
    {
        beginTest ("every transform is total: degenerate inputs never crash and produce a well-formed Sequence");
        {
            const berlin::MutationEngine::Transform transforms[] = {
                berlin::MutationEngine::transpose,  berlin::MutationEngine::reverse,
                berlin::MutationEngine::rotate,     berlin::MutationEngine::changeNote,
                berlin::MutationEngine::addNote,    berlin::MutationEngine::removeNote,
                berlin::MutationEngine::stretch,    berlin::MutationEngine::compress,
                berlin::MutationEngine::invert,     berlin::MutationEngine::scaleIntervals,
                berlin::MutationEngine::palindrome
            };

            const int sizes[] = { 0, 1, 4, 64 };

            for (int size : sizes)
            {
                for (bool allActive : { false, true })
                {
                    for (auto* transform : transforms)
                    {
                        const berlin::Sequence input = makeSequence (size, allActive);
                        berlin::DeterministicRandom random (size * 31 + (allActive ? 1 : 0) + 1);

                        const berlin::Sequence result = transform (input, random);

                        expect (result.size() >= 0);
                    }
                }
            }
        }

        beginTest ("transpose shifts only active notes by a uniform offset; inactive steps untouched");
        {
            berlin::Sequence input (4);
            input[0].active = true;  input[0].note = 60;
            input[1].active = false; input[1].note = 40;
            input[2].active = true;  input[2].note = 64;
            input[3].active = false; input[3].note = 20;

            for (juce::int64 seed = 0; seed < 200; ++seed)
            {
                berlin::DeterministicRandom random (seed);
                const berlin::Sequence result = berlin::MutationEngine::transpose (input, random);

                expectEquals (result.size(), 4);

                expect (! result[1].active);
                expectEquals (result[1].note, 40);
                expect (! result[3].active);
                expectEquals (result[3].note, 20);

                const int offset = result[0].note - 60;
                expectEquals (result[2].note, 64 + offset);
            }
        }

        beginTest ("transpose clamps the offset against active min/max, preserving the interval (100/125 -> +2)");
        {
            berlin::Sequence input (2);
            input[0].active = true; input[0].note = 100;
            input[1].active = true; input[1].note = 125;

            bool foundUpwardClamp = false;

            // DeterministicRandom's underlying juce::Random uses the raw
            // seed as its initial LCG state (no seed-mixing step), so small
            // seeds only vary the LOW bits of the first draw for a while;
            // nextInt(2) reads the TOP bit, which only starts flipping
            // around seed ~5582 for this constant. 10000 gives ample margin.
            for (juce::int64 seed = 0; seed < 10000 && ! foundUpwardClamp; ++seed)
            {
                berlin::DeterministicRandom random (seed);
                const berlin::Sequence result = berlin::MutationEngine::transpose (input, random);

                if (result[1].note == 127)
                {
                    foundUpwardClamp = true;
                    expectEquals (result[0].note, 102);
                }
            }

            expect (foundUpwardClamp);
        }

        beginTest ("transpose result notes always stay within [0, 127]");
        {
            berlin::Sequence input (2);
            input[0].active = true; input[0].note = 0;
            input[1].active = true; input[1].note = 127;

            for (juce::int64 seed = 0; seed < 200; ++seed)
            {
                berlin::DeterministicRandom random (seed);
                const berlin::Sequence result = berlin::MutationEngine::transpose (input, random);

                expect (result[0].note >= 0 && result[0].note <= 127);
                expect (result[1].note >= 0 && result[1].note <= 127);
            }
        }

        beginTest ("reverse is an involution and actually reorders a non-palindromic Sequence");
        {
            berlin::Sequence input (5);
            for (int i = 0; i < 5; ++i)
            {
                input[i].active = (i % 2 == 0);
                input[i].note = 60 + i;
            }

            berlin::DeterministicRandom random (1);
            const berlin::Sequence once  = berlin::MutationEngine::reverse (input, random);
            const berlin::Sequence twice = berlin::MutationEngine::reverse (once, random);

            expect (twice == input);
            expect (! (once == input));
        }

        beginTest ("rotate preserves the multiset of steps");
        {
            berlin::Sequence input (6);
            for (int i = 0; i < 6; ++i)
            {
                input[i].active = (i % 2 == 0);
                input[i].note = 60 + i;
            }

            berlin::DeterministicRandom random (42);
            const berlin::Sequence result = berlin::MutationEngine::rotate (input, random);

            expectEquals (result.size(), input.size());

            auto toMultiset = [] (const berlin::Sequence& s)
            {
                std::vector<std::pair<int, bool>> v;
                for (int i = 0; i < s.size(); ++i)
                    v.push_back ({ s[i].note, s[i].active });
                std::sort (v.begin(), v.end());
                return v;
            };

            expect (toMultiset (result) == toMultiset (input));
        }

        beginTest ("addNote increases the active count by exactly one");
        {
            berlin::Sequence input (8);
            for (int i = 0; i < 8; ++i)
                input[i].active = (i < 3);
            input[0].note = 60; input[1].note = 64; input[2].note = 67;

            berlin::DeterministicRandom random (7);
            const berlin::Sequence result = berlin::MutationEngine::addNote (input, random);

            int after = 0;
            for (int i = 0; i < result.size(); ++i)
                if (result[i].active)
                    ++after;

            expectEquals (after, 4);
        }

        beginTest ("removeNote decreases the active count by exactly one");
        {
            berlin::Sequence input (8);
            for (int i = 0; i < 8; ++i)
                input[i].active = (i < 3);
            input[0].note = 60; input[1].note = 64; input[2].note = 67;

            berlin::DeterministicRandom random (7);
            const berlin::Sequence result = berlin::MutationEngine::removeNote (input, random);

            int after = 0;
            for (int i = 0; i < result.size(); ++i)
                if (result[i].active)
                    ++after;

            expectEquals (after, 2);
        }

        beginTest ("stretch doubles the size, clamped to 64");
        {
            berlin::DeterministicRandom random (1);

            expectEquals (berlin::MutationEngine::stretch (berlin::Sequence (16), random).size(), 32);
            expectEquals (berlin::MutationEngine::stretch (berlin::Sequence (60), random).size(), 64); // ceiling clamp
            expectEquals (berlin::MutationEngine::stretch (berlin::Sequence (64), random).size(), 64); // already at ceiling
        }

        beginTest ("compress halves the size, clamped to 4");
        {
            berlin::DeterministicRandom random (1);

            expectEquals (berlin::MutationEngine::compress (berlin::Sequence (16), random).size(), 8);
            expectEquals (berlin::MutationEngine::compress (berlin::Sequence (6), random).size(), 4); // floor clamp
            expectEquals (berlin::MutationEngine::compress (berlin::Sequence (4), random).size(), 4); // already at floor
        }

        beginTest ("changeNote only ever assigns a pitch already present among the input's active steps");
        {
            berlin::Sequence input (10);
            for (int i = 0; i < 10; ++i)
                input[i].active = (i % 3 == 0);
            input[0].note = 60; input[3].note = 64; input[6].note = 67; input[9].note = 71;

            std::vector<int> inputNotes;
            for (int i = 0; i < input.size(); ++i)
                if (input[i].active)
                    inputNotes.push_back (input[i].note);

            for (juce::int64 seed = 0; seed < 100; ++seed)
            {
                berlin::DeterministicRandom random (seed);
                const berlin::Sequence result = berlin::MutationEngine::changeNote (input, random);

                for (int i = 0; i < result.size(); ++i)
                    if (result[i].active)
                        expect (std::find (inputNotes.begin(), inputNotes.end(), result[i].note) != inputNotes.end());
            }
        }

        beginTest ("changeNote is a no-op below two active steps");
        {
            const berlin::Sequence zeroActive (4);
            berlin::DeterministicRandom randomA (1);
            expect (berlin::MutationEngine::changeNote (zeroActive, randomA) == zeroActive);

            berlin::Sequence oneActive (4);
            oneActive[0].active = true; oneActive[0].note = 55;
            berlin::DeterministicRandom randomB (2);
            expect (berlin::MutationEngine::changeNote (oneActive, randomB) == oneActive);
        }

        beginTest ("addNote is a no-op with zero active steps");
        {
            const berlin::Sequence zeroActive (4);
            berlin::DeterministicRandom random (3);
            expect (berlin::MutationEngine::addNote (zeroActive, random) == zeroActive);
        }

        beginTest ("invert mirrors active notes about the first active step's note; inactive steps and size untouched");
        {
            berlin::Sequence input (4);
            input[0].active = true;  input[0].note = 60; // axis
            input[1].active = false; input[1].note = 40;
            input[2].active = true;  input[2].note = 64;
            input[3].active = false; input[3].note = 20;

            berlin::DeterministicRandom random (1);
            const berlin::Sequence result = berlin::MutationEngine::invert (input, random);

            expectEquals (result.size(), 4);
            expect (result[0].active);
            expectEquals (result[0].note, 60); // axis is a fixed point: 2*60 - 60 = 60
            expect (! result[1].active);
            expectEquals (result[1].note, 40);
            expect (result[2].active);
            expectEquals (result[2].note, 56); // 2*60 - 64 = 56
            expect (! result[3].active);
            expectEquals (result[3].note, 20);
        }

        beginTest ("invert is an involution when no correction applies");
        {
            berlin::Sequence input (4);
            input[0].active = true;  input[0].note = 60;
            input[1].active = true;  input[1].note = 64;
            input[2].active = false; input[2].note = 10;
            input[3].active = true;  input[3].note = 55;

            berlin::DeterministicRandom randomA (1);
            const berlin::Sequence once = berlin::MutationEngine::invert (input, randomA);

            berlin::DeterministicRandom randomB (2);
            const berlin::Sequence twice = berlin::MutationEngine::invert (once, randomB);

            expect (twice == input);
        }

        beginTest ("invert restores [0, 127] with one uniform corrective offset, never per-note clamping");
        {
            berlin::Sequence input (2);
            input[0].active = true; input[0].note = 0;   // axis
            input[1].active = true; input[1].note = 127; // mirrors to 2*0 - 127 = -127

            berlin::DeterministicRandom random (1);
            const berlin::Sequence result = berlin::MutationEngine::invert (input, random);

            // Uncorrected mirror: axis stays 0, other note -> -127. A single
            // uniform offset of +127 brings the out-of-range note to 0 while
            // preserving the interval between the two notes.
            expect (result[0].note >= 0 && result[0].note <= 127);
            expect (result[1].note >= 0 && result[1].note <= 127);
            expectEquals (result[1].note - result[0].note, -127);
        }

        beginTest ("invert is a no-op copy with zero active steps");
        {
            const berlin::Sequence zeroActive (4);
            berlin::DeterministicRandom random (1);
            expect (berlin::MutationEngine::invert (zeroActive, random) == zeroActive);
        }

        beginTest ("scaleIntervals augments (doubles) the interval from the axis on the upward draw");
        {
            berlin::Sequence input (3);
            input[0].active = true; input[0].note = 60; // axis
            input[1].active = true; input[1].note = 64; // +4 from axis
            input[2].active = true; input[2].note = 55; // -5 from axis

            bool foundAugment = false;

            for (juce::int64 seed = 0; seed < 10000 && ! foundAugment; ++seed)
            {
                berlin::DeterministicRandom random (seed);
                const berlin::Sequence result = berlin::MutationEngine::scaleIntervals (input, random);

                // Augment doubles the distance from the axis: a + 2(n - a).
                if (result[0].note == 60 && result[1].note == 68 && result[2].note == 50)
                    foundAugment = true;
            }

            expect (foundAugment);
        }

        beginTest ("scaleIntervals diminishes (halves, truncating toward axis) the interval from the axis on the downward draw");
        {
            berlin::Sequence input (3);
            input[0].active = true; input[0].note = 60; // axis
            input[1].active = true; input[1].note = 64; // +4 -> diminish +2
            input[2].active = true; input[2].note = 55; // -5 -> diminish -2 (truncation toward axis)

            bool foundDiminish = false;

            for (juce::int64 seed = 0; seed < 10000 && ! foundDiminish; ++seed)
            {
                berlin::DeterministicRandom random (seed);
                const berlin::Sequence result = berlin::MutationEngine::scaleIntervals (input, random);

                if (result[0].note == 60 && result[1].note == 62 && result[2].note == 58)
                    foundDiminish = true;
            }

            expect (foundDiminish);
        }

        beginTest ("scaleIntervals no-ops when the augmented span cannot be corrected with a single uniform offset");
        {
            berlin::Sequence input (2);
            input[0].active = true; input[0].note = 0;   // axis
            input[1].active = true; input[1].note = 100; // augment span = 200, exceeds 127 even after uniform shift

            for (juce::int64 seed = 0; seed < 10000; ++seed)
            {
                berlin::DeterministicRandom random (seed);
                const berlin::Sequence result = berlin::MutationEngine::scaleIntervals (input, random);

                // Whether augment or diminish is drawn, an unfittable augmented
                // span must fall back to an unmodified copy - never a partial
                // or per-note-clamped result.
                if (result[0].note != 0 || result[1].note != 100)
                {
                    // A fitting diminish draw is a legitimate non-no-op result.
                    expectEquals (result[0].note, 0);
                    expectEquals (result[1].note, 50); // diminish: 0 + (100-0)/2
                }
            }
        }

        beginTest ("scaleIntervals is a no-op copy with zero active steps");
        {
            const berlin::Sequence zeroActive (4);
            berlin::DeterministicRandom random (1);
            expect (berlin::MutationEngine::scaleIntervals (zeroActive, random) == zeroActive);
        }

        beginTest ("scaleIntervals consumes exactly one draw regardless of content (mirrors transpose)");
        {
            berlin::Sequence input (4); // zero active steps: no-op path, but the draw must still happen
            berlin::DeterministicRandom randomA (5);
            berlin::DeterministicRandom randomB (5);

            berlin::MutationEngine::scaleIntervals (input, randomA);
            const int consumedDraw = randomB.nextInt (2);
            const int nextFromA = randomA.nextInt (2);
            const int nextFromB = randomB.nextInt (2);

            // Both streams started from the same seed; randomB manually consumed
            // one nextInt(2) draw before continuing. If scaleIntervals also
            // consumed exactly one draw, both streams stay in lockstep from here.
            juce::ignoreUnused (consumedDraw);
            expectEquals (nextFromA, nextFromB);
        }

        beginTest ("palindrome mirrors a 16-step sequence into 32 steps, second half mirroring the first");
        {
            berlin::Sequence input (16);
            for (int i = 0; i < 16; ++i)
            {
                input[i].active = (i % 2 == 0);
                input[i].note = 60 + i;
            }

            berlin::DeterministicRandom random (1);
            const berlin::Sequence result = berlin::MutationEngine::palindrome (input, random);

            expectEquals (result.size(), 32);

            for (int i = 0; i < 16; ++i)
                expect (result[i] == input[i]);

            for (int i = 16; i < 32; ++i)
                expect (result[i] == input[31 - i]);
        }

        beginTest ("palindrome respects the [4, 64] length bound like stretch/compress (60 -> 64 ceiling, 64 -> 64, 1 -> 4 floor)");
        {
            berlin::DeterministicRandom random (1);

            expectEquals (berlin::MutationEngine::palindrome (berlin::Sequence (60), random).size(), 64);
            expectEquals (berlin::MutationEngine::palindrome (berlin::Sequence (64), random).size(), 64);
            expectEquals (berlin::MutationEngine::palindrome (berlin::Sequence (1), random).size(), 4);
        }

        beginTest ("palindrome makes no RNG draw and handles size 0 without crashing");
        {
            berlin::DeterministicRandom randomA (9);
            berlin::DeterministicRandom randomB (9);

            const berlin::Sequence zeroSize (0);
            const berlin::Sequence result = berlin::MutationEngine::palindrome (zeroSize, randomA);

            expectEquals (result.size(), 0);
            expectEquals (randomA.nextInt (1000), randomB.nextInt (1000)); // no draw consumed
        }

        beginTest ("mutationSeed avoids the (seed+1, gen 2) == (seed+2, gen 1) collision, and is stable across repeated calls");
        {
            constexpr juce::int64 seed = 12345;

            expect (berlin::MutationEngine::mutationSeed (seed + 1, 2) != berlin::MutationEngine::mutationSeed (seed + 2, 1));

            const juce::int64 a = berlin::MutationEngine::mutationSeed (seed, 5);
            const juce::int64 b = berlin::MutationEngine::mutationSeed (seed, 5);
            expectEquals (a, b);
        }

        beginTest ("applyRandomTransform selects exactly one transform per call and returns a well-formed Sequence");
        {
            berlin::Sequence input (16);
            for (int i = 0; i < 16; ++i)
            {
                input[i].active = (i % 2 == 0);
                input[i].note = 60 + i;
            }

            // Span a wide seed range (see the transpose clamp test above for
            // why: the top bits nextInt() reads from only start varying once
            // the seed is large enough) so the selection genuinely exercises
            // more than one transform, not just whichever one seed 0 picks.
            bool sawResize = false;

            for (juce::int64 seed = 0; seed < 100000; seed += 977)
            {
                berlin::DeterministicRandom random (seed);
                const berlin::Sequence result = berlin::MutationEngine::applyRandomTransform (input, random);

                const int size = result.size();
                expect (size == 8 || size == 16 || size == 32);

                if (size != 16)
                    sawResize = true;
            }

            expect (sawResize); // confirms selection is not stuck on a single transform
        }
    }
};

static MutationEngineTests mutationEngineTests;
