# MIDI Export Trigger Specification

## Purpose

A user-initiated MIDI export: a `juce::TextButton` on `MainComponent` opens an async, save-mode `juce::FileChooser`, and the chosen destination is passed through the unchanged `Source/export/` tier (`buildMidiExportTimeline` + `MidiFileWriter::writeToFile`). Replaces the Phase 6 ctor one-shot hook with a deliberate, addressable, observable action. No musical or file-format logic lives here — only trigger, destination selection, and outcome feedback.

## Requirements

### Requirement: Export Button Affordance

`MainComponent` MUST expose a `juce::TextButton` labelled to export MIDI (e.g. "Export MIDI...") as a visible child component, laid out in `resized()`. Clicking it MUST be the only way export happens — no export MUST fire automatically on construction or at any other time.

#### Scenario: Button is visible and positioned after resize

- GIVEN `MainComponent` has been constructed and resized
- WHEN the component is displayed
- THEN the export button is visible and positioned within the component's bounds

#### Scenario: No export occurs without a button click

- GIVEN a freshly launched application
- WHEN no user interaction has occurred
- THEN no `.mid` file is written and no existing file is overwritten

### Requirement: Async FileChooser With Member Lifetime

Clicking the export button MUST launch a `juce::FileChooser` in save mode asynchronously (never a blocking/modal browse call). The chooser instance MUST be owned by a `MainComponent` member (e.g. `std::unique_ptr<juce::FileChooser>`) so it remains alive until its completion callback runs, regardless of how long the user takes to respond.

#### Scenario: Chooser survives until the user responds

- GIVEN the export button has been clicked and the save dialog is open
- WHEN the user takes an arbitrary amount of time before choosing a destination or cancelling
- THEN the callback fires correctly with no crash or use-after-free

#### Scenario: Message thread is never blocked

- GIVEN the export button is clicked
- WHEN the dialog is presented
- THEN the application's message thread remains responsive; no synchronous/blocking browse call is used

### Requirement: Cancel Is Not a Failure

Dismissing or cancelling the FileChooser MUST NOT write any file, MUST NOT report an error to the user, and MUST NOT log or surface a failure state. It is a silent no-op.

#### Scenario: Cancelling the dialog writes nothing and reports nothing

- GIVEN the save dialog is open
- WHEN the user cancels it without selecting a destination
- THEN no file is written and no error/failure feedback is shown

### Requirement: Chosen Destination Overwrite Behavior

When the user selects a destination via the save dialog (including one that already exists, since the chooser is configured to allow overwrite), the system MUST write to exactly that path, replacing any existing file at that location.

#### Scenario: Selecting an existing file overwrites it

- GIVEN the user selects a destination path that already contains a `.mid` file
- WHEN the export proceeds
- THEN the file at that path is replaced with the newly exported content

#### Scenario: Selecting a new path creates the file

- GIVEN the user selects a destination path with no existing file
- WHEN the export proceeds
- THEN a new file is created at exactly that path

### Requirement: Outcome Feedback Mapped From MidiFileWriteResult

After a destination is chosen, the export MUST invoke the existing `buildMidiExportTimeline` + `MidiFileWriter::writeToFile` path unchanged, then report the resulting `MidiFileWriteResult` to the user asynchronously and non-blocking (not modal, not log-only). A failing result MUST be visibly reported to the user and MUST NOT crash the application.

#### Scenario: Successful write is confirmed to the user

- GIVEN a destination the process can write to
- WHEN the write succeeds
- THEN the user receives non-blocking, visible confirmation, and the file exists with valid contents at the chosen path

#### Scenario: Failed write is reported, not silent

- GIVEN a destination the process cannot write to (e.g. read-only directory)
- WHEN the write fails
- THEN the user receives non-blocking, visible failure feedback, no crash occurs, and the failure is not left log-only

### Requirement: Export Stays Read-Only and Off the Audio Path

Building the export timeline MUST only read `sequence` and MUST NOT acquire a lock, perform a heap allocation, or otherwise touch the audio thread's real-time path. Export MUST be safe to trigger while playback is active, with no audible glitch, dropout, or alteration to audio output.

#### Scenario: Exporting during playback does not affect audio

- GIVEN playback is currently running
- WHEN the user triggers an export and it completes
- THEN audio output is not glitched, dropped, or altered, and the export produces a correct file

#### Scenario: Export reads sequence without mutating or locking it

- GIVEN a sequence is being read by the audio thread for playback
- WHEN export builds its timeline from the same sequence
- THEN export performs no write to `sequence` and introduces no lock/allocation shared with the audio callback

## Known Coverage Gap

This capability has no automated test coverage. `Tests/BerlinTests.jucer` is deliberately `juce_core`-only (see `unit-test-harness`), and `MainComponent` requires `juce_gui_basics` for the button, layout, and `FileChooser`. Extracting the build-and-write call into one private method improves structure but does not make it unit-testable within the existing harness; expanding the harness is explicitly out of scope. Verification is: (1) source code review of the trigger/callback/feedback wiring, and (2) the human-confirmed manual gate — clicking the button, exporting to a real path, and opening the result in a DAW to confirm BPM, 4/4, notes, and bar count, per the same manual gate already established for `midi-file-output`. This is the same accepted tradeoff, not a new one.
