/*
  ==============================================================================

   TempoSync - out-of-line definitions (tempo-control spec).

  ==============================================================================
*/

#include "TempoSync.h"

#include <cmath>

namespace berlin
{

double delaySecondsFor (double bpm, SyncDivision division) noexcept
{
    if (bpm <= 0.0)
        return 0.0;

    return (60.0 / bpm) * factorFor (division);
}

int delayMillisecondsFor (double bpm, SyncDivision division) noexcept
{
    return static_cast<int> (std::lround (delaySecondsFor (bpm, division) * 1000.0));
}

std::string formatDelayRecommendations (double bpm)
{
    std::string result;

    for (int i = 0; i < kNumSyncDivisions; ++i)
    {
        const auto division = static_cast<SyncDivision> (i);

        if (i > 0)
            result += " | ";

        result += divisionLabelFor (division);
        result += ' ';
        result += std::to_string (delayMillisecondsFor (bpm, division));
        result += " ms";
    }

    return result;
}

} // namespace berlin
