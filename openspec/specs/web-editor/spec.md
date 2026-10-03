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

### Requirement: PoC UI Content

The UI MUST show the BPM with −/+ buttons that issue `dispatch` and a 16-step playhead row reflecting `playhead` events and the initial snapshot. It MUST NOT include other controls.

#### Scenario: BPM change
- GIVEN the UI shows BPM 120
- WHEN the user presses +
- THEN `dispatch` is called and the engine BPM changes (MANUAL: verified in standalone and Cakewalk Sonar)

#### Scenario: Playhead follows playback (MANUAL)
- GIVEN playback is running
- WHEN steps advance
- THEN the highlighted step in the row follows

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
