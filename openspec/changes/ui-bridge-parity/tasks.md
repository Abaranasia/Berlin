# Tasks: Full Bridge + Functional Parity (UI Redesign Slice 2/6)

Strict TDD is active: every behavior task is RED (write the failing test, run it, confirm it fails for the right reason) then GREEN (minimal code) then run the suite. Package manager is pnpm only (`pnpm --dir ui ...`); npm is broken, never use it. MSBuild: `C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe`. Projucer: `H:\Proyectos\Juce\Projucer\Projucer.exe --resave <jucer>`. C++ tests live in `Tests/Source/*` and run via `Tests/Builds/VisualStudio2026/x64/Debug/ConsoleApp/BerlinTests.exe --category=Berlin`. All four C++ test files this slice touches (`WebEditorSupportTests`, `PresetManagerFileTests`, `BerlinAudioProcessorTests`, `UiBridgeTests`) are already registered in `Tests/BerlinTests.jucer` (checked), so no `.jucer` test registration and no resave is needed for BerlinTests.

## Review Workload Forecast

| Field | Value |
|---|---|
| Estimated authored lines | ~2,680 total (excludes lockfiles, generated assets, `ui/src/bridge/juce/index.js`, Projucer-regenerated files) |
| Review budget | 800 authored lines per PR |
| Delivery strategy | auto-chain |
| Chain strategy | feature-branch-chain, stacked (each PR targets the previous PR branch). Skill: `gentle-ai-chained-pr` is not in the skill registry; fell back to `C:\Users\ran_k\.claude\skills\chained-pr\SKILL.md` (found, used for Slice 1) |

| PR | Branch / base | Boundary | Est. lines | Verification | Rollback |
|---|---|---|---|---|---|
| PR1 | `feat/ui-bridge-parity` -> `feat/ui-webview-poc-pr2` (branch exists) | C++ host: gate, dialogs, `snapshot` event, `busy`, preset cache, `owner` removal | ~420 (prod ~230, tests ~190); risk Low | `BerlinTests --category=Berlin`; both app builds flag off and flag on | Revert PR1: UI still sends PoC commands; additive `busy` and cache are internal |
| PR2 | `feat/ui-bridge-parity-pr2` -> PR1 | TS foundation: protocol, limits, status, bridges, store, dev script, App rewire | ~760; risk Medium (95% of budget) | `pnpm --dir ui test`; `typecheck`; `build` + no mock chunk | Revert PR2: C++ host stays inert; Slice 1 UI is restored |
| PR2a | `feat/ui-bridge-parity-pr2` -> PR1 | Split of PR2: dev script, protocol, limits, status, `Bridge` interface, mock bridge, `assertNoMockChunk` (Phases 6-8 and part of 9) | ~626 measured | `pnpm --dir ui test`; `typecheck`; `build` + no mock chunk | Revert PR2a: nothing imports the new modules yet; Slice 1 UI unchanged |
| PR2b | `feat/ui-bridge-parity-pr2b` -> PR2a | Split of PR2: `nativeBridge` rewrite, `resolveBridge`, store, context, App/main rewire (rest of Phase 9, Phases 10-11) | ~551 measured | `pnpm --dir ui test`; `typecheck`; `build` + no mock chunk | Revert PR2b: PR2a modules stay unused; Slice 1 UI is restored |
| PR3 | `feat/ui-bridge-parity-pr3` -> PR2b (`feat/ui-bridge-parity-pr2b`) | Controls A: primitives, status, transport, toggles, generation, rhythm, pitch, evolution, export | ~760; risk Medium (95%) | `pnpm --dir ui test` | Revert PR3: store and bridges stay, panels vanish |
| PR4 | `feat/ui-bridge-parity-pr4` -> PR3 | Controls B: synth, skew, delay, reverb, presets | ~740; risk Medium (93%) | `pnpm --dir ui test` | Revert PR4: Controls A stays |

Proposed split points if a PR measures above 800 during apply (do not exceed; stop and split instead):
- PR2 -> PR2a (protocol, limits, status, bridge/native, resolveBridge, dev script) + PR2b (mock, `assertNoMockChunk`, store, App rewire). Cut at Phase 9/10.
- PR3 -> PR3a (primitives, seed, status line, transport, toggles) + PR3b (generation, rhythm, pitch, evolution, export). Cut at Phase 16/17.
- PR4 -> PR4a (skew, delayRecs, synth, delay, reverb) + PR4b (presets UI, composition). Cut at Phase 24/25.

Decision needed before apply: No
Chained PRs recommended: Yes
Chain strategy: feature-branch-chain
400-line budget risk: High (PR1 Low; PR2-PR4 Medium against the 800 project budget)

Dependency diagram (each PR body must repeat it, marking the current PR):

    feat/ui-webview-poc-pr2 (Slice 1 PR2)
      <- PR1 feat/ui-bridge-parity       (C++ host)
           <- PR2a feat/ui-bridge-parity-pr2   (TS foundation: protocol, limits, status, mock)
                <- PR2b feat/ui-bridge-parity-pr2b  (TS foundation: native, store, App rewire)
                     <- PR3 feat/ui-bridge-parity-pr3  (Controls A)
                          <- PR4 feat/ui-bridge-parity-pr4  (Controls B)

### Suggested Work Units

| Unit | Goal | Likely PR | Focused test command | Runtime harness | Rollback boundary |
|---|---|---|---|---|---|
| 1 | Host emits `snapshot`, dialogs, busy, cached preset list | PR1 | `BerlinTests.exe --category=Berlin` | Flag-on standalone build; Phase 17 manual | `Source/ui/*`, `Source/preset/*`, processor busy path |
| 2 | Typed protocol, bridges, store, dev script | PR2 | `pnpm --dir ui test` | `pnpm --dir ui dev` against the mock | `ui/src/bridge`, `ui/src/store`, `ui/scripts` |
| 3 | Transport, generation, rhythm, pitch, evolution, export | PR3 | `pnpm --dir ui test` | Flag-on build; Phase 17 manual | `ui/src/components/**` (A) |
| 4 | Synth, delay, reverb, presets | PR4 | `pnpm --dir ui test` | Flag-on build; Phase 17 manual | `ui/src/components/**` (B) |

## Phase 0: Baseline (PR1 branch `feat/ui-bridge-parity`)

- [x] 0.1 `git status` and `git branch --show-current`: confirm branch `feat/ui-bridge-parity` exists and its base is `feat/ui-webview-poc-pr2` (verify git yourself).
- [x] 0.2 Build `Builds/VisualStudio2026/Berlin.sln` and `Plugin/Builds/VisualStudio2026/Berlin.sln` (Debug x64, `/t:Rebuild`, flag off); record exit codes and warnings.
- [x] 0.3 Build `Tests/Builds/VisualStudio2026/BerlinTests.sln`; run `BerlinTests.exe --category=Berlin`; record the pass count (Slice 1 ended at 368; use the measured value).
- [x] 0.4 `pnpm --dir ui test`; record the Vitest count (Slice 1: 51).

## Phase 1: PendingEventGate and chooser helpers (PR1, strict TDD)

Files: `Source/ui/WebEditorSupport.{h,cpp}`, `Tests/Source/WebEditorSupportTests.cpp` (spec: web-editor "Snapshot Event On Engine Change", "Export File Chooser Native Function").

- [x] 1.1 RED gate tests: initially `shouldEmit(true)` is false; `markPending` + hidden -> false; pending + visible -> true; no `commit` -> still true; `commit` -> false; multiple `markPending` then one `commit` -> false (coalesce).
- [x] 1.2 RED chooser tests: `defaultExportFile(docs)` == `docs/Berlin/berlin-export.mid`; `exportChooserResult(File())` == `{cancelled:true,path:""}`; non-empty file -> `{cancelled:false,path:<full path>}`; `busyChooserResult()` == `{cancelled:true,path:"",error:"busy"}`.
- [x] 1.3 Build BerlinTests; confirm 1.1-1.2 fail (compile failure, symbols missing).
- [x] 1.4 GREEN `PendingEventGate`, `defaultExportFile`, `exportChooserResult`, `busyChooserResult` in `WebEditorSupport.{h,cpp}` (D1, D2; contracts per design).
- [x] 1.5 Run `--category=Berlin`; confirm 1.1-1.2 GREEN, no regressions.

## Phase 2: PresetManager per-file validity cache (PR1, strict TDD)

Files: `Source/preset/PresetManager.{h,cpp}`, `Tests/Source/PresetManagerFileTests.cpp`, `Tests/Source/UiBridgeTests.cpp` (spec: preset-persistence, ui-bridge "Snapshot presetNames Is Always Current").

- [x] 2.1 RED "cached equals uncached": a populated manager's `listPresetNames()` equals a fresh manager's scan of the same folder (valid, invalid and non-`.xml` files).
- [x] 2.2 RED "unchanged not re-parsed": the second `listPresetNames()` leaves `parseCount()` unchanged.
- [x] 2.3 RED "changed re-parsed": rewriting a file (different size, then `setLastModificationTime` only) increments `parseCount()`; a file corrupted in place is delisted.
- [x] 2.4 RED "added and removed reflected": a new valid file appears, a deleted file disappears and its cache entry is pruned.
- [x] 2.5 RED `UiBridgeTests`: snapshot `presetNames` shows an externally added file on the next command and drops a removed one.
- [x] 2.6 Build; confirm 2.1-2.5 fail (compile failure: `parseCount` missing).
- [x] 2.7 GREEN `PresetManager.{h,cpp}`: `mutable std::map<String, Entry{size, mtime, optional<String> name}>` under `std::mutex`, reuse on size+mtime match, replace map with seen entries, `parseCount()` seam (D4).
- [x] 2.8 Run `--category=Berlin`; confirm GREEN.

## Phase 3: loadPreset busy result (PR1, strict TDD)

Files: `Source/preset/Preset.h`, `Source/plugin/BerlinAudioProcessor.cpp`, `Source/plugin/BerlinAudioProcessorEditor.cpp`, `Source/bridge/UiBridge.cpp`, `Tests/Source/{BerlinAudioProcessorTests,UiBridgeTests}.cpp` (spec: ui-bridge "loadPreset while regeneration is busy").

- [x] 3.1 RED processor test: `loadPreset` while `player.isPublishPending()` returns `PresetResult::busy` and leaves patch, params, seed and bpm unchanged.
- [x] 3.2 RED processor test: `regenerate(false)` returning false after a successful `presetManager.load` -> `loadPreset` returns `busy`. (NOT unit-testable: the D3 pending pre-check makes the refusal unreachable and there is no seam; the defensive `regenerate(false) ? ok : busy` branch is covered by code review only.)
- [x] 3.3 RED `UiBridgeTests`: `loadPreset` while busy -> `ok:false`, `error:"busy"`, no snapshot in the envelope; `loadPreset` success unchanged.
- [x] 3.4 RED legacy mapping test (if `describePresetFailure` is reachable gui-free; otherwise covered by the 4.5 code check): `busy` maps to `Busy, try again`. (Not gui-free: verified by code check and the flag-off App/Plugin builds.)
- [x] 3.5 Build; confirm 3.1-3.3 fail (enum value missing).
- [x] 3.6 GREEN add `PresetResult::busy` (`Preset.h`); `BerlinAudioProcessor::loadPreset` checks pending before `presetManager.load` and honors the `regenerate(false)` result (D3).
- [x] 3.7 GREEN `UiBridge.cpp` `presetResultToken` maps `busy` -> `"busy"`; `BerlinAudioProcessorEditor.cpp` `describePresetFailure` maps `busy` -> `Busy, try again` (D3).
- [x] 3.8 Run `--category=Berlin`; confirm GREEN.

## Phase 4: WebEditor wiring (PR1; no automated test possible for GUI code)

Behavior it relies on is covered by Phases 1-3. Write to thin-adapter shape; verify by build and the manual phase. Files: `Source/ui/WebEditor.{h,cpp}` (all inside `#if BERLIN_WEB_UI`).

- [x] 4.1 D5: delete the `owner` member; add private `BerlinAudioProcessor& engine() noexcept` (`static_cast` of `processor`); replace all uses.
- [x] 4.2 D1: `WebEditor : juce::ChangeListener`; `addChangeListener(&engine())` in the constructor; `changeListenerCallback` only calls `gate.markPending()`; `timerCallback` emits `snapshot` when `gate.shouldEmit(browser && browser->isVisible())` then `gate.commit()` (after the send). Destructor order: `removeChangeListener`, `shuttingDown = true`, `stopTimer`, `exportChooser.reset()`, `confirmBox = {}`, `browser.reset()`.
- [x] 4.3 D2: one `dialogOpen` flag. `chooseExportFile` native function: busy -> `busyChooserResult()`; else `launchAsync(save | canSelectFiles | warnAboutOverwriting)` on `std::unique_ptr<juce::FileChooser>` with `defaultExportFile(userDocumentsDirectory)`; result via `exportChooserResult`. `confirm(title, message)`: busy -> `false`; else `NativeMessageBox::showScopedAsync(makeOptionsOkCancel(...))` in a `juce::ScopedMessageBox`. Callbacks capture `SafePointer<WebEditor>`, clear `dialogOpen`, and skip `done` when `safe == nullptr || safe->shuttingDown`.
- [x] 4.4 Code-review check of listener-removed-first and destructor order against the web-editor spec scenario "Listener removed first on destruction".
- [x] 4.5 Confirm `EditorFactory` and the legacy editor compile path are untouched except `describePresetFailure` (3.7).

## Phase 5: PR1 verification, review gate, commit (PR1)

- [x] 5.1 Flag off: rebuild both app solutions (Debug and Release x64), PATH without pnpm; 0 errors, warnings equal to 0.2 baseline; legacy editor unchanged.
- [x] 5.2 Flag on (temporary, reverted before commit): add `BERLIN_WEB_UI=1` to `JUCERPROJECT@defines`, resave, build both solutions Debug x64; `WebEditor` compiles and links. Revert and resave; confirm `.jucer`/vcxproj diff is empty.
- [x] 5.3 Rebuild `BerlinTests.sln`; run `--category=Berlin`; count = 0.3 + new tests.
- [x] 5.4 `pnpm --dir ui test` still green (PR1 touches no TS).
- [x] 5.5 Size check: PR1 authored lines within 800 (~420 forecast); report the measured number.
- [x] 5.6 Run bounded review: `gentle-ai review start` on the staged PR1 target, run the selected lenses, finalize; obtain an approved lineage. **Approved: lineage review-ceecf91296eb8303 (high, 4R), suggestions only.**
- [x] 5.7 `gentle-ai review bind-sdd --change ui-bridge-parity` with the approved lineage BEFORE `git commit`; then `gentle-ai review validate --gate pre-commit`. **Bound after commit (the committed tree b27d22b equals the reviewed candidate tree); revision sha256:5dc6b9b3....**
- [x] 5.8 USER commits (conventional, no AI attribution, e.g. `feat: add snapshot event, native dialogs and preset cache to web editor`) and pushes. Verify git state yourself. **Commit 7e5a627 "chore: add C++ host", pushed.**
- [x] 5.9 Validate pre-push and pre-pr: `gentle-ai review validate --gate <gate> --base-ref origin/feat/ui-webview-poc-pr2`. PR1 body: Chain Context, dependency diagram (PR1 marked), follow-up = PR2. **pre-push and pre-pr allow.**

## Phase 6: Dev script and config (PR2; branch `feat/ui-bridge-parity-pr2` off PR1)

- [x] 6.1 Create branch `feat/ui-bridge-parity-pr2` from PR1; re-run 0.3/0.4 baselines. **Branch pre-created off 7e5a627; baselines: Vitest 51, BerlinTests 379.**
- [x] 6.2 RED `ui/scripts/dev-config.test.mjs`: `package.json` has `"dev": "vite"`; `vite.config.ts` pins `server.port: 5173` and `strictPort: true` (spec "Pinned port").
- [x] 6.3 GREEN `ui/package.json` dev script, `ui/vite.config.ts` server block, `ui/src/vite-env.d.ts` (`/// <reference types="vite/client" />`) (D10).

## Phase 7: Protocol and limits (PR2, strict TDD)

Files: `ui/src/bridge/{protocol,limits}.ts`, `ui/src/bridge/protocol.test.ts`, `ui/scripts/limits-drift.test.mjs` (spec: "Typed Protocol", "Limits Drift Guard").

- [x] 7.1 RED `protocol.test.ts`: enum literal lists (`Waveform`, `LfoDestination`, `DelayDivision`, `RhythmMode`, `ScaleType`) equal the `ui-bridge` Encoding Conventions; fixture `Snapshot` and `DispatchResult` typecheck; a `// @ts-expect-error` unknown command name.
- [x] 7.2 RED `limits-drift.test.mjs`: reads `../../Source/synth/SynthPatch.h` via `fs` + `import.meta.url`, regex-parses `kMin*/kMax*` (including bpm) and compares with `limits.ts` (D9).
- [x] 7.3 Run `pnpm --dir ui test`; confirm 7.1-7.2 fail (modules missing).
- [x] 7.4 GREEN `protocol.ts` (hand-written): `Command` union of 15 `{name,args}` variants (`Partial<...>` for `setPatch`/`setGenerationParams`), `Snapshot`, `DispatchResult`, `PlayheadEvent`, `ChooseExportResult`, `ErrorToken`, enum unions (D6).
- [x] 7.5 GREEN `limits.ts` matching `SynthPatch.h`.
- [x] 7.6 Run `pnpm --dir ui test` and `pnpm --dir ui run typecheck`; confirm GREEN.

## Phase 8: status.ts (PR2, strict TDD)

Files: `ui/src/store/status.ts`, `ui/src/store/status.test.ts` (spec: "Status Line And Error Messages").

- [x] 8.1 RED table test over every row of the design Status Messages table: `busy`, `exists`, preset tokens, export tokens (`writeFailed` command-dependent: preset vs export), `path not absolute`, thrown -> `Error: <message>`, unmapped token -> raw token, success messages.
- [x] 8.2 Run `pnpm --dir ui test`; confirm 8.1 fails.
- [x] 8.3 GREEN `describe(command, token)` per the table (D12); error rows flagged for the error style.
- [x] 8.4 Run `pnpm --dir ui test`; confirm GREEN.

## Phase 9: Bridge, native, mock, resolveBridge (PR2, strict TDD)

Files: `ui/src/bridge/{bridge,native,mock,index}.ts` and tests, `ui/scripts/embed-lib.mjs`, `ui/scripts/embed-lib.test.mjs` (spec: "Bridge Interface And Implementations", "Mock Bridge Is Dev-Only").

PR2 split: 9.2, 9.4, 9.7, 9.9 and the `bridge.ts` half of 9.6 ship in PR2a; 9.1, 9.3, the `native.ts` half of 9.6 and 9.8 moved to PR2b (code held outside the repo until branch `feat/ui-bridge-parity-pr2b` exists).

- [ ] 9.1 (PR2b) RED `native.test.ts` (rewritten against `window.__JUCE__`): `dispatch`, `getSnapshot`, `chooseExportFile`, `confirm` forward to native functions; `onPlayhead` and `onSnapshot` subscribe and unsubscribe; missing host degrades without throwing.
- [x] 9.2 RED `mock.test.ts`: envelopes `{ok,...snapshot}`; value clamps to `limits.ts`; mock reproduces error tokens (`missing arg`, `invalid enum`, `exists`, `busy`).
- [ ] 9.3 (PR2b) RED `index.test.ts` `resolveBridge({hasHost, loadMock})` matrix: host -> native; no host + `loadMock` -> mock; no host + undefined -> native (degrades).
- [x] 9.4 RED `embed-lib.test.mjs` `assertNoMockChunk(files)`: throws on any asset name matching `/mock/`; passes otherwise.
- [x] 9.5 Run `pnpm --dir ui test`; confirm 9.1-9.4 fail. **Done before the split; PR2b re-confirms 9.1 and 9.3 RED on its branch.**
- [ ] 9.6 GREEN `bridge.ts` (`Bridge` interface) and rewrite `native.ts` as `nativeBridge` (D7). **Split: `bridge.ts` done in PR2a; the `native.ts` rewrite is in PR2b.**
- [x] 9.7 GREEN `mock.ts` (`mockBridge`, minimal engine, same tokens).
- [ ] 9.8 (PR2b) GREEN `index.ts` `resolveBridge`.
- [x] 9.9 GREEN `embed-lib.mjs` `assertNoMockChunk` and call it in the embed step.
- [ ] 9.10 Run `pnpm --dir ui test`; confirm GREEN. **PR2a part green (125 tests, 2026-10-03); re-run on PR2b once 9.1, 9.3, 9.6 and 9.8 land.**

## Phase 10: External store (PR2b, strict TDD)

PR2 split: this phase moved to PR2b; its code was written during the PR2 apply and is held outside the repo until branch `feat/ui-bridge-parity-pr2b` exists.

Files: `ui/src/store/{store,context}.ts(x)`, `ui/src/store/store.test.ts` (spec: store, optimistic, stale, failures requirements; D8, D13).

- [ ] 10.1 RED slices: `engine`, `overlay`, `playhead`, `status`, `draft`; selector isolation (an unrelated key does not notify); `snapshot` event updates `engine`.
- [ ] 10.2 RED coalescing: 3 rapid `send` on one key -> 2 dispatches, last value wins; different keys do not block each other.
- [ ] 10.3 RED optimistic and ordering: overlay shows instantly; out-of-order response (`seq <= lastAppliedSeq`) dropped but its key still settles; a `snapshot` event does not clobber an overlay until the key settles.
- [ ] 10.4 RED failure: `ok:false` with nothing pending drops the overlay and sets status from the token; a newer pending value keeps its overlay and is still sent.
- [ ] 10.5 RED rejection: a thrown or rejected dispatch never escapes; status becomes `Error: <message>`.
- [ ] 10.6 Run `pnpm --dir ui test`; confirm 10.1-10.5 fail.
- [ ] 10.7 GREEN `store.ts` `createStore(bridge)` with `send(key, cmd, optimistic)`, global `seq`, `useSyncExternalStore` selectors, `.catch` everywhere.
- [ ] 10.8 GREEN `context.ts` provider and `useStore` hook.
- [ ] 10.9 Run `pnpm --dir ui test`; confirm GREEN.

## Phase 11: App and main rewire (PR2b, strict TDD)

PR2 split: this phase moved to PR2b; in PR2a `App.tsx`, `main.tsx` and `App.test.tsx` stay at their PR1 versions.

Files: `ui/src/App.tsx`, `ui/src/main.tsx`, `ui/src/App.test.tsx` (spec: "Rapid BPM presses"; fixes the Slice 1 `changeBpm` stale closure).

- [ ] 11.1 RED rewrite `App.test.tsx` with `react-dom/client` + `act` and a fake `Bridge` (drop `vi.mock('./bridge/native')`): BPM from snapshot; + and - send `setBpm` through the store; rapid presses coalesce and end at the last value; playhead event moves the active step.
- [ ] 11.2 Run `pnpm --dir ui test`; confirm 11.1 fails.
- [ ] 11.3 GREEN `App.tsx` takes `store` as a prop and uses selectors; `main.tsx` calls `resolveBridge({hasHost, loadMock: import.meta.env.DEV ? () => import('./bridge/mock').then(m => m.mockBridge) : undefined})` (D7, D11).
- [ ] 11.4 Run `pnpm --dir ui test`, `typecheck`, `build`; confirm `ui/dist` contains no mock chunk (`assertNoMockChunk` passes).

## Phase 12: PR2a verification, review gate, commit (PR2a, branch `feat/ui-bridge-parity-pr2`)

- [ ] 12.1 Flag-off builds of both app solutions and `BerlinTests` unchanged (PR2 touches no C++); flag-on standalone build embeds the new UI and loads (smoke). **Partial: BerlinTests rebuilt and 379/379 green (PR2 touches no C++; embed-lib.mjs changed); flag-off app solutions and flag-on standalone smoke not run.**
- [ ] 12.2 Size check: PR2 authored lines within 800 (~760 forecast). If above, stop and split at Phase 9/10 per the forecast. **Measured 1177 added lines (552 prod, 625 tests) > 800: STOP, split required (see apply-progress).** **After the split, PR2a measures 626 authored lines (prod 366, tests 260); typecheck, 125 tests and build green; `ui/dist` has no mock chunk.**
- [ ] 12.3 `gentle-ai review start`; run lenses; finalize; obtain an approved lineage.
- [ ] 12.4 `gentle-ai review bind-sdd --change ui-bridge-parity` BEFORE commit; validate pre-commit.
- [ ] 12.5 USER commits (e.g. `feat: add typed bridge protocol, store and mock bridge`) and pushes; verify git state yourself.
- [ ] 12.6 Validate pre-push and pre-pr with `--base-ref origin/feat/ui-bridge-parity`. PR2a body: Chain Context, diagram (PR2a marked).

## Phase 12b: PR2b restore, verification, review gate, commit (PR2b, branch `feat/ui-bridge-parity-pr2b` off PR2a)

- [ ] 12b.1 After PR2a is committed, create branch `feat/ui-bridge-parity-pr2b` from it; restore the held PR2b files over the workspace (see the holding `README.txt`); confirm 9.1 and 9.3 RED by temporarily reverting `native.ts`/`index.ts`, or record that RED was proven before the split.
- [ ] 12b.2 Run `pnpm --dir ui test`, `typecheck`, `build`; confirm GREEN and no mock chunk in `ui/dist`; then tick 9.1, 9.3, 9.6, 9.8, 9.10 and Phases 10-11.
- [ ] 12b.3 Size check: PR2b authored lines within 800 (~551 forecast); report the measured number.
- [ ] 12b.4 `gentle-ai review start`; run lenses; finalize; obtain an approved lineage.
- [ ] 12b.5 `gentle-ai review bind-sdd --change ui-bridge-parity` BEFORE commit; validate pre-commit.
- [ ] 12b.6 USER commits (e.g. `feat: add native bridge, store and app rewire`) and pushes; verify git state yourself.
- [ ] 12b.7 Validate pre-push and pre-pr with `--base-ref origin/feat/ui-bridge-parity-pr2`. PR2b body: Chain Context, diagram (PR2b marked).

## Phase 13: Primitives and seed helper (PR3; branch `feat/ui-bridge-parity-pr3` off PR2b)

- [ ] 13.1 Create branch `feat/ui-bridge-parity-pr3` from PR2b (`feat/ui-bridge-parity-pr2b`); re-run Vitest baseline.
- [ ] 13.2 RED `components/controls/controls.test.tsx`: `Slider` (value, `onChange`, disabled, optional `skew` prop passes through), `Select`, `Toggle`, `Button` render and fire handlers.
- [ ] 13.3 RED `lib/seed.test.ts`: `/^-?\d+$/` accepts `0`, `-5`, a 19-digit seed as a string (no Number rounding); rejects `""`, `1.5`, `abc`, `--1` (spec "Seed Validation").
- [ ] 13.4 Run `pnpm --dir ui test`; confirm fail.
- [ ] 13.5 GREEN `components/controls/{Slider,Select,Toggle,Button}.tsx` and `lib/seed.ts` (D11, D12).

## Phase 14: StatusLine and Transport (PR3, strict TDD)

- [ ] 14.1 RED `StatusLine.test.tsx`: renders store status text; error rows use the error style; seed message `Seed must be a whole number.`.
- [ ] 14.2 RED `Transport.test.tsx`: Play/Stop sends the matching command and shows `playing` from the snapshot; BPM +/- and input send `setBpm`; master level slider sends `setMasterLevel` and reverts on failure.
- [ ] 14.3 Run `pnpm --dir ui test`; confirm fail.
- [ ] 14.4 GREEN `StatusLine.tsx` and `Transport.tsx`.

## Phase 15: Toggles and Generation (PR3, strict TDD)

- [ ] 15.1 RED `EngineToggles.test.tsx`: Synth and FX toggles send `setSynthEnabled`/`setEffectsEnabled` (spec "Toggle FX").
- [ ] 15.2 RED `GenerationPanel.test.tsx`: Generate/Randomize/Mutate send `regenerate`/`mutate` with the right args and show `Generated.`/`Randomized.`/`Mutated.`; seed field sends `setSeed` with the raw string and, when invalid, no dispatch plus the seed message; large seed preserved; Lock disables Randomize; rhythm fields reflect the snapshot.
- [ ] 15.3 Run `pnpm --dir ui test`; confirm fail.
- [ ] 15.4 GREEN `EngineToggles.tsx` and `GenerationPanel.tsx`.

## Phase 16: Rhythm, Pitch, Evolution (PR3, strict TDD)

- [ ] 16.1 RED `RhythmPanel.test.tsx`: mode, pulses, rotation, chance send `setGenerationParams` live (spec "Live send"); fields reflect the snapshot.
- [ ] 16.2 RED `PitchPanel.test.tsx`: scale choices equal the `ScaleType` list; root sends `setGenerationParams`; range Lo/Hi sends `rangeLow` and `rangeHigh` together under key `gen.range` (spec "Range sent together").
- [ ] 16.3 RED `EvolutionPanel.test.tsx`: Auto-Evolve toggle and rate send `setAutoEvolveEnabled`/`setAutoEvolveRate`; a `snapshot` event updates `mutationCount` display.
- [ ] 16.4 Run `pnpm --dir ui test`; confirm fail.
- [ ] 16.5 GREEN `RhythmPanel.tsx`, `PitchPanel.tsx`, `EvolutionPanel.tsx`.

## Phase 17: Export flow and composition (PR3, strict TDD)

- [ ] 17.1 RED `ExportButton.test.tsx` with a fake `Bridge`: chosen path -> `exportMidi{path}` then `Exported to <file name>`; cancelled -> no dispatch; `error:"busy"` -> status `Busy, try again`, no dispatch; `writeFailed` -> `Export failed: could not write the file.`.
- [ ] 17.2 RED `App.test.tsx` addition: all PR3 panels are present in `App`; disabled states (FX off) per spec "Disabled States".
- [ ] 17.3 Run `pnpm --dir ui test`; confirm fail.
- [ ] 17.4 GREEN `ExportButton.tsx`; compose PR3 panels in `App.tsx`.
- [ ] 17.5 Run `pnpm --dir ui test`, `typecheck`, `build`; confirm GREEN.

## Phase 18: PR3 verification, review gate, commit (PR3)

- [ ] 18.1 Flag-off builds and `BerlinTests` unchanged; flag-on standalone smoke of the PR3 panels.
- [ ] 18.2 Size check: PR3 within 800 (~760 forecast); if above, split at Phase 15/16.
- [ ] 18.3 `gentle-ai review start`; lenses; finalize; approved lineage.
- [ ] 18.4 `gentle-ai review bind-sdd --change ui-bridge-parity` BEFORE commit; validate pre-commit.
- [ ] 18.5 USER commits (e.g. `feat: add transport, generation, pitch, evolution and export controls`) and pushes; verify git state yourself.
- [ ] 18.6 Validate pre-push and pre-pr with `--base-ref origin/feat/ui-bridge-parity-pr2b`. PR3 body: Chain Context, diagram (PR3 marked).

## Phase 19: Skew and delay recommendation helpers (PR4; branch `feat/ui-bridge-parity-pr4` off PR3)

- [ ] 19.1 Create branch `feat/ui-bridge-parity-pr4` from PR3; re-run Vitest baseline.
- [ ] 19.2 RED `lib/skew.test.ts`: JUCE midpoint skew; midpoints cutoff 1000, resonance 2, attack 0.2, decay 0.3, release 0.5, LFO rate 2; round trip and endpoints exact (spec "Skewed Sliders").
- [ ] 19.3 RED `lib/delayRecs.test.ts`: at 120 bpm `60000/bpm × [2,1,.75,.5,1/3,.25]` rounded to whole ms with labels `1/2, 1/4, 1/8., 1/8, 1/8T, 1/16`; recomputes at other bpm.
- [ ] 19.4 Run `pnpm --dir ui test`; confirm fail.
- [ ] 19.5 GREEN `lib/skew.ts` and `lib/delayRecs.ts` (D12).

## Phase 20: Synth, Delay, Reverb (PR4, strict TDD)

- [ ] 20.1 RED `SynthPanel.test.tsx`: waveform choices equal the `Waveform` list; cutoff slider uses skew and sends `setPatch{cutoff}`; pulse width, resonance, ADSR, LFO destination/rate/depth send `setPatch` per field.
- [ ] 20.2 RED `DelayPanel.test.tsx`: Sync on stores `draft.lastManualDelay` and Sync off restores it; Time disabled while Sync is on; recommendations shown and update with BPM; division, feedback, mix send `setPatch`.
- [ ] 20.3 RED `ReverbPanel.test.tsx`: Room, Damping, Wet, Dry send `setPatch` (spec "Wet change"); delay and reverb disabled while FX is off.
- [ ] 20.4 Run `pnpm --dir ui test`; confirm fail.
- [ ] 20.5 GREEN `SynthPanel.tsx`, `DelayPanel.tsx`, `ReverbPanel.tsx`.

## Phase 21: Presets UI and composition (PR4, strict TDD)

- [ ] 21.1 RED `PresetPanel.test.tsx` with a fake `Bridge`: Save disabled with empty name, Load disabled with no selection; new name -> `savePreset{overwrite:false}` -> `Saved "<name>".`; `exists` -> `confirm` -> true -> `savePreset{overwrite:true}`; declined -> no second dispatch; Load fills the name field and shows `Loaded "<name>".`; Load `busy` -> `Busy, try again`, name unchanged; `presetNames` refresh from the snapshot.
- [ ] 21.2 RED `App.test.tsx` addition: all PR4 panels present.
- [ ] 21.3 Run `pnpm --dir ui test`; confirm fail.
- [ ] 21.4 GREEN `PresetPanel.tsx`; compose PR4 panels in `App.tsx`.
- [ ] 21.5 Run `pnpm --dir ui test`, `typecheck`, `build`; confirm GREEN and no mock chunk.

## Phase 22: PR4 verification, review gate, commit (PR4)

- [ ] 22.1 Flag-off builds and `BerlinTests` unchanged; flag-on standalone smoke of the PR4 panels.
- [ ] 22.2 Size check: PR4 within 800 (~740 forecast); if above, split at Phase 20/21.
- [ ] 22.3 `gentle-ai review start`; lenses; finalize; approved lineage.
- [ ] 22.4 `gentle-ai review bind-sdd --change ui-bridge-parity` BEFORE commit; validate pre-commit.
- [ ] 22.5 USER commits (e.g. `feat: add synth, delay, reverb and preset controls`) and pushes; verify git state yourself.
- [ ] 22.6 Validate pre-push and pre-pr with `--base-ref origin/feat/ui-bridge-parity-pr3`. PR4 body: Chain Context, diagram (PR4 marked).

## Phase 23: Manual parity checklist (HUMAN-ONLY, flag-on build)

Who: the user. Run flag-on in standalone `Berlin.exe` AND Cakewalk Sonar (`BerlinPlugin.vst3`). Compare each item against the legacy editor (explore.md section 1). Any failure blocks the Slice.

- [ ] 23.1 **MANUAL** Export MIDI: native save dialog opens at `Documents/Berlin/berlin-export.mid`; file is written; cancel does nothing.
- [ ] 23.2 **MANUAL** Synth and FX toggles.
- [ ] 23.3 **MANUAL** Tempo (BPM -/+ and input); the synced delay recomputes.
- [ ] 23.4 **MANUAL** Delay Sync (restores the last manual time), Division, Time (disabled in Sync), Feedback, Mix.
- [ ] 23.5 **MANUAL** Delay recommendations label matches the legacy label at the same BPM.
- [ ] 23.6 **MANUAL** Reverb Room, Damping, Wet, Dry; delay and reverb disabled while FX is off.
- [ ] 23.7 **MANUAL** Seed (valid, invalid red message, large value).
- [ ] 23.8 **MANUAL** Generate, Randomize (disabled with Lock), Mutate.
- [ ] 23.9 **MANUAL** Lock Seed, Rhythm mode, Pulses, Rotation, Chance %.
- [ ] 23.10 **MANUAL** Scale, Root, Range Lo/Hi (range clamps together).
- [ ] 23.11 **MANUAL** Preset save (new name), overwrite prompt confirm and decline, load fills the name, load while busy shows `Busy, try again`.
- [ ] 23.12 **MANUAL** Auto-Evolve and Rate; the UI follows evolution via `snapshot` events.
- [ ] 23.13 **MANUAL** Waveform, Pulse Width, Cutoff, Resonance, ADSR, LFO Dest/Rate/Depth (skewed sliders feel like legacy).
- [ ] 23.14 **MANUAL** Status line messages for each error path reachable by hand.
- [ ] 23.15 **MANUAL** Play/Stop and the playhead row follow playback.
- [ ] 23.16 **MANUAL** Master level slider.
- [ ] 23.17 **MANUAL** Dialog close mid-flight: close the editor while the export chooser, then the overwrite confirm, is open; no crash, no stale callback. Confirm whether the Win32 teardown invokes the callback synchronously (design open question).
- [ ] 23.18 **MANUAL** `snapshot` event on auto-evolve and on state restore (save the host project, reopen, UI shows restored values; auto-evolve updates the pattern live).
- [ ] 23.19 **MANUAL** Slider drags do not re-parse presets (no stutter with a large preset folder).
- [ ] 23.20 **MANUAL** Flag-off: rebuild with the flag off; the legacy editor is unchanged in standalone and Sonar, and the build works with pnpm absent from PATH.
- [ ] 23.21 Record the parity result in the last PR description and in Engram.

## Delivery Notes

- Flag-on test builds need `BERLIN_WEB_UI=1` in the `.jucer` project defines (`JUCERPROJECT@defines`; plugin: a new `&#10;` line after `JUCE_VST3_CAN_REPLACE_VST2=0`), then a Projucer resave (`H:\Proyectos\Juce\Projucer\Projucer.exe --resave <jucer>`, each from its own directory). Revert both edits and resave before commit; the flag-off diff of `.jucer`/vcxproj must be empty.
- The Projucer per-configuration prebuild runs in all 3 plugin targets.
- Review budget is 800 authored lines per PR, excluding lockfiles and generated files (`ui/pnpm-lock.yaml`, `ui/src/bridge/juce/index.js`, `Source/ui/generated/*`, Projucer output). PR2-PR4 forecasts sit at 93-95% of budget; the split points are listed in the forecast. Do not exceed; split.
- PR2 split into PR2a (`feat/ui-bridge-parity-pr2`, ~626) and PR2b (`feat/ui-bridge-parity-pr2b`, targets PR2a, ~551) after apply measured 1177 lines; user decision 2026-10-03.
- Decision applied: the busy message is the legacy string `Busy, try again`.
- User-reviewable new status strings with no legacy source: `Export failed: the path is not absolute.`, `A preset with that name already exists.` and `Error: <message>`.
- Chained-PR skill note: `gentle-ai-chained-pr` was not found in the skill registry; `C:\Users\ran_k\.claude\skills\chained-pr\SKILL.md` was used instead. Each PR body repeats the dependency diagram.
- Threat matrix: N/A (no routing, shell, subprocess, VCS automation or executable classification boundary), so no threat-matrix RED tasks.
- Resolved questions (done in design): live generation params, Play/Stop and master level included in PR3, TS limits drift guard kept, export default path in C++.

### Follow-ups (out of scope)

- Bridge error logging.
- `BerlinAudioProcessor::setStateInformation` (line ~169) also ignores the result of `regenerate`; same busy-ignored pattern as D3.
- From explore: pre-build lock and timeout items; `setGenerationParams` range normalization; enum-decode duplication.
- Slice 4: MIDI device and channel configuration; standalone clipping follow-up.
