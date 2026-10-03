# Tasks: WebView PoC (UI Redesign Slice 1/6)

Strict TDD is active: every behavior task is RED (write the failing test, run it, confirm it fails for the right reason) then GREEN (minimal code) then run the suite. Package manager is pnpm 11.9.0 (`%LOCALAPPDATA%\pnpm`); npm is broken, never use it. MSBuild: `C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe`. Projucer: `H:\Proyectos\Juce\Projucer\Projucer.exe --resave <jucer>`.

## Review Workload Forecast

| Field | Value |
|---|---|
| Estimated authored lines | ~910 total (excludes `pnpm-lock.yaml`, generated assets, copied JUCE `index.js`, Projucer-regenerated vcxproj) |
| Review budget | 800 lines; risk: High as a single PR |
| Delivery strategy | auto-chain (user decision: 2 PRs) |
| Chain strategy | feature-branch-chain; tracker `feat/UI-design` (draft, no-merge); skill `C:\Users\ran_k\.claude\skills\chained-pr\SKILL.md` (found) |

| PR | Branch / base | Boundary | Est. lines | Verification | Rollback |
|---|---|---|---|---|---|
| PR1 | `feat/ui-webview-poc` -> `feat/ui-engine-api-pr2` | D1-D5 plumbing: `.gitignore`, minimal `ui/` scaffold, `embed-assets.mjs` + `embed-lib.mjs` + Vitest, `WebAssets`, `WebEditorSupport` + BerlinTests, `.jucer` edits (flag OFF), resave | ~540 | `pnpm --dir ui test`; `BerlinTests.exe --category=Berlin` (353 + new); both app solutions build, flag off, no pnpm required | Revert PR1: removes `ui/`, `Source/ui/` helpers, `.jucer` options and pre-build; no runtime behavior was changed |
| PR2 | `feat/ui-webview-poc-pr2` -> PR1 branch | `WebEditor`, `EditorFactory`, `createEditor` + `MainComponent` change, React UI + Vitest, fallback, flag-on build | ~370 | `pnpm --dir ui test`; `BerlinTests`; flag-off builds unchanged; flag-on builds both targets | Revert PR2: PR1 plumbing stays inert; flag defaults off so shipping builds are unchanged |

Decision needed before apply: No
Chained PRs recommended: Yes (already chosen)
Chain strategy: feature-branch-chain
400-line budget risk: High (each PR is under the 800 project budget)

Dependency diagram (each PR body must repeat it, marking the current PR):

    feat/UI-design (tracker, draft)
      <- feat/ui-engine-api-pr2 (Slice 0 bridge)
           <- PR1 feat/ui-webview-poc  (plumbing)
                <- PR2 feat/ui-webview-poc-pr2  (WebEditor + React UI)

## Phase 0: Baseline Builds (blocking, do first; PR1 branch)

- [x] 0.1 Build `Builds/VisualStudio2026/Berlin.sln` (Debug x64, `/t:Rebuild`); record exit code, errors, warnings.
- [x] 0.2 Build `Plugin/Builds/VisualStudio2026/Berlin.sln` (VST3, Debug x64); record the same. Expected to pass since Slice 0 registered TempoSync/UiBridge; record honestly otherwise.
- [x] 0.3 Build `Tests/Builds/VisualStudio2026/BerlinTests.sln` (Debug x64, `/t:Rebuild`); record exit code.
- [x] 0.4 Run `Tests/Builds/VisualStudio2026/x64/Debug/ConsoleApp/BerlinTests.exe --category=Berlin`; record the pass count (expected 353).
- [x] 0.5 Record tool versions (`node --version`, `pnpm --version` expected 11.9.0) and confirm `where node` resolves from a VS shell. Note: flag-off verification later needs a shell/PATH without pnpm.

## Phase 1: Ignore rules and ui/ scaffold (PR1)

- [x] 1.1 `.gitignore`: add `Source/ui/generated/`, `ui/node_modules/`, `ui/dist/` (spec: Generated files untracked).
- [x] 1.2 `ui/package.json` (private, `"type":"module"`, scripts `build`, `test`; devDependencies vite, typescript, react, react-dom, vitest, @vitejs/plugin-react), `ui/tsconfig.json`, `ui/vite.config.ts` (`base:'./'`, vitest config). Minimal: only what the embed script and Vitest need in PR1; React sources arrive in PR2.
- [x] 1.3 `pnpm --dir ui install` once to produce `ui/pnpm-lock.yaml` (committed, not counted in forecast). Confirm `pnpm --dir ui test` runs (no tests yet is acceptable).

## Phase 2: embed-lib.mjs pure functions (PR1, strict TDD)

Files: `ui/scripts/embed-lib.mjs`, `ui/scripts/embed-lib.test.mjs` (Vitest).

- [x] 2.1 RED `readWebUiFlag`: `defines="BERLIN_WEB_UI=1"` -> on; absent -> off; `BERLIN_WEB_UI=0` -> off; plugin case `JUCE_VST3_CAN_REPLACE_VST2=0` + `BERLIN_WEB_UI=1` separated by newline, comma and space -> on each; `BERLIN_WEB_UI` anywhere outside `JUCERPROJECT@defines` (e.g. in a config `defines`/`extraPreprocessorDefinitions`) -> throws (D1).
- [x] 2.2 RED `renderAssetSources`: stub (no assets) emits empty table and `BERLIN_EMBEDDED_ASSETS_FULL 0`; full mode emits `FULL 1`, one entry per file with `/`-normalised relative path and correctly escaped/hex bytes; output deterministic (D2).
- [x] 2.3 RED `computeStamp`: same mode + same inputs -> same hash; changed byte in any listed input (`ui/src/**`, `index.html`, `package.json`, `pnpm-lock.yaml`, `vite.config.ts`, `tsconfig*.json`, `scripts/*`) or mode change -> different hash (D5).
- [x] 2.4 RED `runPipeline` with injected `spawn`: flag off -> no spawn calls; flag on -> `pnpm --dir ui install --frozen-lockfile`, then `run build`, then embed, in order; pnpm spawn error/ENOENT -> error containing `pnpm not found` (BRL002); non-zero build exit -> throws and embed not run; matching stamp + files present -> skips everything; write-if-changed (identical content not rewritten) (D3, D5).
- [x] 2.5 Run `pnpm --dir ui test`; confirm 2.1-2.4 FAIL (module missing or functions absent).
- [x] 2.6 GREEN implement `embed-lib.mjs` (`readWebUiFlag`, `renderAssetSources`, `computeStamp`, `runPipeline`); fixed literal spawn args only, `shell:true` only for pnpm resolution on Windows; temp-file + rename writes.
- [x] 2.7 Run `pnpm --dir ui test`; confirm all GREEN.

## Phase 3: embed-assets.mjs CLI (PR1)

- [x] 3.1 `ui/scripts/embed-assets.mjs`: parse `--jucer <path>`; resolve repo root from `import.meta.url` (cwd is the vcxproj folder at pre-build); read flag; stub vs full; write `Source/ui/generated/EmbeddedAssets.{h,cpp}` and `generated/.stamp`; print `Berlin prebuild : error BRLnnn: ...` and exit 1 on any failure. Thin wrapper only (logic lives in the tested lib).
- [x] 3.2 Smoke (manual command, not a test): run `node ui/scripts/embed-assets.mjs --jucer Berlin.jucer` from `Builds/VisualStudio2026`; confirm stub files generated, exit 0, no pnpm invoked; rerun and confirm no file change (stamp skip).
- [x] 3.3 Smoke: with a PATH lacking pnpm and a temp copy of the `.jucer` with the flag on, confirm exit 1 and a `BRL002 ... pnpm not found` line. Delete the temp copy.

## Phase 4: WebAssets helper (PR1, strict TDD)

Files: `Source/ui/WebAssets.{h,cpp}`, `Tests/Source/WebAssetsTests.cpp`. juce_core only.

- [x] 4.1 RED `WebAssetsTests.cpp` (category `Berlin`): `/` and `/index.html` hit; `/assets/a.js` hit; unknown path -> nullopt; query/fragment stripped (`/index.html?x=1`); `../x`, `/a/../b`, `\x`, `/c:x`, `%2e`, `/%2e%2e/x` -> nullopt; empty table `{nullptr,0}` -> nullopt; MIME per extension (html js css svg json png woff2 ico), `app.JS` -> `text/javascript`, `file.xyz` and no extension -> `application/octet-stream`; `webViewUserDataFolder(tempBase)` ends with `Berlin/WebView2` under the base.
- [x] 4.2 Register `WebAssets.{h,cpp}` and `WebAssetsTests.cpp` in `Tests/BerlinTests.jucer` (own file ids; do NOT register generated files or WebEditor). Resave BerlinTests.jucer, build; confirm RED = compile failure (symbols missing).
- [x] 4.3 GREEN `WebAssets.h/.cpp`: `EmbeddedAsset`, `AssetTable`, `AssetResource`, `normaliseAssetPath`, `findAsset`, `mimeTypeForPath`, `getEmbeddedAssets()` declaration only (defined by the generated cpp).
- [x] 4.4 Run BerlinTests `--category=Berlin`; confirm 4.1 cases GREEN.

## Phase 5: WebEditorSupport helper (PR1, strict TDD)

Files: `Source/ui/WebEditorSupport.{h,cpp}`, `Tests/Source/WebEditorSupportTests.cpp`. Gui-free (links `UiBridge`).

- [x] 5.1 RED adapter `dispatchNativeCall(UiBridge&, args)`: valid `setBpm` with `{bpm:...}` -> ok + snapshot; empty array and `""` -> `missing arg: command`; non-string cmd -> `invalid type: command`; args absent and void -> treated as `{}` (no adapter error); args array and number -> `invalid type: args`; no crash on any.
- [x] 5.2 RED `PlayheadChangeDetector`: true before first commit; false after `commit(s)` for same state; true when step or playing differs; true again if no commit after a changed state (uncommitted stays changed).
- [x] 5.3 RED `devServerOrigin`: accepts `http://localhost:5173` and `http://127.0.0.1:5173`; rejects empty, `https://localhost:5173`, `http://example.com:5173`, `http://localhost` (no port), `http://localhost:5173.evil.com`.
- [x] 5.4 Register both new files + test in `Tests/BerlinTests.jucer`; resave; build; confirm RED (compile failure).
- [x] 5.5 GREEN `WebEditorSupport.h/.cpp`: `PlayheadState`, `PlayheadChangeDetector`, `dispatchNativeCall`, `devServerOrigin`.
- [x] 5.6 Run BerlinTests `--category=Berlin`; confirm 5.1-5.3 GREEN and total = 353 + new tests, no regressions.

## Phase 6: App .jucer edits, flag OFF (PR1)

Run the embed script once (task 3.2) BEFORE any resave so the generated files exist (D4).

- [x] 6.1 `Berlin.jucer`: set `JUCE_USE_WIN_WEBVIEW2="1"`; add `Source/ui/WebAssets.{h,cpp}`, `Source/ui/WebEditorSupport.{h,cpp}`, and generated `Source/ui/generated/EmbeddedAssets.{h,cpp}` (`compile="1"` on cpp); per-config (Debug and Release) `prebuildCommand`: `where node >nul 2>nul || (echo Berlin prebuild : error BRL001: node not found on PATH & exit /b 1) & node ..\..\ui\scripts\embed-assets.mjs --jucer ..\..\Berlin.jucer`. Do NOT add `BERLIN_WEB_UI` (flag stays off).
- [x] 6.2 `Plugin/BerlinPlugin.jucer`: same edits with one more `..\` level (`--jucer ..\..\..\BerlinPlugin.jucer` style path as appropriate; verify the relative path from the vcxproj folder) and Debug and Release prebuild in every target config.
- [x] 6.3 Resave both: `Projucer.exe --resave Berlin.jucer`, `Projucer.exe --resave Plugin\BerlinPlugin.jucer`. Each `.jucer` resaved from its own directory only. Confirm vcxproj changes are only the WebView2 NuGet package, new files and the pre-build event.
- [x] 6.4 Build `Berlin.sln` and Plugin `Berlin.sln` (Debug x64; also Release x64 for the prebuild path) with flag OFF. First NuGet restore needs network: coordinate with the user (see 9.1). Confirm 0 errors, warnings equal to Phase 0, stub generated, exit 0, and the build works in a PATH without pnpm.
- [x] 6.5 Rebuild `BerlinTests.sln`, run `--category=Berlin`; confirm count equals Phase 5.6 total.
- [x] 6.6 Run `pnpm --dir ui test`; confirm green. Run `git status`; confirm `Source/ui/generated/*` does not appear.

## Phase 7: PR1 review gate and commit

- [x] 7.1 Check size: PR1 authored lines (exclude lockfile, generated, vcxproj) within the 800 budget (~540 forecast). **Result: ~880 authored lines (380 of them tests), over budget. User accepted `size:exception` for PR1 on 2026-10-03, rejecting a PR1a/PR1b split.**
- [x] 7.2 Run bounded review (`gentle-ai review start`, then lenses, then finalize) on the staged PR1 target; obtain an approved lineage. **Done: review-3ba7e7dbd902a4b6 approved (4R, no blockers), bound via bind-sdd before commit; PR1 commit 9df6d61 pushed; pre-push/pre-pr allow.**
- [x] 7.3 Run `gentle-ai review bind-sdd --change webview-poc` with the approved lineage BEFORE `git commit` (Slice 0 lesson). Then `gentle-ai review validate --gate pre-commit`. **Done: review-3ba7e7dbd902a4b6 approved (4R, no blockers), bound via bind-sdd before commit; PR1 commit 9df6d61 pushed; pre-push/pre-pr allow.**
- [x] 7.4 Commit with a conventional message, no AI attribution (e.g. `feat: add web ui build plumbing and gui-free helpers`). Verify git state yourself. **Done: review-3ba7e7dbd902a4b6 approved (4R, no blockers), bound via bind-sdd before commit; PR1 commit 9df6d61 pushed; pre-push/pre-pr allow.**
- [x] 7.5 Push and open PR1 targeting `feat/ui-engine-api-pr2`, with Chain Context + dependency diagram (PR1 marked), start/end, follow-up = PR2, out-of-scope = WebEditor/UI. Validate pre-push/pre-pr gates. **Done: review-3ba7e7dbd902a4b6 approved (4R, no blockers), bound via bind-sdd before commit; PR1 commit 9df6d61 pushed; pre-push/pre-pr allow.**

## Phase 7b: PR1 review follow-ups (PR2)

- [x] 7b.1 RES-1/REL-1: Projucer has no per-target pre-build (`prebuildCommand` is per configuration, applied to every project), so the script is made concurrency-safe: unique tmp names (pid+random), rename retry on EPERM/EBUSY/ENOENT (`renameWithRetry`), lock file `Source/ui/generated/.lock` with stale-lock timeout (`withLock`) and a stamp re-check inside the lock. Vitest covers both mechanisms and the lock/stamp interplay.
- [x] 7b.2 RES-2: both `.jucer` prebuild commands end the embed call with `|| exit /b 1`; pnpm `spawnSync` has a 5 min timeout, reported as `BRL006: pnpm ... timed out`; Vitest covers the timeout path via the injected spawn.
- [x] 7b.3 REL-2/READ-4: `devServerOrigin` port must be 1-65535 (named constants); RED tests first (0, 00000, 65536, 99999 rejected; 1, 65535 accepted).
- [x] 7b.4 REL-3/READ-2: absent/void-args test now proves args became `{}` (`mutate` succeeds with no args; `setBpm` with no args returns the bridge's own `missing arg: bpm`); misleading comment fixed.
- [x] 7b.5 READ-1 (no-op `.map` removed from `stampInputs`), READ-3 (`mimeTypeForPath` guards before computing the extension), READ-5 (generated header no longer redeclares `getEmbeddedAssets`; `BERLIN_EMBEDDED_ASSETS_FULL` documented, consumed by WebEditor's `static_assert`).

## Phase 8: WebEditor, factory, host wiring (PR2; branch `feat/ui-webview-poc-pr2` off PR1)

No automated test is possible for GUI code (BerlinTests stays gui-free). Behavior it relies on is already covered by Phases 4-5. Write the code to thin-adapter shape; verify by build + manual phase.

- [x] 8.1 Re-run baselines on the PR2 branch (BerlinTests count, both app builds flag off) to confirm PR1 as the starting point. **Done: Baseline on PR2 branch: BerlinTests 367 before follow-ups; both app builds flag off OK.**
- [x] 8.2 `Source/ui/EditorFactory.{h,cpp}`: `createBerlinEditor(BerlinAudioProcessor&)` returning `juce::AudioProcessorEditor*` / unique_ptr; single `#if BERLIN_WEB_UI` (WebEditor vs the legacy `BerlinAudioProcessorEditor`) (D6).
- [x] 8.3 `Source/ui/WebEditor.{h,cpp}`: derives `juce::AudioProcessorEditor`; entire body inside `#if BERLIN_WEB_UI`; `static_assert(BERLIN_EMBEDDED_ASSETS_FULL)` (D2); member order `processor&`, `UiBridge`, `PlayheadChangeDetector`, `fallbackLabel`, `unique_ptr<WebBrowserComponent> browser`; constructor `setSize(800,680)`, `createDirectory()` on the data folder, options with `Backend::webview2`, `withNativeFunction` `dispatch` (via `dispatchNativeCall`) and `getSnapshot`, `withResourceProvider` (via `findAsset`/`mimeTypeForPath`), debug-only dev origin under `#if JUCE_DEBUG`; `areOptionsSupported` false or folder failure -> show fallback label, no browser, no timer; else `goToURL(getResourceProviderRoot())` and `startTimerHz(30)`; `timerCallback` emits `playhead` via `emitEventIfBrowserIsVisible`, calling `detector.commit` only when sent; destructor `stopTimer()` then `browser.reset()`.
- [x] 8.4 `Source/plugin/BerlinAudioProcessor.cpp`: `createEditor` calls `createBerlinEditor` inside the non-`BERLIN_HEADLESS` branch only (BerlinTests must not pull GUI code).
- [x] 8.5 `Source/MainComponent.{h,cpp}`: `std::unique_ptr<juce::Component> editor` declared after `processor`; legacy path keeps its current size and behavior (no clipping fix).
- [x] 8.6 Register `EditorFactory.{h,cpp}`, `WebEditor.{h,cpp}` in `Berlin.jucer` and `Plugin/BerlinPlugin.jucer` only (not BerlinTests). Resave both after running the embed script.
- [x] 8.7 Build both app targets flag OFF; confirm 0 errors, warnings equal to baseline, legacy editor identical (WebEditor compiles to nothing). **Done: Debug+Release x64 Rebuild both solutions flag OFF, PATH without pnpm: 0 errors, warnings = baseline (app 2 C4100, plugin 0).**

## Phase 9: React UI and its tests (PR2, strict TDD)

- [x] 9.1 Copy JUCE `native/javascript/index.js` to `ui/src/bridge/juce/index.js` (not counted).
- [x] 9.2 RED Vitest `state/steps.test.ts` `stepRow(playheadStep, playing)`: 16 entries; exactly one active when playing at a step; none active when stopped; out-of-range/negative step -> none active.
- [x] 9.3 RED Vitest `bridge/native.test.ts` against a mock `window.__JUCE__`: `dispatch(cmd,args)` calls the native function and returns its result; `getSnapshot()` returns the snapshot; playhead event listener subscribe/unsubscribe; missing `__JUCE__` degrades without throwing.
- [x] 9.4 RED Vitest `App.test.tsx` (react-dom test, small): shows BPM from initial snapshot; + and - issue `dispatch("setBpm", ...)` with bpm+1 and bpm-1; playhead event moves the highlighted step.
- [x] 9.5 Run `pnpm --dir ui test`; confirm 9.2-9.4 FAIL.
- [x] 9.6 GREEN `ui/src/state/steps.ts`, `bridge/native.ts`, `App.tsx`, `main.tsx`, `ui/index.html`; no controls beyond BPM -/+ and the 16-step row.
- [x] 9.7 Run `pnpm --dir ui test` and `pnpm --dir ui run typecheck`/`build` (add scripts if missing); confirm GREEN and `ui/dist` produced. **Done: Vitest 51 passing (was 18); typecheck clean; `vite build` produces ui/dist.**

## Phase 10: Flag-on verification (PR2)

- [x] 10.1 Temporarily add `BERLIN_WEB_UI=1` to `JUCERPROJECT@defines` of `Berlin.jucer` (plugin: new line `&#10;` after `JUCE_VST3_CAN_REPLACE_VST2=0`), resave, build standalone and VST3 (Debug x64): confirm the full pipeline (`pnpm install --frozen-lockfile`, build, embed) runs, `static_assert` passes, and a second build skips via stamp. **Done: Flag-on Rebuild of both solutions (Debug x64): pipeline ran, static_assert passed, second build skipped via stamp.**
- [x] 10.2 Negative checks: flag on with pnpm removed from PATH -> build fails with `BRL002 ... pnpm not found`; flag on with stub assets forced -> compile fails on `static_assert` (D2). Revert any temporary edits. **Done: Verified: no pnpm on PATH -> BRL002 exit 1; forcing FULL 0 -> C2338 static_assert failure. Edits reverted.**
- [x] 10.3 Decide with the user whether the flag-on `.jucer` edit is reverted to OFF before commit (default: committed OFF; flag stays off by default). Rebuild flag-off and run `BerlinTests --category=Berlin`: count unchanged from PR1. **Done: Default applied: committed OFF (.jucer/vcxproj byte-identical to pre-flag state, no residual BERLIN_WEB_UI). BerlinTests 368 (367 + 1 port-range test).**

## Phase 11: PR2 review gate and commit

- [x] 11.1 Check size: PR2 authored lines within the 800 budget (~370 forecast); confirm total across PRs ~910. **Result: ~885 authored lines (~375 tests, ~330 PR1 review follow-ups), over budget. User accepted `size:exception` for PR2 on 2026-10-03, rejecting a split.**
- [x] 11.2 Run bounded review on the staged PR2 target; obtain an approved lineage. **Approved: lineage review-90deccba3fb1ae28 (high tier, 4R, 34 files), no blockers; Vitest 51, BerlinTests 1223 test cases.**
- [x] 11.3 Run `gentle-ai review bind-sdd --change webview-poc` with the approved lineage BEFORE `git commit`. Then validate the pre-commit gate. **Bound (revision sha256:5c6727f7...); pre-commit allow.**
- [x] 11.4 Commit (conventional, no AI attribution, e.g. `feat: add WebView2 editor and PoC react ui behind BERLIN_WEB_UI`). Verify git state yourself. **Commit 55cec13, verified.**
- [x] 11.5 Push and open PR2 targeting the PR1 branch, with Chain Context + dependency diagram (PR2 marked). Validate pre-push/pre-pr gates. **Pushed to origin/feat/ui-webview-poc-pr2 (verified); pre-push and pre-pr gates allow against origin/feat/ui-webview-poc. PR opened by the user (not verified here: gh is not installed).**

## Phase 12: Manual go/no-go (HUMAN-ONLY, not automatable; flag-on build)

Who: the user. The first NuGet restore (WebView2 package, needs network) is done by the user (or in a user-observed build), before the apply agent's build verification in 6.4. Any failure below is a no-go (fall back to plain JUCE).

- [x] 12.1 **MANUAL** Standalone `Berlin.exe` (flag on): UI renders, BPM -/+ changes the engine BPM, playhead row follows playback.
- [x] 12.2 **MANUAL** Cakewalk Sonar with `BerlinPlugin.vst3` (flag on): same checks as 12.1.
- [x] 12.3 **MANUAL** Two plugin instances open simultaneously in Sonar operate independently, no crash.
- [x] 12.4 **MANUAL** Correct layout/text at 100% and 150%+ DPI (standalone and Sonar).
- [x] 12.5 **MANUAL** Keyboard focus: input works inside the web view; host shortcuts are not stolen when focus is outside it.
- [x] 12.6 **MANUAL** Close and reopen the editor repeatedly: engine state intact, no leak/crash.
- [ ] 12.7 **MANUAL** Missing-runtime fallback: simulate WebView2 runtime absence (or an unsupported options path); native fallback label shows, host stable. **Not run: Windows 11 ships the WebView2 runtime, so its absence cannot be simulated here. The fallback path is covered by code review and the resized() fix; deferred to a machine without the runtime.**
- [x] 12.8 **MANUAL** Flag-off builds unchanged: legacy editor identical in standalone and Sonar; flag-off build succeeds on a machine/PATH without pnpm; BerlinTests and Vitest pass. **Automated parts PASS (apply 2026-10-03: flag-off Debug+Release builds of both solutions at baseline warnings with pnpm absent from PATH; BerlinTests 368, Vitest 51). Manual check PASS (2026-10-03): after a flag-off Debug rebuild of both solutions (stub assets, BERLIN_EMBEDDED_ASSETS_FULL 0), the user confirmed the legacy editor works in standalone and Sonar.**
- [x] 12.9 Record the go/no-go decision in the PR2 description and in Engram; if no-go, stop Slices 2-5.

**Go/no-go result (2026-10-03): GO.** The user ran 12.1-12.6 on flag-on Debug builds (standalone and Cakewalk Sonar) and all passed: the UI renders, BPM -/+ drives the engine, the playhead follows playback, two instances work, DPI, focus and close/reopen are fine. The first run showed a blank window: the WebView stayed at 0x0 because `setSize()` ran `resized()` before the browser existed. Fixed in `WebEditor.cpp` by calling `resized()` after creating the browser and the fallback label, and confirmed via a DevTools probe (innerWidth/innerHeight 800x680, "+" changed BPM 120 to 121).

## Delivery Notes

- PR1 is delivered as `size:exception`: ~880 authored lines against the 800 budget (forecast ~540). Of these, 380 are tests (C++ 231, Vitest 149). User decision, 2026-10-03.
- PR2 is delivered as `size:exception`: ~885 authored lines against the 800 budget (forecast ~370). About 375 are tests and about 330 are PR1 review follow-ups (Phase 7b). User decision, 2026-10-03.
- Open Question carried over: the Visual Studio fast up-to-date check can skip the pre-build after edits made only under `ui/`; use Rebuild in that case (accepted for the PoC).
- Resave discipline: each `.jucer` is resaved only from its own directory, and only after the embed script has generated the stub (D4, D7).
- Threat matrix: only the pre-build subprocess boundary applies; its RED cases are Phase 2.4 and the smoke checks 3.2-3.3.
- Diffs under review exclude `ui/pnpm-lock.yaml`, `ui/src/bridge/juce/index.js`, `Source/ui/generated/*` and Projucer-regenerated vcxproj/JuceLibraryCode files.
