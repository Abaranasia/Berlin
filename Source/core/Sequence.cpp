/*
  ==============================================================================

   Sequence - out-of-line definitions (sequencing-core spec).

  ==============================================================================
*/

#include "Sequence.h"

namespace berlin
{

Sequence::Sequence (int numSteps)
    : steps (static_cast<std::size_t> (numSteps))
{
}

int Sequence::size() const noexcept
{
    return static_cast<int> (steps.size());
}

void Sequence::resize (int numSteps)
{
    steps.resize (static_cast<std::size_t> (numSteps));
}

Step& Sequence::operator[] (int index)
{
    return steps[static_cast<std::size_t> (index)];
}

const Step& Sequence::operator[] (int index) const
{
    return steps[static_cast<std::size_t> (index)];
}

bool operator== (const Sequence& lhs, const Sequence& rhs)
{
    if (lhs.size() != rhs.size())
        return false;

    for (int i = 0; i < lhs.size(); ++i)
        if (! (lhs[i] == rhs[i]))
            return false;

    return true;
}

} // namespace berlin
