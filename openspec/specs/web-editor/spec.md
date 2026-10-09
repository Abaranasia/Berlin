# Web Editor Specification

## Purpose

Defines the `WebEditor`: a JUCE `WebBrowserComponent` (WebView2) host for the React UI, driving the engine only through `UiBridge`. It is the only editor of the standalone app and the plugin.

## Requirements

### Requirement: WebEditor Is The Only Editor

The system MUST have exactly one editor implementation, `WebEditor`. `createBerlinEditor` MUST always return a `WebEditor`; there MUST be no `BerlinAudioProcessorEditor` class, source file or `.jucer` entry, and no `BERLIN_WEB_UI` compile-time selection anywhere in the sources, `.jucer` files or scripts. The standalone MUST pass its `StandaloneHost` to the editor; the plugin MUST pass none. `BERLIN_WEB_UI_DEV_URL` (the Debug dev-server environment variable) and `JUCE_USE_WIN_WEBVIEW2=1` MUST remain unchanged. `WebEditor.cpp` MUST keep a `static_assert(BERLIN_EMBEDDED_ASSETS_FULL == 1)` whose message states that the generated asset table is stale or incomplete and the pre-build embed step must be re-run.

#### Scenario: Standalone gets WebEditor with host [M]
- GIVEN the standalone app
- WHEN it launches
- THEN a `WebEditor` is shown, constructed with the `StandaloneHost`, and the Settings dialog is available

#### Scenario: Plugin gets WebEditor without host [M]
- GIVEN the VST3 loaded in Cakewalk Sonar
- WHEN the editor opens
- THEN a `WebEditor` is shown with no `StandaloneHost` and the Settings button is hidden

#### Scenario: No legacy editor or flag remains [B]
- GIVEN the repository outside `openspec/changes/archive`
- WHEN searched for `BerlinAudioProcessorEditor` and for `BERLIN_WEB_UI` not followed by `_DEV_URL`
- THEN there are no matches, and both `.jucer` files and both projects build without the legacy editor files

#### Scenario: Stale stub header fails loudly [B]
- GIVEN a stale generated header with `BERLIN_EMBEDDED_ASSETS_FULL` equal to 0 on disk
- WHEN `WebEditor.cpp` is compiled
- THEN the `static_assert` fails with the reworded message

### Requirement: WebView2 Hosting And User Data Folder

`WebEditor` MUST use `Backend::webview2` and MUST store WebView2 data in `userApplicationDataDirectory/Berlin/WebView2`, creating the folder before the view is constructed. It MUST be resizable with an initial size of 960x680, minimum 720x520 and maximum 1920x1200. Both `.jucer` files MUST enable `JUCE_USE_WIN_WEBVIEW2="1"`.

#### Scenario: Data folder created
- GIVEN the folder does not exist
- WHEN `WebEditor` is constructed
- THEN the folder exists before the WebView2 environment is created

#### Scenario: Initial size and limits [M]
- GIVEN a `WebEditor` is constructed
- WHEN its size and constrainer are read
- THEN the size is 960x680, it is resizable, and its limits are min 720x520, max 1920x1200

#### Scenario: Resize in host [M]
- GIVEN the editor in standalone and in Cakewalk Sonar
- WHEN the window is resized within and beyond the limits
- THEN the web view follows within limits, and a host that ignores resize keeps the layout usable at its size

### Requirement: Missing Runtime Fallback

`WebEditor` MUST check the WebView2 runtime with `areOptionsSupported` before creating the browser. If it is unsupported, or if the WebView2 user data folder cannot be created, it MUST show only the existing centered fallback text label, MUST NOT create the browser, and MUST NOT crash. The label text MUST name the actual cause: one message when the WebView2 runtime is unavailable, a different message (including the user data folder path) when that folder cannot be created. No other native fallback editor exists.

#### Scenario: Runtime missing [M]
- GIVEN the WebView2 runtime is unavailable
- WHEN the editor opens
- THEN only the centered fallback text label is shown and the host stays stable (may be deferred if it cannot be reproduced on a machine with WebView2 installed)

#### Scenario: User data folder cannot be created [M]
- GIVEN the WebView2 user data folder cannot be created
- WHEN the editor opens
- THEN only the centered fallback text label is shown, no browser is created and the host stays stable (may be deferred if it cannot be reproduced)

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

#### Scenario: Controls beyond BPM present [M]
- GIVEN a build of the project
- WHEN the editor loads
- THEN the controls required by `web-ui-controls` are present and the playhead row still follows `playhead` events (standalone and Cakewalk Sonar)

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
