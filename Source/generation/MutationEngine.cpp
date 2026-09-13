/*
  ==============================================================================

   MutationEngine - out-of-line definitions (evolution-mutation-engine spec,
   Manual Mutate, Slice 1).

  ==============================================================================
*/

#include "MutationEngine.h"

#include <algorithm>
#include <cstddef>
#include <vector>

namespace berlin::MutationEngine
{

Sequence transpose (const Sequence& input, DeterministicRandom& random)
{
    // Exactly one draw, always, regardless of whether there is an active
    // step to shift - keeps the RNG stream length independent of content
    // (mirrors RhythmGenerator's "one draw regardless" invariant).
    const bool wantsUpward = random.nextInt (2) == 1;

    Sequence result = input;

    int minNote = 0, maxNote = 0;
    bool hasActive = false;

    for (int i = 0; i < input.size(); ++i)
    {
        if (! input[i].active)
            continue;

        if (! hasActive)
        {
            minNote = maxNote = input[i].note;
            hasActive = true;
        }
        else
        {
            minNote = std::min (minNote, input[i].note);
            maxNote = std::max (maxNote, input[i].note);
        }
    }

    if (! hasActive)
        return result; // nothing to shift; inactive steps are already untouched

    // Clamp the OFFSET once against the active-step min/max (design.md
    // Decision 7) - never per-note, which would compress/expand intervals.
    const int requestedOffset = wantsUpward ? 12 : -12;
    const int offset = wantsUpward
        ? std::min (requestedOffset, 127 - maxNote)
        : std::max (requestedOffset, -minNote);

    for (int i = 0; i < result.size(); ++i)
        if (result[i].active)
            result[i].note += offset;

    return result;
}

Sequence reverse (const Sequence& input, DeterministicRandom&)
{
    Sequence result (input.size());
    for (int i = 0; i < input.size(); ++i)
        result[i] = input[input.size() - 1 - i];
    return result;
}

Sequence rotate (const Sequence& input, DeterministicRandom& random)
{
    const int n = input.size();
    if (n < 2)
        return input;

    const int k = 1 + random.nextInt (n - 1); // shift in [1, n-1]

    Sequence result (n);
    for (int i = 0; i < n; ++i)
        result[i] = input[(i + k) % n];

    return result;
}

Sequence changeNote (const Sequence& input, DeterministicRandom& random)
{
    std::vector<int> activeIndices;
    for (int i = 0; i < input.size(); ++i)
        if (input[i].active)
            activeIndices.push_back (i);

    if (activeIndices.size() < 2)
        return input; // no second in-pattern pitch to copy from

    const int targetPos = random.nextInt ((int) activeIndices.size());
    int sourcePos = random.nextInt ((int) activeIndices.size() - 1);
    if (sourcePos >= targetPos)
        ++sourcePos; // skip re-picking the same index

    Sequence result = input;
    result[activeIndices[(std::size_t) targetPos]].note =
        input[activeIndices[(std::size_t) sourcePos]].note;

    return result;
}

Sequence addNote (const Sequence& input, DeterministicRandom& random)
{
    std::vector<int> activeIndices, inactiveIndices;
    for (int i = 0; i < input.size(); ++i)
        (input[i].active ? activeIndices : inactiveIndices).push_back (i);

    if (activeIndices.empty() || inactiveIndices.empty())
        return input; // no in-pattern pitch to copy, or nothing left to activate

    const int inactivePos = random.nextInt ((int) inactiveIndices.size());
    const int activePos   = random.nextInt ((int) activeIndices.size());

    Sequence result = input;
    const int targetIndex = inactiveIndices[(std::size_t) inactivePos];
    result[targetIndex].active = true;
    result[targetIndex].note = input[activeIndices[(std::size_t) activePos]].note;

    return result;
}

Sequence removeNote (const Sequence& input, DeterministicRandom& random)
{
    std::vector<int> activeIndices;
    for (int i = 0; i < input.size(); ++i)
        if (input[i].active)
            activeIndices.push_back (i);

    if (activeIndices.empty())
        return input;

    const int pos = random.nextInt ((int) activeIndices.size());

    Sequence result = input;
    result[activeIndices[(std::size_t) pos]].active = false;

    return result;
}

Sequence stretch (const Sequence& input, DeterministicRandom&)
{
    const int n = input.size();
    if (n == 0)
        return input; // no step to duplicate

    // Duplicate each step in order; when the doubled size falls outside
    // [4, 64] the target size is clamped and the (i / 2) % n index wraps,
    // which reduces to a clean, ordered duplication in the un-clamped case
    // and a well-formed (if partial) result at either bound.
    const int targetSize = std::clamp (n * 2, 4, 64);
    Sequence result (targetSize);
    for (int i = 0; i < targetSize; ++i)
        result[i] = input[(i / 2) % n];

    return result;
}

Sequence compress (const Sequence& input, DeterministicRandom&)
{
    const int n = input.size();
    if (n == 0)
        return input; // no step to keep

    // Keep every other step; when the halved size falls outside [4, 64] the
    // target size is clamped and the (2 * i) % n index wraps, which reduces
    // to a clean "every other step" selection in the un-clamped case and a
    // well-formed (if partial) result at either bound.
    const int targetSize = std::clamp (n / 2, 4, 64);
    Sequence result (targetSize);
    for (int i = 0; i < targetSize; ++i)
        result[i] = input[(2 * i) % n];

    return result;
}

Sequence applyRandomTransform (const Sequence& input, DeterministicRandom& random)
{
    // Exactly ONE selection draw, always first (design.md Decision 2), then
    // delegate to the chosen transform's own draws.
    static constexpr Transform transforms[] = {
        transpose, reverse, rotate, changeNote, addNote, removeNote, stretch, compress
    };

    const int index = random.nextInt ((int) (sizeof (transforms) / sizeof (transforms[0])));
    return transforms[index] (input, random);
}

juce::int64 mutationSeed (juce::int64 baseSeed, int generation) noexcept
{
    // splitmix64 finalisation - plain addition would collide, e.g.
    // (seed+1, gen 2) == (seed+2, gen 1) (design.md).
    auto x = (juce::uint64) baseSeed + 0x9E3779B97F4A7C15ull * (juce::uint64) (generation + 1);
    x ^= x >> 30; x *= 0xBF58476D1CE4E5B9ull;
    x ^= x >> 27; x *= 0x94D049BB133111EBull;
    return (juce::int64) (x ^ (x >> 31));
}

} // namespace berlin::MutationEngine
