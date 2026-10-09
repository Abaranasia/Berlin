/*
  ==============================================================================

   StepEdit - pure validation and the editable note list for per-step edits
   (step-editing spec, design.md D3). Standard library plus GenerationParams
   only: no processor, no threads, no JUCE types.

   scaleNotesFor uses the same normalizePitchRange as buildSeededSequence, so
   the editable note set is exactly what generation can produce for the same
   scale, root and range. The span is >= 12, so the list is never empty.

  ==============================================================================
*/

#pragma once

#include <vector>

#include "core/Sequence.h"
#include "generation/GenerationParams.h"

namespace berlin
{

// Only the 16 visible steps are editable (clamped further by the live size).
inline constexpr int kEditableSteps = 16;

enum class StepEditError { none, stepOutOfRange, noteOutOfScale };

// Ascending in-scale MIDI notes within the normalized pitch range.
std::vector<int> scaleNotesFor (const GenerationParams& params);

// Index is checked first, then the note. A muted step keeps its note, so the
// gate is not part of validation.
StepEditError validateStepEdit (const Sequence& sequence, const std::vector<int>& scaleNotes,
                                int index, int note) noexcept;

} // namespace berlin
