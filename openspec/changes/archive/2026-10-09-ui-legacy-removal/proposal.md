# Proposal: Legacy Editor Removal (UI Redesign Slice 5, final)

## Intent

`BERLIN_WEB_UI` is not defined anywhere, so every default build (standalone and VST3) still ships the 1,204-line legacy JUCE editor with an empty asset table. The web UI has reached parity and is verified in Sonar. This slice makes the WebEditor the only editor and removes the flag, which leaves one build path.

## Scope

### In Scope
- Delete `Source/plugin/BerlinAudioProcessorEditor.{h,cpp}` and remove them from both `.jucer` files (D1).
- Remove all flag code (D2): the `#if BERLIN_WEB_UI` guards in `WebEditor.*`, the `EditorFactory` branches and the dead `setResizable(false)` branch in `Main.cpp`. `EditorFactory` stays as a thin wrapper.
- Embed pipeline: remove `readWebUiFlag`, the full/stub `mode` and the `--jucer` argument. The pipeline always runs full. Update the `.jucer` pre-build commands and `embed-lib.test.mjs`.
- **Guard decision:** the generated header still emits `BERLIN_EMBEDDED_ASSETS_FULL 1`, and the `static_assert` in `WebEditor.cpp` stays. Generated files are gitignored, so it fails loudly if a stale stub header from an older build is still on disk.
- Standalone window is always resizable within the existing WebEditor limits. The plugin editor is the WebEditor with no `StandaloneHost` (D4).
- Document that node and pnpm are required build prerequisites (D3). No README or build notes exist, so add a short top-level `README.md` "Build prerequisites" section.
- Clean up the legacy-editor comments in `GenerationParams.h`, `SynthPatch.h`, `BerlinAudioProcessor.h` and `MainComponent.h`.

### Out of Scope
- A native fallback editor. When WebView2 is missing, the existing centered `fallbackLabel` is the only fallback.
- Any change to `BERLIN_WEB_UI_DEV_URL`, `JUCE_USE_WIN_WEBVIEW2`, ui-bridge or standalone-settings behavior.
- Hand-editing the generated vcxproj files. They change only through a Projucer re-save.

## Capabilities

### New Capabilities
None.

### Modified Capabilities
- `web-editor`: removes "Compile-Time Editor Selection" and the opt-in purpose. The WebEditor is always the editor.
- `plugin-host-integration`: updates the editor wording and "Standalone Shell Preserves Today's Observable Behavior" (the window is now resizable), and removes the flag-off scenarios.
- `embedded-ui-assets`: a single full pipeline, with the "Flag off, no pnpm" scenario removed. The empty-table lookup scenario stays, reworded.
- `unit-test-harness`: drops the `BerlinAudioProcessorEditor` mentions.
- `web-ui-controls`: rewords the flag and legacy wording, and removes the legacy-parity requirement.

## Approach

Approach A from explore: hard removal with an always-full pipeline. The C++ cleanup and the pipeline cleanup are independent. The pipeline tests are rewritten first (strict TDD), then the scripts are changed.

## Affected Areas

| Area | Impact |
|---|---|
| `Source/plugin/BerlinAudioProcessorEditor.*` | Removed |
| `Source/ui/WebEditor.*`, `Source/ui/EditorFactory.*`, `Source/Main.cpp` | Flag code removed |
| `ui/scripts/embed-*.mjs` + test | Mode and flag removed |
| `Berlin.jucer`, `Plugin/BerlinPlugin.jucer` (+ regenerated vcxproj) | File list, pre-build command |
| `README.md` (new) | Prerequisites |

## Risks

| Risk | Likelihood | Mitigation |
|---|---|---|
| No flag rollback after this slice | Certain | Revert with git (see the rollback plan) |
| Without WebView2 the user sees only a label, and this path is untested | Low | Manual check if feasible, and the prerequisites are documented |
| Fresh clones or CI lack node/pnpm | Med | README note, and the pipeline fails loudly |
| vcxproj churn from the Projucer re-save | High | Excluded from the review count, committed separately |

## Rollback Plan

`git revert` the slice PR. That restores the legacy editor and the stub pipeline. No persisted data format changes.

## Delivery Forecast

- Pure deletions: ~1,250-1,350 lines (the editor plus the flag and stub code).
- Authored additions: ~100-200 lines (tests, script, specs, README).
- Generated vcxproj files are excluded from the count.

Recommendation: single PR with `size:exception`, noted as deletion-dominated, which matches the user's earlier choice.

## Success Criteria

- [ ] Standalone and VST3 (in Sonar) open the resizable WebEditor. The standalone Settings dialog works, and the plugin hides it.
- [ ] A fresh build with no generated assets regenerates the full table, and the `static_assert` holds.
- [ ] Vitest and `BerlinTests` pass, and no `BERLIN_WEB_UI` flag checks (`BERLIN_WEB_UI_DEV_URL` excepted) or `BerlinAudioProcessorEditor` references remain outside the archive.
- [ ] The fallback label is observed without WebView2, if feasible. Otherwise this is recorded as deferred.

## Proposal question round

Auto mode. Assumptions for review:
- A new `README.md` is acceptable as the place for the build notes.
- Keeping the `static_assert` is preferred over deleting it.
