/*
  ==============================================================================

   Sequence - a resizable ordered collection of Step values (sequencing-core
   spec).

   JUCE-free: standard library only. Backed by std::vector<Step>, kept private
   so the container is an implementation detail (design.md's rationale: keeps
   Source/core/ JUCE-free and leaves room to swap the container later without
   a breaking change). Definitions live in Sequence.cpp so a forgotten <FILE>
   registration in either .jucer project fails loudly as an unresolved-
   external link error, per design.md's ".h/.cpp for types with out-of-line
   definitions" decision.

  ==============================================================================
*/

#pragma once

#include <vector>

#include "Step.h"

namespace berlin
{

class Sequence
{
public:
    Sequence() = default;
    explicit Sequence (int numSteps);          // steps default-constructed

    int  size() const noexcept;
    void resize (int numSteps);                // grow pads with default Step, shrink truncates

    Step&       operator[] (int index);        // precondition: 0 <= index < size()
    const Step& operator[] (int index) const;

private:
    std::vector<Step> steps;
};

bool operator== (const Sequence& lhs, const Sequence& rhs);

} // namespace berlin
