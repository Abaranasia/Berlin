/*
  ==============================================================================

   TempoSync - JUCE-free (bpm, division) -> seconds conversion (tempo-control
   spec, design.md Interfaces block). Feeds SynthPatch.delayTimeSeconds when a
   delay is synced to the transport tempo instead of set manually (Free mode).

   Declaration order of SyncDivision is PERSISTED via PresetManager's
   divisionNames() table (design.md Decision 7) - NEVER reorder or insert
   enumerators without a corresponding preset schema version bump.

  ==============================================================================
*/

#pragma once

#include <string>

namespace berlin
{

enum class SyncDivision
{
    half,
    quarter,
    dottedEighth,
    eighth,
    eighthTriplet,
    sixteenth
};

inline constexpr int kNumSyncDivisions = 6;

// Beats-per-repeat factor for each division: half=2, quarter=1,
// dottedEighth=0.75, eighth=0.5, eighthTriplet=1/3, sixteenth=0.25.
constexpr double factorFor (SyncDivision division) noexcept
{
    switch (division)
    {
        case SyncDivision::half:          return 2.0;
        case SyncDivision::quarter:       return 1.0;
        case SyncDivision::dottedEighth:  return 0.75;
        case SyncDivision::eighth:        return 0.5;
        case SyncDivision::eighthTriplet: return 1.0 / 3.0;
        case SyncDivision::sixteenth:     return 0.25;
    }

    return 1.0;   // unreachable for a valid enumerator; keeps the function total
}

// seconds = (60 / bpm) * factorFor(division). bpm <= 0 -> 0 (no division by zero).
double delaySecondsFor (double bpm, SyncDivision division) noexcept;

// ---- delay-time-recommendation spec (Slice 3/5): display helpers layered on
// top of delaySecondsFor - JUCE-free, unit-testable (design.md D1-D4). ----

// UI display label for each division. Order MUST match SyncDivision's
// declaration order (same precedent as factorFor's switch above) - NOT the
// same table as PresetManager::divisionNames() (persistence keys stay
// separate from UI labels).
constexpr const char* divisionLabelFor (SyncDivision division) noexcept
{
    switch (division)
    {
        case SyncDivision::half:          return "1/2";
        case SyncDivision::quarter:       return "1/4";
        case SyncDivision::dottedEighth:  return "1/8.";
        case SyncDivision::eighth:        return "1/8";
        case SyncDivision::eighthTriplet: return "1/8T";
        case SyncDivision::sixteenth:     return "1/16";
    }

    return "";   // unreachable for a valid enumerator; keeps the function total
}

// Whole-millisecond rounding of delaySecondsFor (half away from zero, design.md
// D1). bpm <= 0 -> 0, inherited from delaySecondsFor's own guard.
int delayMillisecondsFor (double bpm, SyncDivision division) noexcept;

// Formats all kNumSyncDivisions divisions' delay times at the given BPM as
// "<label> <ms> ms" joined by " | ", in enum order (design.md D3-D4). Not
// noexcept: builds a std::string via std::to_string/operator+.
std::string formatDelayRecommendations (double bpm);

} // namespace berlin
