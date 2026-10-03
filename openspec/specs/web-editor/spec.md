# Web Editor Specification

## Purpose

Defines the opt-in `WebEditor`: a JUCE `WebBrowserComponent` (WebView2) host for the PoC React UI, driving the engine only through `UiBridge`. Selected at compile time by `BERLIN_WEB_UI` (default off).

## Requirements

### Requirement: Compile-Time Editor Selection

The system MUST define `BERLIN_WEB_UI` at project level, default off. With it off, `createEditor` and `MainComponent` MUST produce the legacy editor with unchanged size and behavior. With it on, they MUST produce `WebEditor`. `MainComponent` MUST hold the editor as `std::unique_ptr<juce::Component>`.

#### Scenario: Flag off keeps legacy editor
- GIVEN a build with `BERLIN_WEB_UI` undefined
- WHEN the standalone app or plugin editor is created
- THEN the legacy editor is shown at its current size, with behavior unchanged and no WebView2 usage

#### Scenario: Flag on selects WebEditor
- GIVEN a build with `BERLIN_WEB_UI` defined
- WHEN the editor is created
- THEN a `WebEditor` is returned

### Requirement: WebView2 Hosting And User Data Folder

`WebEditor` MUST use `Backend::webview2` and MUST store WebView2 data in `userApplicationDataDirectory/Berlin/WebView2`, creating the folder before the view is constructed. It MUST be fixed-size and fit within 800x680. Both `.jucer` files MUST enable `JUCE_USE_WIN_WEBVIEW2="1"`.

#### Scenario: Data folder created
- GIVEN the folder does not exist
- WHEN `WebEditor` is constructed
- THEN the folder exists before the WebView2 environment is created

#### Scenario: Editor fits the standalone window
- GIVEN the standalone window is 800x680
- WHEN `WebEditor` is shown
- THEN its fixed size fits without clipping beyond the existing window behavior

### Requirement: Missing Runtime Fallback

`WebEditor` MUST check the WebView2 runtime with `areOptionsSupported` before creating the browser. If unsupported, it MUST show a native fallback message component and MUST NOT crash or create the browser.

#### Scenario: Runtime missing (MANUAL)
- GIVEN the WebView2 runtime is unavailable
- WHEN the editor opens
- THEN a native message explains the missing runtime and the host stays stable

### Requirement: Native Functions

`WebEditor` MUST expose `dispatch(cmd, args)`, forwarding to `UiBridge::dispatch` and returning its result, and `getSnapshot`, returning `UiBridge::snapshot()`. It MUST NOT alter the `UiBridge` command set or snapshot, and MUST NOT touch the audio thread. Missing or invalid arguments MUST yield an error result and MUST NOT crash, using exactly these `{ok:false,error}` tokens: no `cmd` argument → `missing arg: command`; empty-string `cmd` → `missing arg: command`; non-string `cmd` → `invalid type: command`; `args` absent or void → treated as `{}` (no error); `args` present but not an object → `invalid type: args`.

#### Scenario: Dispatch forwards to bridge
- GIVEN valid `cmd` and `args`
- WHEN JS calls `dispatch`
- THEN `UiBridge::dispatch` receives them and its result is returned

#### Scenario: Snapshot returned
- GIVEN a running engine
- WHEN JS calls `getSnapshot`
- THEN the current `UiBridge::snapshot()` is returned

#### Scenario: Bad arguments
- GIVEN `dispatch` is called with no arguments, an empty-string `cmd`, a non-string `cmd`, or a non-object `args`
- WHEN the adapter runs
- THEN an error result with `missing arg: command`, `missing arg: command`, `invalid type: command`, or `invalid type: args` respectively is returned and no exception or crash occurs

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

### Requirement: Playhead Event Emitted Only On Change

`WebEditor` MUST poll the playhead from a message-thread `Timer` at about 30 Hz, reading it directly via `processor.getPlayheadStep()` / `isPlaying()` (read-only, message thread, not through `UiBridge`), and emit event `playhead {step, playing}` only when `step` or `playing` differs from the last emitted value. The last emitted value MUST be updated only when the event is actually sent (browser visible), so a hidden-to-visible transition re-emits. The timer MUST stop when the editor is destroyed.

#### Scenario: Change emits once
- GIVEN the playhead moves from step 3 to step 4
- WHEN the timer ticks
- THEN one `playhead` event with step 4 is emitted

#### Scenario: No change no event
- GIVEN the playhead and playing state are unchanged
- WHEN the timer ticks
- THEN no event is emitted

#### Scenario: Hidden change re-emitted when visible
- GIVEN the playhead changed while the browser was not visible, so no event was sent
- WHEN the timer ticks after the browser becomes visible
- THEN one `playhead` event with the current state is emitted

### Requirement: Dev Server Origin Is Debug-Only And Opt-In

The resource provider MUST serve only embedded assets in Release builds. A dev-server origin MUST be used only under `JUCE_DEBUG` (the `devServerOrigin` helper MAY compile in every configuration for testing) and MUST be activated only by the environment variable `BERLIN_WEB_UI_DEV_URL`, accepted only as `http://localhost:<port>` or `http://127.0.0.1:<port>`.

#### Scenario: Release ignores the variable
- GIVEN a Release build and `BERLIN_WEB_UI_DEV_URL` set
- WHEN the view loads
- THEN content comes from embedded assets only

#### Scenario: Debug opt-in
- GIVEN a Debug build with `BERLIN_WEB_UI_DEV_URL` set to a localhost or 127.0.0.1 URL
- WHEN the view loads
- THEN the dev-server origin is used; with it unset or set to any other origin, embedded assets are used

### Requirement: UI Content Defers To web-ui-controls

The content and behavior of the React UI MUST be as specified by capability `web-ui-controls`. `WebEditor` MUST NOT impose any additional restriction on which controls the UI offers.

#### Scenario: Controls beyond BPM present
- GIVEN the flag-on build
- WHEN the editor loads
- THEN the controls required by `web-ui-controls` are present and the playhead row still follows `playhead` events (MANUAL: standalone and Cakewalk Sonar)

### Requirement: Host Go/No-Go Behaviors (MANUAL)

These behaviors MUST be verified manually in the standalone app and Cakewalk Sonar; any failure is a no-go.

#### Scenario: Two instances (MANUAL)
- GIVEN two plugin instances open together
- WHEN each is operated
- THEN they work independently with no crash

#### Scenario: DPI (MANUAL)
- GIVEN 100% and 150%+ display scaling
- WHEN the editor renders
- THEN layout and text are correct at both

#### Scenario: Keyboard focus (MANUAL)
- GIVEN the web view has focus in a DAW
- WHEN keys are pressed
- THEN input works in the view and host shortcuts are not stolen when focus is outside it

#### Scenario: Close and reopen (MANUAL)
- GIVEN an open editor
- WHEN it is closed and reopened repeatedly
- THEN engine state is intact and nothing leaks or crashes
