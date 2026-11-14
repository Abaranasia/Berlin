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

#include <optional>
#include <vector>

#include "core/Sequence.h"
#include "generation/GenerationParams.h"

namespace berlin
{

enum class StepEditError { none, stepOutOfRange, noteOutOfScale, velocityOutOfRange, gateOutOfRange };

// Ascending in-scale MIDI notes within the normalized pitch range.
std::vector<int> scaleNotesFor (const GenerationParams& params);

// Index is checked first, then the note, then the velocity (1-127 when
// present; nullopt keeps the step's velocity), then the gate length (1-100 when
// present; nullopt keeps it). A muted step keeps its note, so the on/off flag is
// not part of validation. A note equal to the step's current note is
// always accepted, even if it is no longer in scaleNotes.
StepEditError validateStepEdit (const Sequence& sequence, const std::vector<int>& scaleNotes,
                                int index, int note,
                                std::optional<int> velocity = {},
                                std::optional<int> gatePercent = {}) noexcept;

} // namespace berlin
