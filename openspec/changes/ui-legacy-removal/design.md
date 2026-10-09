# Design: Legacy Editor Removal (UI Redesign Slice 5, final)

## Technical Approach

This slice removes code. Nothing new is designed. `createBerlinEditor` always returns `std::make_unique<WebEditor> (processor, host)`: the standalone passes its `StandaloneHost` and the plugin passes none. All `#if BERLIN_WEB_UI` guards go away (`BERLIN_WEB_UI_DEV_URL` and `JUCE_USE_WIN_WEBVIEW2=1` stay). The embed pipeline loses `readWebUiFlag`, the stub mode and `--jucer`, so every build runs pnpm and embeds the full table. The JS pipeline change comes first and follows strict TDD. The C++ change is split into a flag removal step and a pure deletion step, so every commit builds.

## Architecture Decisions

| # | Choice | Rejected | Rationale |
|---|---|---|---|
| D1 | Keep `EditorFactory` as a thin, always-WebEditor wrapper | Including `WebEditor.h` from `BerlinAudioProcessor.cpp`/`MainComponent` | The processor stays free of `juce_gui_extra`. The headless `BERLIN_HEADLESS` path is untouched. |
| D2 | `renderAssetSources({ assets })` always emits `#define BERLIN_EMBEDDED_ASSETS_FULL 1`. The `{ nullptr, 0 }` branch stays for an empty `assets` list. | Deleting the define and the `static_assert` | The static_assert is the loud backstop against a stale gitignored stub header. `findAsset` still accepts an empty table (`WebAssetsTests` covers this). |
| D3 | `computeStamp(inputs)` hashes a fixed `mode:full\n` preamble | No preamble | Existing flag-on stamps stay valid. A stub-era stamp (`mode:stub`) can never match. |
| D4 | `MainComponent::getEditorConstrainer()` returns `const juce::ComponentBoundsConstrainer&` (with a `jassert` that it is non-null). `MainWindow` is always `setResizable (true, false)` and calls `applyEditorLimits()` before and after `setVisible`. | Keeping the nullable pointer | The type states the invariant, so no dead null branches are left. The frame math from Slice 3 is unchanged. |
| D5 | Each `.jucer` change is committed together with its Projucer `--resave` output (vcxproj) | Committing the vcxproj changes separately (as the proposal suggested) | A vcxproj that lists a deleted `.cpp`, or the old `--jucer` command, breaks that commit. Reviewers exclude the generated paths by glob. |
| D6 | `embed-assets.mjs` takes no arguments. Pre-build: `where node … BRL001 … & node <rel>\ui\scripts\embed-assets.mjs \|\| exit /b 1` | Ignoring a leftover `--jucer` | One build path. The BRL001 (node) and BRL002 (pnpm `ENOENT`) diagnostics are unchanged. `BRL004` stays as the generic fallback code. |

## Data Flow

    MSBuild pre-build ─▶ embed-assets.mjs ─▶ runPipeline({spawn, io})
        stamp(mode:full + ui/src, ui/scripts, public, config) == .stamp && files exist ? skip
        : lock ─▶ pnpm --version ─▶ install --frozen-lockfile ─▶ run build ─▶ collect dist
              ─▶ assertNoMockChunk ─▶ EmbeddedAssets.{h,cpp} (FULL 1) ─▶ .stamp
    createEditor()/MainComponent ─▶ createBerlinEditor ─▶ WebEditor (host? settings fns)

**Stale stub on disk:** no manual handling is needed. `stampInputs` hashes `ui/scripts/**`, and this slice edits those files. The preamble also changes from `mode:stub`. So the first build after this slice always misses the stamp and regenerates FULL 1. If someone compiles without running the pre-build, the `static_assert` fails loudly.

## File Changes

| File | Action | Edit |
|---|---|---|
| `ui/scripts/embed-lib.test.mjs` | Modify | Drop the `readWebUiFlag` describe and the `jucer` helper. Stub render test becomes "an empty list still emits FULL 1 and `{ nullptr, 0 }`". `computeStamp` drops "changes with mode" and keeps the stability and byte-change cases, now called as `computeStamp(inputs)`. `runPipeline` tests drop `flagOn`. "flag off" becomes "always spawns pnpm". Add "a stale stub (`.stamp`='stale', header FULL 0) is regenerated as FULL 1". Concurrency/BRL007 tests drop `flagOn`. |
| `ui/scripts/embed-lib.mjs` | Modify | Delete `FLAG`, `decodeEntities` and `readWebUiFlag`. Remove the `mode` parameter, drop the `flagOn` branches, and reword the D2 and header comments ("WebEditor static_asserts it so a stale stub header fails"). |
| `ui/scripts/embed-assets.mjs` | Modify | Usage comment, import, and remove the `--jucer` parsing. Call `runPipeline({ spawn, io })`. |
| `Berlin.jucer`, `Plugin/BerlinPlugin.jucer` | Modify | Commit 1: Debug/Release `prebuildCommand` without `--jucer …`. Commit 3: remove the `plBaeH`/`plBaeC` FILE entries. |
| `Builds/VisualStudio2026/*`, `Plugin/Builds/VisualStudio2026/*` | Regenerate | `Projucer --resave` only, never hand-edited |
| `Source/ui/WebEditor.h` | Modify | Remove the guard and `#endif`. Header: "the plugin and standalone editor" instead of "opt-in … Compiled only when". |
| `Source/ui/WebEditor.cpp` | Modify | Remove the guard and `#endif`. static_assert message: "embedded assets are the empty stub: re-run the pre-build (node and pnpm required)". Fix the comment. |
| `Source/ui/EditorFactory.{h,cpp}` | Modify | Single return. Header and declaration comments describe the always-WebEditor behavior. |
| `Source/MainComponent.{h,cpp}` | Modify | D4 signature. Fix the header comment ("BerlinAudioProcessor / WebEditor via createBerlinEditor"), the `editor` member comment and the constrainer comment. |
| `Source/Main.cpp` | Modify | D4. Remove the D13 comment and the else branch. |
| `Source/plugin/BerlinAudioProcessorEditor.{h,cpp}` | Delete | 1,204 lines |
| `Source/plugin/BerlinAudioProcessor.{h,cpp}` | Modify (comments) | h:18 "an editor". cpp:319 "the web UI's DelayPanel". cpp:423 "`ui/EditorFactory.h` (JUCE GUI headers)". |
| `Source/generation/GenerationParams.h:39`, `Source/synth/SynthPatch.h:69,130` | Modify (comments) | Remove the legacy editor names and point to the web UI. |
| `ui/src/lib/seed.ts:5` | Modify (comment) | "Trimmed before validating (spec 'Seed Validation')." |
| `README.md` | Create | Short project line plus "Build prerequisites": Visual Studio 2026, Projucer (resave), node, pnpm (`corepack enable`), WebView2 runtime. Notes that the pre-build fails with BRL001/BRL002 when node or pnpm is missing. |

`docs/proposal/ui-redesign-plan.md` and the archive stay as they are (historical).

## Interfaces / Contracts

```js
renderAssetSources({ assets }) → { header, source }   // header always FULL 1
computeStamp(inputs) → hex
runPipeline({ spawn, io }) → { skipped: boolean }
```
```cpp
const juce::ComponentBoundsConstrainer& MainComponent::getEditorConstrainer() const noexcept;
```

## Testing Strategy

| Layer | What | How |
|---|---|---|
| Vitest (RED first) | The rewritten `embed-lib.test.mjs` cases above, plus the stale-stub regeneration case | `pnpm --dir ui test`. RED happens on the import or arity before the lib edit. |
| Build | Standalone, VST3 (Debug and Release), from a resaved project | MSBuild for both solutions, with zero warnings introduced |
| BerlinTests | 410 tests unchanged and passing (headless, no `.jucer` change) | Run `BerlinTests.exe` |
| Grep gate | No `BERLIN_WEB_UI\b` flag checks or `BerlinAudioProcessorEditor` outside `openspec/changes/archive` and `docs/proposal` | `rg` |
| Manual | See the gate below | Human |

**Manual gate:** (1) The standalone opens at 960x680 and resizes within 720x520 to 1920x1200 with the frame included. Settings opens and applies. (2) The VST3 in Sonar opens the resizable WebEditor with no Settings button. (3) A build with the existing stale stub on disk regenerates FULL 1. Deleting `Source/ui/generated/` and rebuilding regenerates it too. (4) A build with pnpm hidden from PATH fails with BRL002. (5) The WebView2 fallback label is checked if feasible, otherwise recorded as deferred.

## Threat Matrix

| Boundary | Applicability |
|---|---|
| Documentation-like paths | N/A: no file classification or execution |
| Git repository selection / Commit / Push / PR | N/A: no VCS automation |

The pnpm spawn keeps its fixed literal arguments and timeout. This slice only removes the flag gate and the `--jucer` file read, which shrinks the input surface.

## Work Units and Commits (single PR, `size:exception`, deletion-dominated)

| # | Commit | Builds because |
|---|---|---|
| 1 | `refactor(ui): always embed the full web UI` — test RED, then lib, script, both `.jucer` prebuild commands and the resave | The legacy editor is still selected. The full table compiles in either mode. |
| 2 | `refactor: make WebEditor the only editor` — guards, factory, Main/MainComponent (D4) | The legacy files still compile, unused |
| 3 | `refactor: delete the legacy JUCE editor` — delete the files, remove the `.jucer` FILE entries, resave | Nothing references them after commit 2 |
| 4 | `docs: add build prerequisites and drop legacy-editor comments` | Comment and README changes only |

**Forecast** (generated vcxproj excluded): about 1,300 deletions (editor 1,204, about 45 for the flag and stub in JS, about 25 in C++ and the jucer files) and about 130 authored additions (tests ~45, C++ ~25, comments ~20, README ~30, scripts ~10). The total of about 1,430 is over the 800 budget, but more than 90% of it is the deletion of whole files. Authored review load is about 250 lines.

## Migration / Rollout

No data migration. Rollback is `git revert` of the PR. Developers need node and pnpm on PATH, which the README documents.

## Open Questions

- [ ] Should the pipeline reject an empty `dist` (no `index.html`) with a new BRL code? This design keeps today's behavior: an empty table, which gives a blank WebView because every lookup misses.
