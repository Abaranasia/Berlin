/*
  ==============================================================================

   StepEdit - out-of-line definitions (step-editing spec, design.md D3).

  ==============================================================================
*/

#include "StepEdit.h"

#include <algorithm>

#include "core/Scale.h"
#include "generation/SequenceBuilder.h"

namespace berlin
{

std::vector<int> scaleNotesFor (const GenerationParams& params)
{
    int low  = params.rangeLow;
    int high = params.rangeHigh;
    normalizePitchRange (low, high);   // same normalization as buildSeededSequence

    const auto scale = Scale::fromPitchClass (params.scaleType, params.rootPitchClass);

    std::vector<int> notes;
    for (int note = low; note <= high; ++note)
        if (scale.contains (note))
            notes.push_back (note);

    return notes;
}

StepEditError validateStepEdit (const Sequence& sequence, const std::vector<int>& scaleNotes,
                                int index, int note) noexcept
{
    if (index < 0 || index >= kEditableSteps || index >= sequence.size())
        return StepEditError::stepOutOfRange;

    // Ascending list: binary search keeps this allocation-free.
    if (! std::binary_search (scaleNotes.begin(), scaleNotes.end(), note))
        return StepEditError::noteOutOfScale;

    return StepEditError::none;
}

} // namespace berlin
