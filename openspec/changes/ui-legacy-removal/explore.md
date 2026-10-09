# Exploration: ui-legacy-removal (UI Redesign Slice 5/6, final)

Mirror of Engram `sdd/ui-legacy-removal/explore` (id 410).

## Key correction

`BERLIN_WEB_UI` is NOT defined in HEAD. Berlin.jucer has no `defines`; Plugin/BerlinPlugin.jucer has only `JUCE_VST3_CAN_REPLACE_VST2=0`. "Flag off" means absent, so default builds (including the VST3) ship the legacy editor with a stub asset table. No `.jucer` define needs deleting. `BERLIN_WEB_UI_DEV_URL` is a different identifier (Debug dev-server env var) and stays.

## Inventory

- Legacy editor: `Source/plugin/BerlinAudioProcessorEditor.{h,cpp}` (216 + 988 = 1,204 lines). No other code becomes dead; shared helpers are also used by `UiBridge`. Comment-only mentions in GenerationParams.h, SynthPatch.h, BerlinAudioProcessor.h, MainComponent.h.
- C++ flag uses: `WebEditor.h` guard, `WebEditor.cpp` guard + `static_assert(BERLIN_EMBEDDED_ASSETS_FULL == 1)`, `EditorFactory.cpp` two branches, `EditorFactory.h` comment, `Main.cpp` comment + dead `setResizable(false)` branch.
- .jucer: both list the legacy editor files; both keep `JUCE_USE_WIN_WEBVIEW2="1"`; pre-build runs `node ui/scripts/embed-assets.mjs --jucer <path>` (the `--jucer` arg exists only to read the flag).
- Pre-build scripts: `embed-assets.mjs`, `embed-lib.mjs` (`readWebUiFlag`, full/stub `mode`, `flagOn` branch), `embed-lib.test.mjs` (flag and stub cases). `dev-config.test.mjs` only references `BERLIN_WEB_UI_DEV_URL`.
- Tests: `Tests/BerlinTests.jucer` needs no change (no editor, WebEditor or EmbeddedAssets registered). Headless `createEditor() == nullptr` test unaffected.

## Behavior changes

- Standalone: fixed 800x952 legacy window goes away; WebEditor is resizable (960x680 initial, 720x520 to 1920x1200). `Main.cpp` always applies `applyEditorLimits`.
- Plugin: editor becomes the resizable WebEditor with no `StandaloneHost` (no settings functions). Already verified in Sonar under the flag.
- Missing WebView2: existing centered `fallbackLabel` in WebEditor.cpp is the only fallback; never exercised (Slice 1 task 12.7 deferred).
- Build: node + pnpm become mandatory for every build (generated assets are gitignored); WebView2 runtime + NuGet package required.

## Specs needing deltas

- web-editor: REMOVE "Compile-Time Editor Selection" and opt-in Purpose; MODIFY to always-WebEditor.
- plugin-host-integration: MODIFY editor wording and "Standalone Shell Preserves Today's Observable Behavior"; REMOVE flag-off scenarios.
- embedded-ui-assets: MODIFY "Generated Assets And Pre-Build Pipeline" to a single full pipeline; REMOVE "Flag off, no pnpm"; keep the empty-table lookup scenario reworded.
- unit-test-harness: MODIFY to drop `BerlinAudioProcessorEditor` mentions.
- web-ui-controls: reword flag/legacy wording; REMOVE the legacy-parity requirement.
- Unchanged: ui-bridge, standalone-settings.

## Approaches

| | A. Hard removal, always-full pipeline (recommended) | B. Hard removal, keep stub pipeline | C. Minimal native fallback editor |
|---|---|---|---|
| Scope | Delete editor and all flag/stub code | Delete editor and flag branches; stub code stays dead | Small JUCE editor instead of the label |
| Pros | One build path, smallest surface | Smaller diff | Better UX without WebView2 |
| Cons | Rewrites pipeline tests | Dead stub code, misleading define | Second UI to maintain; large scope |

Keep `EditorFactory` as a thin wrapper to avoid including `WebEditor.h` from the processor and MainComponent.

## Risks and size

- Plugin users without WebView2 see only a text label (untested).
- No flag-based rollback after this slice.
- Fresh clones and CI need node and pnpm.
- Projucer re-save of two .jucer files churns tracked vcxproj files.
- Size: ~1,300-1,450 deleted lines vs ~100-200 authored; deletions alone exceed the 800 budget. Single PR with size:exception noted as pure deletion, or a 2-PR split.
