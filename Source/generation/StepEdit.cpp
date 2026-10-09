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

    // The step's current note is always accepted (so mute still works after a
    // scale or range change); a changed note must be in scale. Ascending list:
    // binary search keeps this allocation-free.
    if (note != sequence[index].note
        && ! std::binary_search (scaleNotes.begin(), scaleNotes.end(), note))
        return StepEditError::noteOutOfScale;

    return StepEditError::none;
}

} // namespace berlin
