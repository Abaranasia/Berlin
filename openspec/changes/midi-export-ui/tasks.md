# Tasks: MIDI Export UI

## Review Workload Forecast

| Field | Value |
|-------|-------|
| Estimated changed lines | ~150-190 (additions+deletions; 2 files, no new files) |
| 400-line budget risk | Low |
| Chained PRs recommended | No |
| Suggested split | Single PR |
| Delivery strategy | single-pr |
| Chain strategy | pending (N/A — estimate is well under 400, no exception needed) |

Decision needed before apply: No
Chained PRs recommended: No
Chain strategy: pending
400-line budget risk: Low

Single-PR holds: confirmed. Diff is confined to `Source/MainComponent.h` (+3 members, +2 method decls, 1 rename) and `Source/MainComponent.cpp` (~19-line deletion, ~90-line net addition). No new files, no `.jucer`/Projucer regeneration, no test-file changes. No chaining decision is required.

### Suggested Work Units

| Unit | Goal | Likely PR | Focused test command | Runtime harness | Rollback boundary |
|------|------|-----------|----------------------|-----------------|-------------------|
| 1 | Add export button + async chooser + extracted method, remove ctor hook | PR 1 (only PR) | `BerlinTests.exe --category=Berlin` (regression, no new coverage reachable) | Manual: launch app, click "Export MIDI...", export to real path, open `.mid` in a DAW | `git revert` restores `Source/MainComponent.h/.cpp` to Phase 6 state; no other tier touched |

## Phase 1: Header Member Additions (`Source/MainComponent.h`)

- [x] 1.1 Add private members `juce::TextButton exportButton { "Export MIDI..." };` and `juce::Label statusLabel;` after the existing domain members.
- [x] 1.2 Add private member `std::unique_ptr<juce::FileChooser> exportChooser;`.
- [x] 1.3 Add private method declarations `void launchExportChooser();` and `void exportSequenceTo (const juce::File& destination);`.
- [x] 1.4 Rename `static juce::File resolveExportFile();` (line 36) to `static juce::File defaultExportFile();`; replace the `// TEMPORARY hook - deleted in Phase 7` comment with `// default destination seeded into the save dialog`.

## Phase 2: Extracted Export Method + Flag Fix (`Source/MainComponent.cpp`)

- [x] 2.1 Rename `MainComponent::resolveExportFile()` definition (lines 35-40) to `defaultExportFile()`.
- [x] 2.2 Add file-local `describeWriteFailure (berlin::MidiFileWriteResult)` in the existing anonymous namespace, mapping `invalidTimeline`/`pathUnavailable`/`writeFailed` to the user-facing strings from Decision 4's table.
- [x] 2.3 Implement `MainComponent::exportSequenceTo (const juce::File& destination)`: call `buildMidiExportTimeline`; on failure set `statusLabel` (red) + `NativeMessageBox::showMessageBoxAsync (WarningIcon, ..., no callback)`; on `MidiExportStatus::ok`, construct `MidiFileWriter (kMidiChannel, kNoteVelocity)` and call `writeToFile`; map `writeFailed`/`pathUnavailable` the same way; on `ok`, set `statusLabel` to `"Exported to " + destination.getFileName()`, no dialog.
- [x] 2.4 Implement `MainComponent::launchExportChooser()`: `exportChooser = std::make_unique<juce::FileChooser> ("Export MIDI...", defaultExportFile(), "*.mid");`; `launchAsync` with flags **`juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting`** — do NOT use `canOverwriteExisting` (not a real JUCE flag); in the callback, `return;` silently if `chooser.getResult() == juce::File()`, else call `exportSequenceTo (destination)`.

## Phase 3: Ctor/onClick Wiring, Destructor Reset (`Source/MainComponent.cpp`)

- [x] 3.1 In `~MainComponent()`, add `exportChooser.reset();` as the first statement, before `shutdownAudio()`.
- [x] 3.2 In the ctor, add `addAndMakeVisible (exportButton);`, `addAndMakeVisible (statusLabel);`, and `exportButton.onClick = [this] { launchExportChooser(); };`, all before `setSize (800, 600);` (must stay the last statement of the UI section).

## Phase 4: `resized()` Layout (`Source/MainComponent.cpp`)

- [x] 4.1 Add `constexpr int kMargin = 12, kControlHeight = 28, kButtonWidth = 140;` to the anonymous namespace.
- [x] 4.2 Implement the currently-empty `MainComponent::resized()` (lines 137-142) using `getLocalBounds().reduced (kMargin)` + `removeFromTop`/`removeFromLeft` to place `exportButton` then `statusLabel`, per Decision 5.

## Phase 5: Ctor Hook Removal (`Source/MainComponent.cpp`)

- [x] 5.1 Delete the one-shot export block (lines 55-73), including the `// ---- One-shot MIDI export (Decision 1)` and `Temporary hook - deleted in Phase 7` comments.
- [x] 5.2 Confirm `#include "export/MidiExportTimeline.h"` / `"export/MidiFileWriter.h"` remain, and `kExportFileName`, `kExportRepeats`, `kBpm`, `kStepsPerBeat`, `kMidiChannel` stay unchanged and still consumed.
- [x] 5.3 Search `Source/MainComponent.h` and `.cpp` for `TEMPORARY`/`Phase 7`; confirm zero remaining matches.

## Phase 6: Verification (Code Review + Manual Gate)

- [x] 6.1 Build the Berlin standalone target; confirm it compiles cleanly.
- [x] 6.2 Run `BerlinTests.exe --category=Berlin`; confirm exit 0 (regression only — `midi-export-trigger` has no reachable automated coverage, per its documented Known Coverage Gap; do not modify `Tests/`).
- [x] 6.3 Diff `Source/export/*` and `Tests/BerlinTests.jucer` against `main`; confirm byte-for-byte unchanged.
- [x] 6.4 Code review against Decisions 1/2/4/6: `exportChooser` member ownership and reset, no audio-thread interaction, failure-only `NativeMessageBox`, `Source/export/` untouched.
- [x] 6.5 Manual DAW-import gate: launch app (no file written) → click "Export MIDI..." (dialog seeded at `Documents/Berlin/berlin-export.mid`) → cancel (silent no-op) → export to a real path and open it in a DAW (120 BPM, 4/4, notes, 4 bars) → export to a read-only folder (dialog shown, no crash) → export during playback (no audible glitch). Confirmed by user 2026-09-05: export to DAW correct, cancel worked, playback uninterrupted during export, read-only folder handled without crash.
