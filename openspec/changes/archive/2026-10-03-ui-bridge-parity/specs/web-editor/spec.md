# Delta for Web Editor

## ADDED Requirements

### Requirement: Export File Chooser Native Function

`WebEditor` MUST expose native function `chooseExportFile()` resolving to `{cancelled: bool, path: string}`. The dialog MUST default to `~/Documents/Berlin/berlin-export.mid` (default owned by C++). On user cancel it MUST resolve `{cancelled: true, path: ""}`. One pending-dialog flag MUST cover both `chooseExportFile` and `confirm`. If either dialog is already pending it MUST resolve `{cancelled: true, path: "", error: "busy"}` without opening another. It MUST NOT alter `UiBridge`.

#### Scenario: User picks a file
- GIVEN no dialog is pending
- WHEN JS calls `chooseExportFile()` and the user selects a path
- THEN it resolves `{cancelled: false, path: <selected absolute path>}`

#### Scenario: User cancels
- GIVEN the chooser is open
- WHEN the user cancels
- THEN it resolves `{cancelled: true, path: ""}`

#### Scenario: Default location
- GIVEN no prior choice
- WHEN the chooser opens
- THEN its initial file is `~/Documents/Berlin/berlin-export.mid`

#### Scenario: Re-entry while pending
- GIVEN a chooser or confirm dialog is pending
- WHEN JS calls `chooseExportFile()` again
- THEN it resolves `{cancelled: true, path: "", error: "busy"}` and no second dialog opens

### Requirement: Confirm Native Function

`WebEditor` MUST expose native function `confirm(title, message)` resolving to a `bool` (true only when the user confirms). If a chooser or confirm dialog is already pending it MUST resolve `false` immediately without opening another.

#### Scenario: User confirms
- GIVEN no dialog is pending
- WHEN JS calls `confirm("Overwrite", "Replace Lead?")` and the user accepts
- THEN it resolves `true`

#### Scenario: User declines
- GIVEN the confirm dialog is open
- WHEN the user declines
- THEN it resolves `false`

#### Scenario: Re-entry while pending
- GIVEN a dialog is pending
- WHEN JS calls `confirm(...)`
- THEN it resolves `false` immediately and no second dialog opens

### Requirement: Dialog Lifetime Safety

If the editor is destroyed while a dialog is open, the native-function completion (the JS result callback of `chooseExportFile` or `confirm`) MUST NOT be invoked, even if the dialog's own callback runs synchronously during teardown, and nothing MUST touch the destroyed editor or browser. The destructor MUST remove the change listener first, then mark the editor as shutting down, stop the timer, and release any open file chooser and message box before releasing the browser.

#### Scenario: Close editor mid-dialog (MANUAL)
- GIVEN a file chooser or confirm dialog is open, in standalone and in Cakewalk Sonar
- WHEN the editor is closed
- THEN the host stays stable, no completion runs, and reopening the editor works

### Requirement: Snapshot Event On Engine Change

`WebEditor` MUST register as a `juce::ChangeListener` on the processor in its constructor and MUST remove itself FIRST in its destructor. The change callback MUST only mark a pending flag. The existing ~30 Hz timer MUST emit event `snapshot` (payload identical in shape to `getSnapshot`) only while the browser is visible and the flag is set, and MUST clear the flag only after sending, so a hidden-to-visible transition re-emits. A gui-free `PendingEventGate` (`Source/ui/WebEditorSupport.{h,cpp}`) with `markPending()`, `shouldEmit(bool visible)` and `commit()` MUST implement this gating and be unit-tested in `BerlinTests`.

#### Scenario: Change emits once
- GIVEN the gate is idle
- WHEN `markPending()` is called, then `shouldEmit(true)` returns true and `commit()` is called
- THEN a following `shouldEmit(true)` returns false

#### Scenario: No pending flag, no emit
- GIVEN `markPending()` was never called
- WHEN `shouldEmit(true)` is called
- THEN it returns false

#### Scenario: Hidden keeps pending
- GIVEN `markPending()` was called
- WHEN `shouldEmit(false)` is called, then later `shouldEmit(true)`
- THEN the first returns false and the second returns true (re-emit after becoming visible)

#### Scenario: Pending until committed
- GIVEN `markPending()` was called and `shouldEmit(true)` returned true
- WHEN `commit()` has not been called
- THEN `shouldEmit(true)` still returns true

#### Scenario: Multiple marks coalesce
- GIVEN `markPending()` is called three times before a tick
- WHEN one emit is committed
- THEN `shouldEmit(true)` is false afterwards

#### Scenario: State restore and auto-evolve reach the UI (MANUAL)
- GIVEN the web UI is visible
- WHEN auto-evolve mutates or state is restored
- THEN the UI updates from a `snapshot` event without user input, in standalone and Cakewalk Sonar

#### Scenario: Listener removed first on destruction (MANUAL / code review)
- GIVEN a live `WebEditor`
- WHEN it is destroyed while the processor sends a change message
- THEN no callback reaches the destroyed editor

### Requirement: UI Content Defers To web-ui-controls

The content and behavior of the React UI MUST be as specified by capability `web-ui-controls`. `WebEditor` MUST NOT impose any additional restriction on which controls the UI offers.

#### Scenario: Controls beyond BPM present
- GIVEN the flag-on build
- WHEN the editor loads
- THEN the controls required by `web-ui-controls` are present and the playhead row still follows `playhead` events (MANUAL: standalone and Cakewalk Sonar)

## REMOVED Requirements

### Requirement: PoC UI Content

(Reason: the PoC restriction "MUST NOT include other controls" is obsolete; the UI now provides full legacy parity.)
(Migration: replaced by "UI Content Defers To web-ui-controls"; BPM and playhead behavior are specified in `web-ui-controls`.)
