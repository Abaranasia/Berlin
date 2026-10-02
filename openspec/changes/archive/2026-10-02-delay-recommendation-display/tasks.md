# Tasks: Delay-Time Recommendation Display

## Review Workload Forecast

| Field | Value |
|---|---|
| Estimated changed lines | ~130-160 (TempoSync +35, editor +25/-8, header +4, tests +60) |
| 400-line budget risk | Low |
| Chained PRs recommended | No |
| Suggested split | Single PR |
| Delivery strategy | single-pr-default |
| Chain strategy | size-exception |

Decision needed before apply: No
Chained PRs recommended: No
Chain strategy: size-exception
400-line budget risk: Low

### Suggested Work Units

| Unit | Goal | Likely PR | Focused test command | Runtime harness | Rollback boundary |
|---|---|---|---|---|---|
| 1 | TempoSync helpers + editor wiring + layout, full suite green | Single PR | `Tests/Builds/VisualStudio2026/x64/Debug/ConsoleApp/BerlinTests.exe --category=Berlin --test=TempoSync` | Manual: open plugin editor, drag tempo slider, load a preset, toggle FX off, toggle Sync/Free | Revert the single commit; `TempoSync.h/.cpp` additions and editor row are additive, no schema/audio-path change |

## Phase 0: Build Registration Check (blocking, do first)

- [x] 0.1 Confirm no `.jucer` change is needed: `TempoSync.h/.cpp` already registered in `Berlin.jucer` (lines 18-19) and `Tests/BerlinTests.jucer` (lines 62-63, 75-76); no new files this slice.
- [x] 0.2 Build test target: `MSBuild BerlinTests.sln /p:Configuration=Debug /p:Platform=x64 /t:Rebuild` (0 errors/0 warnings baseline, per Slice 2 verify report).
- [x] 0.3 Run baseline: `Tests/Builds/VisualStudio2026/x64/Debug/ConsoleApp/BerlinTests.exe --category=Berlin` passes green before any edit (regression safety net). Actual: 311/311 passing.

## Phase 1: TempoSync Unit — RED→GREEN (delay-time-recommendation spec, Req 1-2)

- [x] 1.1 RED — `Tests/Source/TempoSyncTests.cpp`: `delayMillisecondsFor` at 160 BPM returns 750/375/281/188/125/94 (enum order); at 120 BPM returns 1000/500/375/250/167/125; at 240 BPM returns 500/250/188/125/83/63; at 40 BPM `half` returns 3000; `bpm<=0` returns 0 for every division.
- [x] 1.2 RED — `divisionLabelFor` returns the 6 labels "1/2","1/4","1/8.","1/8","1/8T","1/16" in enum order.
- [x] 1.3 RED — `formatDelayRecommendations(160)` equals exactly `"1/2 750 ms | 1/4 375 ms | 1/8. 281 ms | 1/8 188 ms | 1/8T 125 ms | 1/16 94 ms"`; `formatDelayRecommendations(40)` equals the full exact 40 BPM string (stronger than the "contains" floor; no seconds unit).
- [x] 1.4 Confirm all 1.1-1.3 tests fail to compile/fail-assert (RED proven) before writing production code. Actual: 3 x C2039/C3861 compile errors for `delayMillisecondsFor`/`divisionLabelFor`/`formatDelayRecommendations` not being members of `berlin`.
- [x] 1.5 GREEN — `Source/core/TempoSync.h`: add `#include <string>`; declare `constexpr const char* divisionLabelFor (SyncDivision) noexcept` (switch mirroring `factorFor`, fallback return); declare `int delayMillisecondsFor (double bpm, SyncDivision) noexcept`; declare `std::string formatDelayRecommendations (double bpm)`.
- [x] 1.6 GREEN — `Source/core/TempoSync.cpp`: add `#include <cmath>`; define `delayMillisecondsFor` = `static_cast<int>(std::lround(delaySecondsFor(bpm,d)*1000.0))`; define `formatDelayRecommendations` looping `0..kNumSyncDivisions-1`, joining `"<label> <ms> ms"` with `" | "` via `std::to_string`.
- [x] 1.7 Run `BerlinTests.exe --name=TempoSync` — confirm 1.1-1.3 now pass (GREEN). Actual: 14/14 TempoSync tests pass. NOTE: the task's suggested `--category=Berlin --test=TempoSync` flag does not exist in this harness; the real filter flag is `--name` (see `Tests/Source/Main.cpp`), confirmed by running `BerlinTests.exe --help`-equivalent code read.

## Phase 2: Editor Wiring — Label Table Consolidation (design D2)

- [x] 2.1 Delete `kDivisionNames` array from `Source/plugin/BerlinAudioProcessorEditor.cpp` (lines 33-37).
- [x] 2.2 Update the combo-box population loop (`.cpp` ~lines 94-96) to call `berlin::divisionLabelFor(static_cast<SyncDivision>(i))` instead of indexing the deleted array, looping `0..kNumSyncDivisions-1`; behavior (item text/order/ids) unchanged.
- [x] 2.3 Build and run `BerlinTests.exe --category=Berlin` — confirm no regression (editor is excluded from the test target, so this is a build-only check via `Berlin.sln`). Actual: done together with Phase 5's full rebuild (Berlin.sln clean, BerlinTests green).

## Phase 3: Editor Wiring — Display Label + Refresh (design D5-D7)

- [x] 3.1 `Source/plugin/BerlinAudioProcessorEditor.h`: add `juce::Label delayRecommendationLabel;` member and `void updateDelayRecommendations();` private method declaration.
- [x] 3.2 `Source/plugin/BerlinAudioProcessorEditor.cpp` constructor: after the `delayMixSlider` setup (~line 127), add `addAndMakeVisible (delayRecommendationLabel);` (no "correct/recommended" styling, per D6/spec Req 1).
- [x] 3.3 `Source/plugin/BerlinAudioProcessorEditor.cpp`: implement `updateDelayRecommendations()` — sets `delayRecommendationLabel.setText (berlin::formatDelayRecommendations (owner.getBpm()), juce::dontSendNotification);`.
- [x] 3.4 Call site: in `pushTempoFromWidgets()` (~line 731-735), call `updateDelayRecommendations();` after `recomputeSyncedDelayTime();`.
- [x] 3.5 Call site: at the end of `refreshFromProcessor()` (~line 488-500), call `updateDelayRecommendations();` as the last statement (covers constructor's existing `refreshFromProcessor()` call at line 464, `setStateInformation`, and preset load).
- [x] 3.6 Confirm `delayRecommendationLabel` is NOT added to `delayReverbWidgets` and NOT touched by `updateDelayReverbEnablement()` (D6 — stays visible regardless of FX toggle). Verified by diff review: `delayReverbWidgets` initializer list and `updateDelayReverbEnablement()` body untouched.

## Phase 4: Layout — New Row + Height Bump

- [x] 4.1 `resized()` (~line 544, after `delayRow2`'s `area.removeFromTop (kMargin / 2);`, before `reverbRow1`): insert `delayRow3` block — `auto delayRow3 = area.removeFromTop (kControlHeight); delayRow3.removeFromLeft (kLabelWidth); delayRecommendationLabel.setBounds (delayRow3); area.removeFromTop (kMargin / 2);`.
- [x] 4.2 Constructor `setSize` call (line 473): change `7 * (kControlHeight + kMargin / 2)` to `8 * (kControlHeight + kMargin / 2)`; update the preceding row-count comment to mention the new delay-recommendation row (+1 row, +34px).

## Phase 5: Full-Suite Regression Gate

- [x] 5.1 Build: `MSBuild BerlinTests.sln /p:Configuration=Debug /p:Platform=x64 /t:Rebuild` — 0 errors, 0 warnings. Actual: clean rebuild, 0 errors.
- [x] 5.2 Build: confirm `Berlin.sln` (standalone app, exercises the editor `.cpp` changes) also builds clean. Actual: clean rebuild (`Berlin_App.vcxproj` -> `Berlin.exe`), 0 errors; 2 pre-existing unrelated warnings in `Source/Main.cpp` (unreferenced `commandLine` parameter, not touched by this slice).
- [x] 5.3 Run full suite: `Tests/Builds/VisualStudio2026/x64/Debug/ConsoleApp/BerlinTests.exe --category=Berlin` — all tests green, including Phase 1's new cases and the pre-existing `TempoSyncTests.cpp`/`TransportTests.cpp`/etc. regression set. Actual: 320/320 passing (311 baseline + 9 new), exit code 0, "All tests completed successfully".
- [x] 5.4 Code-review gate: confirm the refresh path (Phase 3) contains no `juce::Timer`, no polling loop, no audio-thread change, no preset/schema change (spec Req 3's "No timer or audio-thread involvement" scenario). Actual: `grep -n Timer` across both editor and TempoSync files returns only a doc-comment mentioning "never on a Timer"; no `.jucer`/`PresetManager.cpp` diff; `updateDelayRecommendations()` only touches a `juce::Label` on the message thread.

## Phase 6: Manual Verification (human-only — flag clearly, not automatable)

- [x] 6.1 **MANUAL**: Open the plugin editor (not just standalone) — confirm the new row is visible and not clipped at the bottom of the window. _(Verified 2026-10-02 by the user in the standalone app (Berlin.exe x64 Debug); not separately checked inside a plugin host.)_
- [x] 6.2 **MANUAL, flag for follow-up**: Check the pre-existing standalone `MainComponent` fixed `setSize(800,680)` (`MainComponent.cpp:9`) against the editor's new request (952px after this slice's +34px) — bottom rows (including this new one) may already be clipped in the standalone app; this is a pre-existing out-of-scope condition, not introduced by this slice, but worth confirming/flagging at review time. _(Verified 2026-10-02: user confirmed clipping at the default standalone size, recoverable by enlarging the window; flagged as a follow-up fix, also raised by the review resilience/reliability lenses.)_
- [x] 6.3 **MANUAL**: Drag the tempo slider — confirm the recommendation row updates live with no additional action. _(Verified 2026-10-02 by the user.)_
- [x] 6.4 **MANUAL**: Load a preset (or restore state) that changes BPM — confirm the row updates to the new BPM's values even though `tempoSlider` is set with `dontSendNotification` (spec Req 3's preset/state-restore scenario). _(Verified 2026-10-02 by the user.)_
- [x] 6.5 **MANUAL**: Toggle internal FX off — confirm the row stays visible and correct (spec Req 4). _(Verified 2026-10-02 by the user.)_
- [x] 6.6 **MANUAL**: Toggle delay Sync/Free to Free — confirm the row stays visible and correct, independent of the manually-set delay time (spec Req 4). _(Verified 2026-10-02 by the user.)_
- [x] 6.7 **MANUAL**: Visually confirm no division is marked/highlighted as "correct" or "recommended" (spec Req 1). _(Verified 2026-10-02 by the user.)_

Note: Threat matrix is N/A for this change (no routing/shell/subprocess/VCS/process-integration boundary) — no RED tests owed beyond Phase 1's unit cases already listed.
