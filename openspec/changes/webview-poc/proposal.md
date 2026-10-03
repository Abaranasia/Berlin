# Proposal: WebView Proof of Concept (UI Redesign Slice 1/6, go/no-go gate)

## Intent

Before rewriting the whole UI (Slices 2-5), prove that React + TypeScript + Vite inside JUCE 9 `WebBrowserComponent` (WebView2) works for Berlin in both the standalone app and a real Windows DAW. The PoC drives the engine only through the existing `UiBridge`. If it fails the go/no-go checks, the redesign stops and falls back to plain JUCE.

## Scope

### In Scope
- Add `JUCE_USE_WIN_WEBVIEW2="1"` to `Berlin.jucer` and `Plugin/BerlinPlugin.jucer`, then re-save them so Projucer adds the WebView2 package (NuGet `Microsoft.Web.WebView2` 1.0.3485.44, pinned).
- Add a project-level `BERLIN_WEB_UI` define, off by default:
  - `createEditor` picks `WebEditor` or the old editor with `#if`.
  - `MainComponent` holds the editor as `std::unique_ptr<juce::Component>`.
- `WebEditor`:
  - uses `Backend::webview2`
  - stores WebView2 data in `userApplicationDataDirectory/Berlin/WebView2`, creating the folder first
  - checks the runtime with `areOptionsSupported`; if it is missing, shows a native fallback label instead of the browser
  - exposes the native functions `dispatch` (passes to `UiBridge::dispatch`) and `getSnapshot`
  - polls the playhead with a ~30 Hz message-thread `Timer` and emits a `playhead {step, playing}` event only when the value changes
- A resource provider serves the embedded assets, with `/` mapped to `index.html`. In Debug builds only (`#if JUCE_DEBUG`), an environment variable can switch it to the Vite dev server.
- Gui-free helpers, using only `juce_core` so the headless tests can use them:
  - asset lookup
  - MIME type from file extension
  - rejecting path traversal
  - building the user data folder path
  - the adapter from native-function `var` arguments to `dispatch`
- `ui/`, a pnpm + Vite + React + TS project:
  - includes JUCE's `native/javascript/index.js`
  - shows the BPM with −/+ buttons
  - shows a 16-step playhead row
- `ui/scripts/embed-assets.mjs` generates `Source/ui/generated/EmbeddedAssets.{h,cpp}`.
- Pre-build step, in both `.jucer` files and every configuration:
  - runs `pnpm --dir ui install --frozen-lockfile && pnpm --dir ui run build && node ui/scripts/embed-assets.mjs`
  - fails with a clear message on any error, including when pnpm or node is missing

### Out of Scope
- Standalone clipping fix (window fixed at 800x680). It stays deferred to Slice 3. Slice 1 changes `MainComponent` only as much as the `unique_ptr` member requires, and the old-editor path keeps its current size and behaviour. `WebEditor` uses a fixed size that fits the current window.
- Any other control, layout or styling, the mock bridge, and Playwright.
- Changes to the `UiBridge` command set or snapshot, and any audio-thread changes.
- Making the web UI the default, and removing the legacy editor.

## Capabilities

### New Capabilities
- `web-editor`:
  - how the editor is chosen at compile time
  - WebView2 hosting and its user data folder
  - the fallback when the runtime is missing
  - the `dispatch`/`getSnapshot` native functions
  - the playhead event, sent only on change
  - the dev-server origin, which only Debug builds allow
- `embedded-ui-assets`:
  - asset table lookup and MIME mapping
  - rejecting path traversal
  - generating the assets in the pre-build step, which must fail loudly

### Modified Capabilities
- `plugin-host-integration`: the editor can now be either of two compile-time-selected implementations. The attach/detach and standalone-shell requirements apply to both.
- `unit-test-harness`: the gui-free `Source/ui/` helpers are registered in the tests project. `WebEditor` and the generated assets are excluded.
- `ui-bridge`: no delta. The adapter uses it unchanged.

## Approach

`WebEditor` is a thin adapter: the bridge logic lives in `UiBridge` and the asset logic in the gui-free helpers.

**Generated `EmbeddedAssets` (recommended): add it to `.gitignore` and regenerate it on every build.**
- Committing it would add large, noisy diffs that easily go stale.
- Trade-off: building the app or plugin needs pnpm even with the flag off. `BerlinTests` is not affected.

## Affected Areas

| Area | Impact |
|---|---|
| `Berlin.jucer`, `Plugin/BerlinPlugin.jucer` | Modified: WebView2 option, define, pre-build step, new sources |
| `Tests/BerlinTests.jucer` | Modified: gui-free UI helpers |
| `Source/MainComponent.{h,cpp}` | Modified: editor held through `unique_ptr` |
| `Source/plugin/BerlinAudioProcessor.cpp` | Modified: `createEditor` |
| `Source/ui/` | New: `WebEditor`, helpers, generated assets (gitignored) |
| `ui/` | New: frontend, build and embed scripts |

## Risks

| Risk | Likelihood | Mitigation |
|---|---|---|
| The first NuGet restore needs network access | Med | Note it as a one-time manual step |
| Visual Studio's PATH has no pnpm | Med | The pre-build step checks for it and fails clearly |
| Each plugin instance creates its own WebView (memory, shared data folder) | Med | Go/no-go check |
| Focus and DPI quirks in the DAW | Med | Go/no-go check in Cakewalk Sonar |
| AU can't be tested on Windows | Low | Accepted |
| Diff larger than forecast | Med | See the forecast below |

## Size Forecast (budget 800)

About 550-750 authored lines:

| Part | Lines |
|---|---|
| C++ | ~300 |
| C++ tests | ~130 |
| `ui/` source and scripts | ~200 |
| Vitest | ~60 |
| `.jucer` edits | ~40 |

Not counted: `pnpm-lock.yaml`, the generated assets, and the copied JUCE `index.js`.

800-line budget risk: Medium. If the diff goes over budget, split it into two PRs:
1. build plumbing, helpers and their tests
2. `WebEditor` and the UI

## Rollback Plan

The flag is off by default, so shipping builds are unchanged. To roll back fully, revert the PR. That removes `ui/`, `Source/ui/`, the `.jucer` options and the pre-build step, and restores the editor member in `MainComponent`.

## Dependencies

Slice 0 (`UiBridge`, PR2), pnpm 11.x, node 24, the WebView2 runtime, and network access for the first NuGet restore.

## Success Criteria (go/no-go)
- [ ] Standalone app and Cakewalk Sonar: the PoC UI renders, BPM −/+ changes the engine's BPM, and the playhead row follows playback.
- [ ] Two plugin instances open at once work independently.
- [ ] Correct at 100% and 150%+ DPI.
- [ ] Keyboard focus works in the web view and doesn't steal the host's shortcuts.
- [ ] After closing and reopening the editor, state is intact and nothing leaks or crashes.
- [ ] With the runtime missing, a native fallback message appears and nothing crashes.
- [ ] Builds with the flag off behave the same as before, and `BerlinTests` and Vitest pass.

If any check fails, stop and revisit the stack decision (plain JUCE fallback).

## Proposal question round

Auto mode, so no questions were asked. These assumptions need user review:
1. A failure in Sonar alone (with the standalone app passing) counts as a no-go.
2. ~~Requiring pnpm for every app/plugin build, even with the flag off, is acceptable.~~ **Resolved by the user (2026-10-03): builds with `BERLIN_WEB_UI` off MUST NOT require pnpm.** With the flag off, the pre-build step runs only `node ui/scripts/embed-assets.mjs` in stub mode. That writes an empty `EmbeddedAssets` table, with no pnpm, network or Vite. With the flag on, it runs the full `pnpm install --frozen-lockfile` + `pnpm run build` + embed pipeline. The generated files stay gitignored either way. Design must define how the script learns the flag state (for example, a per-configuration argument or environment variable) and keep that in sync with the `BERLIN_WEB_UI` define.
3. A fixed-size PoC editor (no clipping fix) is acceptable.
