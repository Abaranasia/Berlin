/*
  ==============================================================================

   EuclideanRhythmGenerator - out-of-line definitions (euclidean-rhythm spec).

   Bjorklund's algorithm by group pairing (design.md Interfaces/Contracts):
   build `pulses` groups of [1] and `numSteps - pulses` groups of [0], then
   repeatedly pair min(a.size(), b.size()) elements by appending each B-group
   onto the corresponding A-group, tracking leftovers, until b.size() <= 1;
   flatten A then B into the final pattern; then apply rotation.

  ==============================================================================
*/

#include "EuclideanRhythmGenerator.h"

#include <algorithm>
#include <vector>

namespace berlin
{

EuclideanRhythmGenerator::EuclideanRhythmGenerator (int numStepsIn, int pulsesIn, int rotationIn)
    : numSteps (numStepsIn),
      pulses   (std::clamp (pulsesIn, 0, numStepsIn)),
      rotation (numStepsIn > 0 ? ((rotationIn % numStepsIn) + numStepsIn) % numStepsIn : 0)
{
}

Sequence EuclideanRhythmGenerator::generate() const
{
    Sequence sequence (numSteps);            // Step defaults to { 0, false }

    if (numSteps <= 0 || pulses <= 0)
        return sequence;                     // LOAD-BEARING: with pulses == 0 the pairing
                                             // count below is 0, so B never shrinks -> infinite loop

    // Bjorklund by group pairing. Each round appends one B-group to the end of
    // each A-group; whichever side had leftovers becomes the new B. Terminates
    // because (|A|,|B|) follows the subtractive Euclidean algorithm.
    std::vector<std::vector<char>> a ((std::size_t) pulses,            std::vector<char> { 1 });
    std::vector<std::vector<char>> b ((std::size_t) (numSteps - pulses), std::vector<char> { 0 });

    while (b.size() > 1)
    {
        const std::size_t pairs = std::min (a.size(), b.size());

        for (std::size_t i = 0; i < pairs; ++i)
            a[i].insert (a[i].end(), b[i].begin(), b[i].end());

        std::vector<std::vector<char>> leftovers;
        if (a.size() > pairs) leftovers.assign (a.begin() + pairs, a.end());   // assign BEFORE resize
        else                  leftovers.assign (b.begin() + pairs, b.end());

        a.resize (pairs);
        b = std::move (leftovers);
    }

    std::vector<char> pattern;
    pattern.reserve ((std::size_t) numSteps);
    for (const auto& g : a) pattern.insert (pattern.end(), g.begin(), g.end());
    for (const auto& g : b) pattern.insert (pattern.end(), g.begin(), g.end());

    for (int i = 0; i < numSteps; ++i)       // rotate LEFT
        sequence[i].active = pattern[(std::size_t) ((i + rotation) % numSteps)] != 0;

    return sequence;
}

} // namespace berlin
