/*
  ==============================================================================

   TempoSync - out-of-line definitions (tempo-control spec).

  ==============================================================================
*/

#include "TempoSync.h"

namespace berlin
{

double delaySecondsFor (double bpm, SyncDivision division) noexcept
{
    if (bpm <= 0.0)
        return 0.0;

    return (60.0 / bpm) * factorFor (division);
}

} // namespace berlin
