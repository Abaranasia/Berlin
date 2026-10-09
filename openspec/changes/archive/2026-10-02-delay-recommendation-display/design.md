# Design: Delay-Time Recommendation Display

Change `delay-recommendation-display` (Slice 3/5). Implements `proposal.md`, capability
`delay-time-recommendation`. Reuses Slice 2's `TempoSync` unit unchanged.

## Technical Approach

Add three pure, JUCE-free functions to `Source/core/TempoSync.h/.cpp`: a whole-ms helper, a
division-label lookup, and a string builder. The editor gets one read-only `juce::Label` row
directly under the DELAY rows. A private `updateDelayRecommendations()` sets its text from
`owner.getBpm()`. It runs on the message thread only. There is no Timer, no audio-thread change,
and no preset change.

## Architecture Decisions

| # | Decision | Alternatives rejected | Rationale |
|---|---|---|---|
| D1 | `int delayMillisecondsFor (double bpm, SyncDivision)` = `static_cast<int> (std::lround (delaySecondsFor (bpm, d) * 1000.0))` | Return `long`; fractional ms | Pinned rounding rule. The maximum is 3000, so `int` is exact. For `bpm <= 0` it inherits 0. |
| D2 | Move the label table into `TempoSync.h` as `constexpr const char* divisionLabelFor (SyncDivision)` (a switch, mirroring `factorFor`). The editor's combo box loops `kNumSyncDivisions` over it, and `kDivisionNames` is deleted from the editor `.cpp` (lines 33-37) | Keep `kDivisionNames` and duplicate it in core; pass labels in from the editor | One label table, so the combo box and the display cannot drift apart. The JUCE-free formatter needs the labels. Net diff is about -5/+12 lines. `PresetManager::divisionNames()` is a persistence key table, not display text, so it stays separate. |
| D3 | `std::string formatDelayRecommendations (double bpm)` builds the whole line | Format in the editor with `juce::String`; a per-division label grid | The exact output can be unit-tested headlessly (the editor is excluded from the test target). The grid is out of scope (Approach 2). |
| D4 | Format `"<label> <ms> ms"` joined by `" \| "`, in enum order, ASCII only | Unicode bullet or middle dot; a trailing single `ms` | ASCII avoids JUCE's non-ASCII `String(const char*)` assertion. The worst case at 40 BPM is 81 chars (about 570 px at the default 15 px font), which fits the 680 px slot. The per-item `ms` keeps each value self-describing. |
| D5 | Refresh call sites: `pushTempoFromWidgets()` (after `recomputeSyncedDelayTime()`) and the end of `refreshFromProcessor()` | Constructor-only plus the tempo slider | **Gap found:** preset load and `setStateInformation` call `owner.setBpm` (`BerlinAudioProcessor.cpp:153,228`), then `refreshFromProcessor` sets `tempoSlider` with `dontSendNotification`. Without D5 the display would go stale. The constructor already calls `refreshFromProcessor()` (line 464), so this placement satisfies the pinned "constructor" call site. |
| D6 | Not added to `delayReverbWidgets` and not touched by `updateDelayReverbEnablement()` | Grey it out with FX | Pinned decision. The values are for external units too. |
| D7 | Single label, indented by `kLabelWidth` under the DELAY column, no caption widget | Separate caption label | One widget, and it aligns with the DELAY row content. The text itself shows what it is. |

## Data Flow

    tempoSlider ─> pushTempoFromWidgets ─> owner.setBpm
                                       ├─> recomputeSyncedDelayTime   (unchanged)
                                       └─> updateDelayRecommendations
    state restore ─> sendChangeMessage ─> refreshFromProcessor ─> updateDelayRecommendations
    preset load (editor loadSelectedPreset) ─> refreshFromProcessor ─> updateDelayRecommendations
    updateDelayRecommendations: owner.getBpm() ─> formatDelayRecommendations ─> delayRecommendationLabel.setText

## File Changes

| File | Action | Description |
|---|---|---|
| `Source/core/TempoSync.h` | Modify | `#include <string>`; `divisionLabelFor`; declarations of `delayMillisecondsFor` and `formatDelayRecommendations` |
| `Source/core/TempoSync.cpp` | Modify | `<cmath>`; definitions (loop `0..kNumSyncDivisions-1`, `std::to_string`) |
| `Source/plugin/BerlinAudioProcessorEditor.h` | Modify | `juce::Label delayRecommendationLabel;` and `void updateDelayRecommendations();` |
| `Source/plugin/BerlinAudioProcessorEditor.cpp` | Modify | Remove `kDivisionNames`; combo box uses `divisionLabelFor`; set up the label after `delayMixSlider` (line ~127); two call sites (D5); `resized()` row; `setSize` changes from 7 to 8 rows |
| `Tests/Source/TempoSyncTests.cpp` | Modify | New RED tests (below) |

No `.jucer` change: no new files are added.

## Interfaces / Contracts

```cpp
// TempoSync.h (JUCE-free)
constexpr const char* divisionLabelFor (SyncDivision) noexcept;  // "1/2","1/4","1/8.","1/8","1/8T","1/16"
int         delayMillisecondsFor (double bpm, SyncDivision) noexcept; // lround(sec*1000); bpm<=0 -> 0
std::string formatDelayRecommendations (double bpm);
// 160 BPM -> "1/2 750 ms | 1/4 375 ms | 1/8. 281 ms | 1/8 188 ms | 1/8T 125 ms | 1/16 94 ms"
```

Layout: insert after `delayRow2` (`resized()` line 544), before `reverbRow1`:

```cpp
auto delayRow3 = area.removeFromTop (kControlHeight);
delayRow3.removeFromLeft (kLabelWidth);
delayRecommendationLabel.setBounds (delayRow3);
area.removeFromTop (kMargin / 2);
```

`setSize (800, 680 + 8 * (kControlHeight + kMargin / 2));` adds 34 px. Update the comment too.

## Testing Strategy

| Layer | What | Approach |
|---|---|---|
| Unit (RED first) | `delayMillisecondsFor`: 160 BPM gives 750/375/281/188/125/94; 120 BPM gives 1000/500/375/250/167/125; 240 BPM gives 500/250/188/125/83/63; 40 BPM half gives 3000; `bpm <= 0` gives 0 | `expectEquals` on ints. 187.5 and 62.5 are exact in binary, so these check half-away rounding. |
| Unit | `divisionLabelFor` returns all 6 labels in enum order | String compare |
| Unit | `formatDelayRecommendations(160)` and `(40)` match exact strings, with `"3000 ms"` (no seconds unit) | `expectEquals (juce::String (...), ...)` |
| Manual | Row visible and not clipped; updates on drag and on preset load; visible with FX off and in Free mode; no "correct" framing | Review gate. The editor is excluded from tests by design (editor header lines 8-12). |

## Threat Matrix

N/A: no routing, shell, subprocess, VCS/PR automation, executable-file classification, or
process-integration boundary.

## Migration / Rollout

No migration required. Purely additive, with no schema or audio-path change.

## Size Estimate

About 130 changed lines: TempoSync +35, editor +25/-8, header +4, tests +60. This is well under 800.

## Open Questions

- [x] D5 extends the pinned call sites with `refreshFromProcessor()` (superset, covers the constructor). Resolved: spec now has "Display refreshes after a preset or state restores BPM".
- [ ] Pre-existing: the standalone `MainComponent` is fixed at `setSize (800, 680)` (`MainComponent.cpp:9`), but the editor requests 918 px and soon 952 px. The bottom rows in standalone may already be clipped. This is out of scope, but it is recorded for manual verification.
