# Exploration: webview-poc (UI Redesign Slice 1/6, go/no-go gate)

Source: sdd-explore phase (Engram `sdd/webview-poc/explore`, obs #344), persisted to file by the orchestrator, with orchestrator spot-check results appended.

## Summary

A WebView2 proof of concept is feasible with the local JUCE 9 modules. Recommended:
- a project-level `BERLIN_WEB_UI` define, off by default
- embedded assets in every configuration, plus an optional Debug-only dev-server origin
- a gui-free C++ asset table, so the headless tests can cover it

## 1. JUCE API

File: `../../Projucer/modules/juce_gui_extra/misc/juce_WebBrowserComponent.h`

- `Options` provides:
  - `withBackend` (`Backend::webview2`)
  - `WinWebView2::withUserDataFolder` (:195)
  - `withWinWebView2Options`
  - `withNativeIntegrationEnabled`
  - `withNativeFunction` (:371)
  - `withEventListener`
  - `withResourceProvider(provider, optional allowedOrigin)` (:438)
- The component itself provides `getResourceProviderRoot()`, `areOptionsSupported` (:514) and `emitEventIfBrowserIsVisible` (:645).
- `NativeFunction` has the signature `void(const Array<var>&, completion)`. Native function callbacks run on the message thread.
- `ResourceProvider` has the signature `optional<Resource>(const String& path)`, where `Resource` is `{ std::vector<std::byte> data; String mimeType; }`. Paths arrive as `/x`, and `/` maps to `index.html`.
- On Windows, the resource provider requires `JUCE_USE_WIN_WEBVIEW2=1`.
- The JS frontend library is at `modules/juce_gui_extra/native/javascript/index.js`, alongside `package.json` and `check_native_interop.js`. It must be copied into `ui/` and exposes `getNativeFunction`.

## 2. WebView2 SDK

- In `jucer_ProjectExport_MSVC.h`, Projucer adds the NuGet package `Microsoft.Web.WebView2` version `1.0.3485.44` (pinned, :2691). It does this when `juce_gui_extra` is enabled and `JUCE_USE_WIN_WEBVIEW2` is set in `<JUCEOPTIONS>`.
- Both `.jucer` files currently set only `JUCE_STRICT_REFCOUNTEDPOINTER="1"` (`Berlin.jucer:104`, `Plugin/BerlinPlugin.jucer:138`). Each needs `JUCE_USE_WIN_WEBVIEW2="1"` and must then be re-saved in Projucer.
- The package is not present locally: there is no `packages.config`, no `packages/` folder and no `~/.nuget`. NuGet restores it on the first build, which needs network access.

## 3. Editor creation and the flag

- **Standalone:**
  - `MainComponent.h:40` holds a concrete `BerlinAudioProcessorEditor` by value. It is constructed at `MainComponent.cpp:5`, added at :8 and sized at :77.
  - The window is set to `setSize(800, 680)` at `MainComponent.cpp:9`, but the editor requests more height (`BerlinAudioProcessorEditor.cpp:474`). This is the known clipping issue that was deferred to this redesign.
- **Plugin:** `createEditor` is in `Source/plugin/BerlinAudioProcessor.cpp:425-432`, under `#if BERLIN_HEADLESS ... #else`.
- **Least invasive approach:**
  - Add `BERLIN_WEB_UI=1` as a project-level `defines` attribute, the same way `BerlinTests.jucer` defines `BERLIN_HEADLESS`. Default off.
  - In `createEditor`, pick `WebEditor` or the old editor with `#if BERLIN_WEB_UI`.
  - In the standalone, change `MainComponent`'s member to `std::unique_ptr<juce::Component> editor`, chosen by `#if`. This is the one invasive edit.

## 4. Pre-build step

- The Projucer VS exporter has a per-configuration `prebuildCommand`, emitted as `PreBuildEvent`. No `.jucer` sets one today.
- Proposed command: `npm --prefix ui run build && node ui/scripts/embed-assets.mjs`.
- The script generates `Source/ui/EmbeddedAssets.cpp` and `.h`, registered in the `.jucer` with `compile="1"`.
- It must exit non-zero on any failure, so the build breaks loudly.
- Avoid BinaryData, because it only regenerates when the `.jucer` is re-saved.

## 5. UiBridge mapping

- **Command dispatch:** a native function `dispatch(cmd, args)` that forwards to `UiBridge::dispatch` (`Source/bridge/UiBridge.cpp:724`) and returns `{ok, error, snapshot}`. Guard against missing or invalid args.
- **State on load:** a native function `getSnapshot`, backed by `UiBridge::snapshot()` (:756).
- **Playhead:**
  - Read `processor.getPlayheadStep()` (`BerlinAudioProcessor.h:142`) and `isPlaying()`.
  - Poll from a `juce::Timer` in `WebEditor` at about 30 Hz, on the message thread.
  - Call `emitEventIfBrowserIsVisible("playhead", {step, playing})` only when the values change.
- The audio thread is untouched.

## 6. Testability

- **Headless (BerlinTests):** the tests project has no `juce_gui_*` modules, so the code under test must be gui-free (juce_core only). That covers:
  - asset table lookup, including `/` mapping to `index.html`
  - mime type by file extension
  - unknown paths returning nullopt, and path traversal being rejected
  - building the user data folder path from a base `File`
  - the adapter from native-function `var` args to `UiBridge::dispatch`
- **Vitest:** pure TypeScript logic:
  - the BPM clamp
  - the step-row state
  - the bridge wrapper, tested against a mock
- **Manual only:**
  - WebView2 rendering
  - the fallback message when the WebView2 runtime is missing
  - two plugin instances open at once
  - DPI scaling
  - keyboard focus
  - closing and reopening the editor
  - the standalone app plus one DAW (Cakewalk Sonar)
  - the first NuGet restore

## 7. Options and recommendation

- **Asset serving:** embedded assets in every configuration, plus an optional Debug-only dev-server origin (`http://localhost:5173`) selected by an environment variable and guarded by `#if JUCE_DEBUG`.
- **Flag:** a project-level `BERLIN_WEB_UI` define, off by default, so CI and the tests are unaffected.

## Risks

- The generated asset `.cpp` must either be committed or be regenerated before every build. If Node is unavailable, the build breaks.
- A missing WebView2 runtime must be detected with `areOptionsSupported`, with a native label shown as the fallback.
- The user data folder (`userApplicationDataDirectory/Berlin/WebView2`) must be created before the browser is constructed.
- The WebView2 package version is pinned by Projucer.
- The AU build can't be tested on Windows.
- The tests project excludes GUI modules, so the asset helpers must use their own struct, not `WebBrowserComponent::Resource`.
- Estimated size: about 300-500 authored lines, excluding generated assets, which is within the 800-line budget.

## Orchestrator spot-check (2026-10-03)

- Confirmed: the API line numbers (:195, :371, :438, :514, :645), the JS library location, the pinned NuGet version 1.0.3485.44, and that both `<JUCEOPTIONS>` contain only `JUCE_STRICT_REFCOUNTEDPOINTER`.
- **BLOCKER — environment:** `node --version` reports v24.0.0 (installed via nvm), but `npm` is broken: `Cannot find module '@npmcli/config'` from `...\AppData\Roaming\nvm\v24.0.0\node_modules\npm\bin\npm-prefix.js`. The npm install under nvm is corrupt.
- **Resolution (user decision, 2026-10-03): use pnpm instead of npm.** A standalone pnpm 11.9.0 (`%LOCALAPPDATA%\pnpm\pnpm`, independent of nvm's npm) was smoke-tested in a scratch folder: `pnpm add -D vite` installed Vite 8.3.2, and `vite build` succeeded.
- The `ui/` package uses pnpm (`pnpm-lock.yaml`), and the pre-build command becomes `pnpm --dir ui install --frozen-lockfile && pnpm --dir ui run build && node ui/scripts/embed-assets.mjs`.
- The pre-build step must check that pnpm is available and fail with a clear message if it isn't, since a Visual Studio build may not inherit the user's PATH.
