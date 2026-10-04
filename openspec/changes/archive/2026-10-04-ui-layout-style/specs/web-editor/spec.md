# Delta for Web Editor

## MODIFIED Requirements

### Requirement: Compile-Time Editor Selection

The system MUST define `BERLIN_WEB_UI` at project level, default off. With it off, `createEditor` and `MainComponent` MUST produce the legacy editor with unchanged behavior; the legacy plugin editor size MUST be unchanged and non-resizable. With it on, they MUST produce `WebEditor`. `MainComponent` MUST hold the editor as `std::unique_ptr<juce::AudioProcessorEditor>`.
(Previously: flag off required "unchanged size and behavior" for both plugin editor and standalone; standalone window size is now governed by plugin-host-integration.)

#### Scenario: Flag off keeps legacy editor
- GIVEN a build with `BERLIN_WEB_UI` undefined
- WHEN the standalone app or plugin editor is created
- THEN the legacy editor is shown with behavior unchanged, the plugin editor size is unchanged and non-resizable, and no WebView2 is used [M]

#### Scenario: Flag on selects WebEditor
- GIVEN a build with `BERLIN_WEB_UI` defined
- WHEN the editor is created
- THEN a `WebEditor` is returned

### Requirement: WebView2 Hosting And User Data Folder

`WebEditor` MUST use `Backend::webview2` and MUST store WebView2 data in `userApplicationDataDirectory/Berlin/WebView2`, creating the folder before the view is constructed. It MUST be resizable with an initial size of 960x680, minimum 720x520 and maximum 1920x1200. Both `.jucer` files MUST enable `JUCE_USE_WIN_WEBVIEW2="1"`.
(Previously: fixed-size, fitting within 800x680.)

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
