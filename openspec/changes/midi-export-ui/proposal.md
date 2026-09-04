# Proposal: MIDI Export UI

## Intent

Phase 6 made export *work* but not *reachable*. Export fires once, in `MainComponent`'s constructor, to a fixed path (`Documents/Berlin/berlin-export.mid`), overwriting silently on every launch, with failures visible only in the log. A user cannot choose when to export, where the file goes, or tell whether it worked. The hook is labelled "Temporary hook - deleted in Phase 7" in `MainComponent.cpp:57` and was designed as a seam to replace here (archived `2026-09-04-midi-export/design.md`).

This change makes export a deliberate, addressable, observable user action.

## Scope

### In Scope

- A `juce::TextButton` child of `MainComponent` ("Export MIDI...") with `resized()` bounds — the first interactive control in this codebase.
- Async `juce::FileChooser` in save mode, held as a `std::unique_ptr` **member** so it outlives the callback; default location/filename seeded from today's `resolveExportFile()` logic.
- Extraction of the ctor's build-and-write block into one private `MainComponent` method invoked from the chooser callback.
- **User-visible outcome**: a failed export MUST report to the user (async, non-blocking), not only `juce::Logger`. Cancel is a no-op, not a failure.
- Deletion of the ctor's one-shot export and its "temporary hook" framing.

### Out of Scope

- All other roadmap Phase 7 "User Interface" work: piano roll, parameter controls, generate, randomize, mutate, presets.
- Any export-option controls (repeat count, channel, velocity, tempo). `kExportRepeats = 4` stays a defaulted constant.
- Any change to `Source/export/` — `buildMidiExportTimeline`, `MidiFileWriter` and `MidiFileWriteResult` are consumed unchanged.
- Remembered/persisted output directory, export history, progress UI, look-and-feel or theming.
- Adding `juce_gui_basics` to `Tests/BerlinTests.jucer` (see Risks).

## Capabilities

### New Capabilities

- `midi-export-trigger`: user-initiated export — button affordance, destination selection, cancel semantics, overwrite handling, and success/failure feedback mapped from `MidiFileWriteResult`.

### Modified Capabilities

- None. `midi-export-timeline` and `midi-file-output` requirements are unchanged; only the call site moves.

## Approach

Exploration approach 1 (`sdd/midi-export-ui/explore`): async `FileChooser` + `TextButton`, export logic extracted to a private method.

1. **Async, not modal.** `launchAsync` with `saveMode | canOverwriteExisting`. `browseForFileToSave` blocks the message thread and is unusable on the mobile targets `Main.cpp` already guards for.
2. **Chooser lifetime is a member.** The classic JUCE dangling-chooser bug; ownership is a hard requirement, not a detail.
3. **Writer already covers the hard parts.** `MidiFileWriter::writeToFile` is atomic via `juce::TemporaryFile`, creates the parent directory, and returns a 4-value result. The UI maps that result to feedback; it adds no file logic.
4. **Message thread only.** Export reads `sequence`; the audio thread also reads it via `player`. Export stays strictly read-only and introduces no lock, atomic, or allocation on the audio path.
5. **Precedent-setting.** This is the first button, first `resized()` layout and first async dialog in the repo. `design.md` must state the pattern explicitly so the rest of Phase 7 inherits it.

## Affected Areas

| Area | Impact | Description |
|------|--------|-------------|
| `Source/MainComponent.h` | Modified | `juce::TextButton` + `std::unique_ptr<juce::FileChooser>` members; private export method; `resolveExportFile` reframed as default-location helper |
| `Source/MainComponent.cpp` | Modified | Ctor hook removed; button wiring, `resized()` layout, chooser callback, outcome feedback |
| `Source/export/` | Untouched | Public API consumed as-is |
| `Source/core/`, `generation/`, `playback/`, `midi/` | Untouched | No audio-path change |
| `openspec/specs/midi-export-trigger/` | New | Spec for the new capability |
| `Berlin.jucer`, `Builds/`, `JuceLibraryCode/` | Possibly regenerated | Only if files are added; no module change (`juce_gui_basics`/`juce_gui_extra` already linked) |
| `Tests/BerlinTests.jucer` | Untouched | Module list stays `juce_core`-only |

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| `FileChooser` destroyed before its async callback fires → crash/no-op | High | Member `unique_ptr` ownership is a spec requirement with an explicit manual gate step |
| **The extracted method is still not unit-testable.** The harness is `juce_core`-only; `MainComponent` needs `juce_gui_basics`. Extraction improves structure, not coverage — the exploration overstated this | High | State the limitation openly; verification is code review + the manual gate, the same documented tradeoff as `midi-file-output`'s "Known Coverage Gap". Do NOT expand the harness to chase it |
| No existing button/dialog convention → ad-hoc pattern becomes de facto standard for all later Phase 7 UI | High | `design.md` records the chosen ownership/layout/feedback pattern as an explicit, reusable precedent |
| Export during live playback reads `sequence` concurrently with the audio thread | Med | Read-only access only; `design.md` must confirm whether `SequencePlayer` copies or references `sequence` and record the conclusion |
| Feedback UI itself blocks the message thread (modal `AlertWindow`) | Med | Async message box only; no modal loop |
| Scope creep into repeat-count/channel controls or wider Phase 7 UI | High | Enumerated non-goals; every option control is deferred to a separate change |
| Silent success leaves the user unsure the file was written | Med | Decide confirmation affordance in `design.md`; failure feedback is mandatory regardless |

## Rollback Plan

`git revert` the change commits. `Source/MainComponent.h/.cpp` return to their Phase 6 state — the ctor one-shot export resumes and `resized()` is empty again. No other source tier is touched, so nothing else needs restoring. If `Berlin.jucer` gained `<FILE>` entries, restore it and run `Projucer.exe --resave`; if it did not, the revert is source-only. Delete `openspec/specs/midi-export-trigger/`. Already-exported `.mid` files are inert user data outside the repo. Existing suites stay green either way, since no test-visible code changes.

## Dependencies

- Exploration: Engram `sdd/midi-export-ui/explore`.
- Archived `midi-export` change (Phase 6) — merged, providing the stable `Source/export/` API.
- `juce_gui_basics` / `juce_gui_extra` — already linked; no module addition.
- Projucer, only if the file list changes.
- A human for the manual gate (no automated coverage is reachable here).

## Success Criteria

- [ ] Launching the app performs **no** export and writes **no** file; `Documents/Berlin/berlin-export.mid` is not created or overwritten on startup.
- [ ] An "Export MIDI..." button is visible, correctly positioned after resize, and opens a save dialog.
- [ ] Choosing a destination writes a valid `.mid` at exactly that path; re-opening it in a DAW shows the same BPM, 4/4, notes and bar count Phase 6's manual gate established.
- [ ] Cancelling the dialog writes nothing and reports no error.
- [ ] A failing write (read-only destination) produces user-visible feedback, not a log-only silence, and does not crash.
- [ ] Exporting while playback is running does not glitch, drop or alter audio output.
- [ ] `Source/export/*` and `Tests/BerlinTests.jucer` are byte-for-byte unchanged in the final diff.
- [ ] `BerlinTests.exe --category=Berlin` still exits 0.
- [ ] No `TEMPORARY`/`Phase 7` hook comment remains in `Source/MainComponent.*`.

## Proposal Question Round

Session mode is `auto`, so these could not be asked interactively. Assumptions taken — correct any before `sdd-spec`/`sdd-design`:

1. **Failure feedback**: assumed a failed export MUST be user-visible. Alternative: keep log-only and defer feedback to a later UI change.
2. **Success feedback**: assumed *optional* (deferred to `design.md`). Should a successful export confirm visibly, or stay silent?
3. **Default destination**: assumed the existing `Documents/Berlin/berlin-export.mid` seeds the dialog, unremembered between exports. Alternative: last-used directory (needs persistence — currently out of scope).
4. **Repeat count**: assumed frozen at 4. Confirm no user control is wanted in this slice.
5. **Export while playing**: assumed allowed. Alternative: disable the button during playback.
