# Delta for web-editor

Purpose line change (non-requirement text): replace "Defines the opt-in `WebEditor` ... Selected at compile time by `BERLIN_WEB_UI` (default off)." with "Defines the `WebEditor`: a JUCE `WebBrowserComponent` (WebView2) host for the React UI, driving the engine only through `UiBridge`. It is the only editor of the standalone app and the plugin."

Scenario tags: [V] Vitest, [T] BerlinTests, [M] manual, [B] build.

## ADDED Requirements

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

## MODIFIED Requirements

### Requirement: Missing Runtime Fallback

`WebEditor` MUST check the WebView2 runtime with `areOptionsSupported` before creating the browser. If it is unsupported, or if the WebView2 user data folder cannot be created, it MUST show only the existing centered fallback text label, MUST NOT create the browser, and MUST NOT crash. The label text MUST name the actual cause: one message when the WebView2 runtime is unavailable, a different message (including the user data folder path) when that folder cannot be created. No other native fallback editor exists.

(Previously: only the unsupported-runtime case, shown as "a native fallback message component"; the user-data-folder failure case was not specified.)

#### Scenario: Runtime missing [M]
- GIVEN the WebView2 runtime is unavailable
- WHEN the editor opens
- THEN only the centered fallback text label is shown and the host stays stable (may be deferred if it cannot be reproduced on a machine with WebView2 installed)

#### Scenario: User data folder cannot be created [M]
- GIVEN the WebView2 user data folder cannot be created
- WHEN the editor opens
- THEN only the centered fallback text label is shown, no browser is created and the host stays stable (may be deferred if it cannot be reproduced)

### Requirement: UI Content Defers To web-ui-controls

The content and behavior of the React UI MUST be as specified by capability `web-ui-controls`. `WebEditor` MUST NOT impose any additional restriction on which controls the UI offers.

(Previously: the scenario was phrased "GIVEN the flag-on build".)

#### Scenario: Controls beyond BPM present [M]
- GIVEN a build of the project
- WHEN the editor loads
- THEN the controls required by `web-ui-controls` are present and the playhead row still follows `playhead` events (standalone and Cakewalk Sonar)

## REMOVED Requirements

### Requirement: Compile-Time Editor Selection

(Reason: `BERLIN_WEB_UI` was never defined in any build; the legacy editor and the flag are deleted, and the always-WebEditor behavior is covered by "WebEditor Is The Only Editor". Removes the scenarios "Flag off keeps legacy editor" and "Flag on selects WebEditor". The `MainComponent` `std::unique_ptr<juce::AudioProcessorEditor>` holding rule moves to plugin-host-integration.)
