# Tasks: Legacy Editor Removal (UI Redesign Slice 5/6, final)

Strict TDD is active: where a unit has a test seam (the embed library), write the failing test first, run it, confirm it fails for the right reason (RED), then make the minimal change (GREEN), then run the suite. Package manager is pnpm only (`pnpm --dir ui ...`); npm is broken, never use it. MSBuild: `C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe`. Projucer: `H:\Proyectos\Juce\Projucer\Projucer.exe --resave <jucer>`. C++ tests: `Tests/Builds/VisualStudio2026/x64/Debug/ConsoleApp/BerlinTests.exe --category=Berlin`. Skills to load when applying: `juce-app-dev`, `juce-app-testing`, `work-unit-commits` (paths in `.atl/skill-registry.md`).

Authoritative inputs: `proposal.md`, `design.md` (D1-D6), `specs/*/spec.md`. Where they overlap, follow the design. In particular D5 overrides the proposal: each `.jucer` edit is committed together with its Projucer `--resave` vcxproj output, never in a separate commit. Generated vcxproj files are never hand-edited.

Verification legend (from the specs): [V] Vitest, [T] BerlinTests, [B] build, [M] manual.

This slice is deletion-dominated. Only the C++ glue in commit 2 and the JS pipeline in commit 1 have behavior to protect; commit 3 is a pure deletion and commit 4 is docs and comments.

## Review Workload Forecast

| Field | Value |
|---|---|
| Deletions (excluding generated vcxproj) | ~1,300 lines (legacy editor 1,204; ~45 flag/stub JS; ~25 C++ and `.jucer`) |
| Authored additions (excluding generated vcxproj) | ~130 lines (tests ~45, C++ ~25, comments ~20, README ~30, scripts ~10) |
| Authored review load | ~250 lines (additions plus the non-whole-file deletions); the 1,204-line deletion is a whole-file removal |
| Total diff | ~1,430 lines (over the 800 budget; more than 90% is whole-file deletion) |
| Generated files excluded | `Builds/VisualStudio2026/*`, `Plugin/Builds/VisualStudio2026/*` (Projucer `--resave` output, excluded by glob) |
| Review budget | 800 authored lines per PR |
| Delivery strategy | single-pr-default |
| Chain strategy | not needed: single PR with `size:exception`, noted as deletion-dominated |
| 400-line budget risk | Low on authored load (~250); High on raw diff only because of whole-file deletion |
| 800-line budget risk | Low on authored load; raw diff ~1,430 is justified by the exception |
| Chained PRs recommended | No |
| Decision needed before apply | No (single PR with `size:exception` was the user's earlier choice and matches the proposal's recommendation) |

## Phase 0: Baseline

- [x] 0.1 `git status` and `git branch --show-current`: confirm branch `feat/ui-legacy-removal`; verify the git state yourself (`?? openspec/changes/ui-legacy-removal/` is expected).
- [x] 0.2 `pnpm --dir ui test`; record the Vitest count (baseline 495 expected, confirm) and, separately, the `embed-lib.test.mjs` count (so the rewrite in Phase 1 can be compared).
- [x] 0.3 Build `Builds/VisualStudio2026/Berlin.sln` and `Tests/Builds/VisualStudio2026/BerlinTests.sln` (Debug x64); run `BerlinTests.exe --category=Berlin`; record the pass count (baseline 410 expected, confirm). It must stay 410 through every commit (no `.jucer` change touches BerlinTests).

## Phase 1: Embed pipeline always full (commit 1, strict TDD)

Commit: `refactor(ui): always embed the full web UI`. Files: `ui/scripts/embed-lib.test.mjs`, `ui/scripts/embed-lib.mjs`, `ui/scripts/embed-assets.mjs`, `Berlin.jucer`, `Plugin/BerlinPlugin.jucer`, plus the resaved vcxproj. Specs: embedded-ui-assets "Generated Assets And Pre-Build Pipeline" and "Asset Lookup". Design D2, D3, D5, D6. Builds because the legacy editor is still selected and the full table compiles in either mode.

- [x] 1.1 RED, rewrite `embed-lib.test.mjs` before touching the lib (all [V]):
  - Drop the `readWebUiFlag` describe block and the `jucer` helper.
  - `renderAssetSources`: the stub render test becomes "an empty list still emits FULL 1 and `{ nullptr, 0 }`".
  - `computeStamp`: drop "changes with mode"; keep the stability and byte-change cases, now called as `computeStamp(inputs)` (no mode argument).
  - `runPipeline` tests: drop `flagOn`; "flag off" becomes "always spawns pnpm"; concurrency and BRL007 tests drop `flagOn`.
  - Add "a stale stub (`.stamp`='stale', header with FULL 0) is regenerated as FULL 1".
  - Satisfies: Pipeline ignores any flag [V]; Generated header guard [V]; Unchanged inputs skipped [V].
- [x] 1.2 Run `pnpm --dir ui test`; confirm RED for the right reason (arity or missing behavior on the old lib: `computeStamp(inputs)` hashing differently, stub header still FULL 0, `flagOn`-less calls rejected), not a syntax error in the test file.
- [x] 1.3 GREEN, edit `embed-lib.mjs`: delete `FLAG`, `decodeEntities` and `readWebUiFlag`; remove the `mode` parameter and the `flagOn` branches; `renderAssetSources({ assets })` always emits `#define BERLIN_EMBEDDED_ASSETS_FULL 1` and keeps the `{ nullptr, 0 }` branch for an empty list (D2); `computeStamp(inputs)` hashes a fixed `mode:full\n` preamble (D3); reword the D2 and header comments ("WebEditor static_asserts it so a stale stub header fails"). Keep the BRL codes and the pnpm spawn literals/timeout unchanged.
- [x] 1.4 Edit `embed-assets.mjs`: update the usage comment and the import, remove the `--jucer` parsing, call `runPipeline({ spawn, io })` with no arguments (D6).
- [x] 1.5 Run `pnpm --dir ui test` (all green; the Vitest count equals baseline minus the dropped cases plus the new stale-stub case), `pnpm --dir ui typecheck`.
- [x] 1.6 `Berlin.jucer` and `Plugin/BerlinPlugin.jucer`: Debug and Release `prebuildCommand` become `where node ... BRL001 ... & node <rel>\ui\scripts\embed-assets.mjs || exit /b 1` with no `--jucer ...` (D6). Keep the existing node/pnpm diagnostics. Run `Projucer --resave` for both `.jucer` files and commit the regenerated vcxproj together with the `.jucer` edit (D5). Never hand-edit the vcxproj.
- [x] 1.7 Build standalone and plugin (Debug x64) with the new pre-build; confirm the first build regenerates `EmbeddedAssets.{h,cpp}` with `BERLIN_EMBEDDED_ASSETS_FULL 1` (stamp miss because `ui/scripts/**` and the `mode:full` preamble changed, per the design's stale-stub note) and the legacy editor still compiles. Run `BerlinTests.exe` (410). Commit 1.

## Phase 2: WebEditor is the only editor (commit 2)

Commit: `refactor: make WebEditor the only editor`. Files: `Source/ui/WebEditor.{h,cpp}`, `Source/ui/EditorFactory.{h,cpp}`, `Source/MainComponent.{h,cpp}`, `Source/Main.cpp`. Specs: web-editor "WebEditor Is The Only Editor"; plugin-host-integration "Standalone Shell Preserves Today's Observable Behavior" and "Editor Attach/Detach Does Not Affect Engine State". Design D1, D4. Builds because the legacy files still compile, now unused. There is no headless seam for this JUCE GUI glue (`juce_gui_extra`), so protection is the compiler, the `static_assert`, and the Phase 6 manual gate.

- [x] 2.1 `WebEditor.h`: remove the `#if BERLIN_WEB_UI` guard and its `#endif`; header comment becomes "the plugin and standalone editor" (no "opt-in ... Compiled only when").
- [x] 2.2 `WebEditor.cpp`: remove the guard and `#endif`; keep `static_assert (BERLIN_EMBEDDED_ASSETS_FULL == 1, ...)` and reword the message to "embedded assets are the empty stub: re-run the pre-build (node and pnpm required)"; fix the stale comment. Keep `BERLIN_WEB_UI_DEV_URL` and `JUCE_USE_WIN_WEBVIEW2=1` untouched. Satisfies: Stale stub header fails loudly [B].
- [x] 2.3 `EditorFactory.{h,cpp}`: `createBerlinEditor (processor, StandaloneHost* host = nullptr)` has a single return, `std::make_unique<WebEditor> (processor, host)`; header and declaration comments describe the always-WebEditor behavior. Keep the thin wrapper so `BerlinAudioProcessor.cpp` stays free of `juce_gui_extra` and the `BERLIN_HEADLESS` path is untouched (D1).
- [x] 2.4 `MainComponent.{h,cpp}` (D4): `getEditorConstrainer()` returns `const juce::ComponentBoundsConstrainer&` (`jassert` the underlying pointer is non-null) with `noexcept const`; remove dead null branches. Fix the header comment ("BerlinAudioProcessor / WebEditor via createBerlinEditor"), the `editor` member comment and the constrainer comment. The Slice 3 frame math is unchanged.
- [x] 2.5 `Main.cpp` (D4): `MainWindow` always `setResizable (true, false)` and calls `applyEditorLimits()` before and after `setVisible`; remove the D13 comment and the `else` (fixed-size) branch. Satisfies: Standalone window resizable [M]; No fixed-size window [M]; Content follows window [M].
- [x] 2.6 Build standalone and plugin, Debug x64, zero new warnings; `BerlinTests.exe` (410); `pnpm --dir ui test`. Commit 2.

## Phase 3: Delete the legacy editor, `.jucer` entries, resave (commit 3)

Commit: `refactor: delete the legacy JUCE editor`. Files: `Source/plugin/BerlinAudioProcessorEditor.{h,cpp}` (1,204 lines), `Berlin.jucer`, `Plugin/BerlinPlugin.jucer`, plus the resaved vcxproj. Design D5.

- [x] 3.1 Confirm nothing references the legacy editor except its own files and the comments handled in Phase 4: `rg "BerlinAudioProcessorEditor" --glob '!openspec/changes/archive/**' --glob '!docs/proposal/**'`. Anything else is a Phase 2 miss and must be fixed first.
- [x] 3.2 Delete `Source/plugin/BerlinAudioProcessorEditor.h` and `.cpp`.
- [x] 3.3 Remove the `plBaeH` and `plBaeC` FILE entries from `Berlin.jucer` and `Plugin/BerlinPlugin.jucer`. Run `Projucer --resave` for both and commit the regenerated vcxproj with this change (D5). Never hand-edit the vcxproj.
- [x] 3.4 Build standalone and plugin (Debug and Release, x64, `/t:Rebuild`) from the resaved projects: zero errors, zero new warnings. Build `BerlinTests.sln` and run `BerlinTests.exe` (410). Commit 3.

## Phase 4: README and stale comments (commit 4)

Commit: `docs: add build prerequisites and drop legacy-editor comments`. Comment and README changes only. Specs: embedded-ui-assets "README documents prerequisites [M]".

- [x] 4.1 Create `README.md`: a short project line plus a "Build prerequisites" section listing Visual Studio 2026, Projucer (for `--resave`), node, pnpm (`corepack enable`), the WebView2 runtime and the NuGet package; note that the pre-build fails with a clear diagnostic (BRL001 node, BRL002 pnpm) when either tool is missing.
- [x] 4.2 `BerlinAudioProcessor.h:18` reads "an editor"; `BerlinAudioProcessor.cpp:319` reads "the web UI's DelayPanel"; `BerlinAudioProcessor.cpp:423` points to `ui/EditorFactory.h` (JUCE GUI headers).
- [x] 4.3 `Source/generation/GenerationParams.h:39` and `Source/synth/SynthPatch.h:69,130`: remove the legacy editor names and point to the web UI.
- [x] 4.4 `ui/src/lib/seed.ts:5`: comment becomes "Trimmed before validating (spec 'Seed Validation')." Leave `docs/proposal/ui-redesign-plan.md` and `openspec/changes/archive` untouched (historical).
- [x] 4.5 Build standalone (comment-only change, expect no behavior change); `pnpm --dir ui test` and `typecheck`. Commit 4.

## Phase 5: Build and test commands (run before each commit and again at the end)

- [x] 5.1 Standalone Debug x64 build: `Builds/VisualStudio2026/Berlin.sln`, `/t:Rebuild` at the end; zero errors, no new warnings.
- [x] 5.2 Plugin Debug x64 build: `Plugin/Builds/VisualStudio2026/Berlin.sln`, `/t:Rebuild` at the end; zero errors. (Release builds of both at least once after commit 3.)
- [x] 5.3 `BerlinTests.exe --category=Berlin`: 410 passing, unchanged from the baseline.
- [x] 5.4 `pnpm --dir ui test`: all green (baseline 495 adjusted only by the Phase 1 test edits).
- [x] 5.5 `pnpm --dir ui typecheck` (tsc, no errors).
- [x] 5.6 `pnpm --dir ui build`, with no mock chunk (existing `assertNoMockChunk`).

## Phase 6: Final grep gate [B]

- [x] 6.1 `rg "BERLIN_WEB_UI(?!_DEV_URL)" --pcre2 --glob '!openspec/changes/archive/**' --glob '!docs/proposal/**' --glob '!Source/ui/generated/**'`: no matches (only `BERLIN_WEB_UI_DEV_URL` may remain). Also check both `.jucer` files, the resaved vcxproj and `ui/scripts`.
- [x] 6.2 `rg "BerlinAudioProcessorEditor" --glob '!openspec/changes/archive/**' --glob '!docs/proposal/**'`: no matches (this includes both `.jucer` files and the resaved vcxproj).
- [x] 6.3 `rg "readWebUiFlag|--jucer"` in `ui/scripts` and both `.jucer` files: no matches. Satisfies: No legacy editor or flag remains [B]; Pipeline ignores any flag [V].

## Phase 7: Manual verification [M] (record the result and date per line)

Run on the final branch build with a real WebView2 runtime. Record results in the apply-progress and hand off to `sdd-verify`.

- [x] 7.1 Standalone launches the `WebEditor` at 960x680 and resizes within 720x520 to 1920x1200 with the native frame included; it is never locked to 800x952; the editor fills the content area; audio and OS MIDI output still work. Satisfies: Standalone window resizable [M]; No fixed-size window [M]; Content follows window [M]; Standalone gets WebEditor with host [M]. (user 2026-10-09)
- [x] 7.2 Settings dialog still opens in the standalone and applies a change (device, port, channel); Escape and Cancel still discard edits. Satisfies: Settings available with the `StandaloneHost` [M]. (user 2026-10-09)
- [x] 7.3 VST3 loaded in Cakewalk Sonar opens the resizable `WebEditor` with no Settings button; the controls from `web-ui-controls` are present and the playhead row follows `playhead` events. Satisfies: Plugin gets WebEditor without host [M]; Plugin editor has no host settings [M]; Controls beyond BPM present [M]. (user 2026-10-09)
- [x] 7.4 Build with the existing stale stub on disk regenerates FULL 1 (the stamp misses; spot check the header). Then delete `Source/ui/generated/` and rebuild: the assets regenerate and the `static_assert` holds. Satisfies: Full pipeline [B]; Generated files untracked [B] (`git status` shows nothing under `Source/ui/generated/`); Stale stub header fails loudly [B] if the pre-build is skipped. (user 2026-10-09)
- [x] 7.5 Build with pnpm hidden from PATH fails non-zero with the pnpm diagnostic (BRL002 per the design; the spec text says "pnpm not found"); a build with node hidden fails with the node diagnostic (BRL001). Record the observed codes against the README text. Satisfies: pnpm missing [B]; node missing [B]. (user 2026-10-09)
- [x] 7.6 WebView2 fallback: if feasible (for example by disabling the runtime or pointing to an unwritable user data folder), only the centered fallback label is shown and the host stays stable. Otherwise record as deferred. Satisfies: Runtime missing [M]; User data folder cannot be created [M] (both may be deferred). (user 2026-10-09, reproduced)
- [x] 7.7 Closing and reopening the editor loses no state and the auto-evolve timer keeps running with no editor open (regression check for the unchanged processor). Satisfies: Closing and reopening the editor loses no state [M]; Auto-evolve continues with no editor open [M]. (user 2026-10-09)
- [x] 7.8 Record all results and the final test counts (BerlinTests 410, Vitest) in the apply-progress; hand off to `sdd-verify`. (verify 2026-10-09: BerlinTests 411, Vitest 490; the 410 baseline became 411 with the fallback-message test)

## Spec Scenario Coverage Map

| Spec scenario | Task(s) |
|---|---|
| web-editor: Standalone gets WebEditor with host [M] | 2.3, 2.5, 7.1, 7.2 |
| web-editor: Plugin gets WebEditor without host [M] | 2.3, 7.3 |
| web-editor: No legacy editor or flag remains [B] | 3.1-3.4, 6.1-6.3 |
| web-editor: Stale stub header fails loudly [B] | 2.2, 7.4 |
| web-editor: Runtime missing / user data folder [M] | 7.6 (deferred if not reproducible) |
| web-editor: Controls beyond BPM present [M] | 7.3 |
| plugin-host-integration: Auto-evolve and reopen scenarios [M] | 7.7 |
| plugin-host-integration: Closing/reopening, teardown, plugin has no host settings [M] | 2.3, 7.3, 7.7 |
| plugin-host-integration: Standalone window resizable, no fixed-size window, content follows window [M] | 2.4, 2.5, 7.1 |
| embedded-ui-assets: Root maps to index / Unknown path / Empty table [T] | 1.1 (unchanged `WebAssetsTests`, 5.3) |
| embedded-ui-assets: Full pipeline [B] | 1.7, 7.4 |
| embedded-ui-assets: Pipeline ignores any flag [V] | 1.1-1.5, 6.3 |
| embedded-ui-assets: Generated header guard [V] | 1.1, 1.3 |
| embedded-ui-assets: Unchanged inputs skipped [V] | 1.1, 1.5 |
| embedded-ui-assets: pnpm missing / node missing [B] | 7.5 |
| embedded-ui-assets: Step failure aborts [B] | 1.1 (existing runPipeline tests stay green) |
| embedded-ui-assets: Generated files untracked [B] | 7.4 |
| embedded-ui-assets: README documents prerequisites [M] | 4.1 |
| unit-test-harness: drops `BerlinAudioProcessorEditor` mentions | 3.2, 5.3, 6.2 |
| web-ui-controls: flag and legacy wording removed | 5.4, 6.1, 7.3 |
