# Tasks: Layout and Visual Style (UI Redesign Slice 3/6)

Strict TDD is active: every behavior task is RED (write the failing test, run it, confirm it fails for the right reason) then GREEN (minimal code) then run the suite. Package manager is pnpm only (`pnpm --dir ui ...`); npm is broken, never use it. MSBuild: `C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe`. Projucer: `H:\Proyectos\Juce\Projucer\Projucer.exe --resave <jucer>`. C++ tests: `Tests/Builds/VisualStudio2026/x64/Debug/ConsoleApp/BerlinTests.exe --category=Berlin`. BerlinTests are unchanged by this slice (sizing code has no headless seam, same convention as `WebEditor.h`), so C++ work is verified manually. Skills to load when applying: `juce-app-dev` (C++ constructors on the message thread, audio path untouched), `juce-app-testing`, `work-unit-commits` (paths in `.atl/skill-registry.md`).

Verification legend (from the specs): [V] Vitest/jsdom or build scan, [M] manual.

## Review Workload Forecast

| Field | Value |
|---|---|
| Estimated authored lines | ~985 (excludes lockfile, OFL text, delta specs, generated `dist`/embedded assets) |
| Review budget | 800 authored lines per PR |
| Delivery strategy | single-pr-default |
| Chain strategy | not yet chosen (orchestrator decides) |
| 400-line budget risk | High (total ~985; even PR A ~565 and PR B ~420 each exceed 400) |
| 800-line budget risk | High as a single PR (~985, about 23% over); Low per PR if split (PR A ~565, PR B ~420) |
| Chained PRs recommended | Yes |
| Decision needed before apply | Yes (choose single PR with `size:exception`, or the A/B split below) |

Split boundary (usable if the orchestrator chooses a split):

| PR | Commits | Content | Est. lines | Rollback |
|---|---|---|---|---|
| PR A | 1-3 | C++ sizing, CSS foundation (fonts, tokens, base, layout), shell (TopBar, SynthDrawer, Card, StepView), `noteName`, `panelState`, App | ~565 | Revert A: UI returns to Slice 2 look; commit 1 is revertable alone (flag-off back to 800x680) |
| ===== SPLIT BOUNDARY ===== | | PR B targets PR A's branch if chained | | |
| PR B | 4-5 | `knob.ts` + `Knob`, panel swaps, Slider refactor, knob CSS; clipboard, `DelayTimesCard`, `delayRecs`/DelayPanel cleanup, chip CSS | ~420 | Revert B: shell stays, controls return to Sliders and text delay recs |

Note: PR A leaves `DelayPanel` with its text recommendations and Sliders in the drawer (still functional); no intermediate state is broken.

## Phase 0: Baseline

- [x] 0.1 `git status` and `git branch --show-current`: confirm branch `feat/add-ui-layout`; verify git state yourself.
- [x] 0.2 `pnpm --dir ui test`; record the Vitest count (baseline for later comparison). (Vitest baseline: 381 tests, 29 files)
- [x] 0.3 Build `Builds/VisualStudio2026/Berlin.sln` (Debug x64, flag off) and `Tests/Builds/VisualStudio2026/BerlinTests.sln`; run `BerlinTests.exe --category=Berlin`; record the pass count. (BerlinTests suite unchanged by this slice; 379 tests completed, exit 0, measured after the change)

## Phase 1: C++ sizing (commit 1, ~35 lines, no TDD seam, manual verification in Phase 9)

Files: `Source/ui/WebEditor.cpp`, `Source/MainComponent.h`, `Source/MainComponent.cpp`, `Source/Main.cpp`. Specs: web-editor "WebView2 Hosting And User Data Folder" (Initial size and limits, Resize in host), plugin-host-integration "Standalone Shell" (all scenarios).

- [x] 1.1 `WebEditor.cpp` constructor (D12): call `setResizable(true,false)`, `setResizeLimits(720,520,1920,1200)`, `setSize(960,680)` in that order on the message thread. Satisfies: Initial size and limits [M]; Resize in host [M].
- [x] 1.2 `MainComponent.h/.cpp` (D13): hold the editor as `std::unique_ptr<juce::AudioProcessorEditor>`; constructor calls `setSize(editor->getWidth(), editor->getHeight())`; add `getEditorConstrainer() const noexcept` returning `editor->isResizable() ? editor->getConstrainer() : nullptr`. Satisfies: Flag off, full legacy editor [M] (800x952 from the legacy editor size); Content follows window [M].
- [x] 1.3 `Main.cpp` `MainWindow`: if `getEditorConstrainer()` is non-null call `setResizable(true,false)` and `setResizeLimits(...)` from it; else `setResizable(false,false)`. Satisfies: Flag on standalone shell [M]; Flag off, full legacy editor [M]; Flag off, plugin editor unchanged [M] (legacy plugin editor not touched).
- [x] 1.4 Build flag-off and flag-on standalone (Debug x64); confirm both compile. Behavior check is deferred to Phase 9. Commit 1: `feat: make web editor host-resizable and size standalone window to editor`.

## Phase 2: CSS foundation, fonts, license (commit 2, ~200 lines)

Files: `ui/src/styles/{fonts,tokens,base,layout}.css`, `ui/src/main.tsx`, `ui/index.html`, `ui/package.json`, `ui/scripts/embed-assets.mjs`, `ui/public/licenses/OFL-IBM-Plex.txt`. Specs: web-ui-layout "Theme Tokens", "Bundled Offline Fonts", "Responsive Layout".

- [x] 2.1 RED: `ui/src/styles/styles.test.ts` reads `tokens.css` and `fonts.css` with `node:fs` (no CSS import, jsdom stays CSS-free). Assert every token is defined on `:root` (ground, surfaces, borders, text, muted, `--accent: #F0A238`, accent-hover, on-accent, radii 6/8/12, focus ring), and every `@font-face` `url()` is relative/local with no `http(s)`. Run; confirm it fails (files missing). Satisfies: Tokens defined [V]; No network fonts [V] (source half).
- [x] 2.2 GREEN: add `@fontsource/ibm-plex-sans-condensed` and `@fontsource/ibm-plex-mono` to `ui/package.json` (pnpm install; lockfile excluded from the line count). Create `fonts.css` (hand-written `@font-face`, D2: Sans Condensed 400/600, Mono 400/500, `latin-{w}-normal.woff2`, `font-display: block`), `tokens.css`. Test passes.
- [x] 2.3 Create `base.css` (reset, body font, `[hidden]{display:none!important}` per D4, accent focus ring, 44 px interactive targets) and `layout.css` (D5: `.app` grid `auto 1fr` / `minmax(0,1fr) auto`; one media query at `max-width:1099.98px` where the drawer overlays: `justify-self:end; width:min(420px,100%); z-index:1`; card grid `repeat(auto-fit,minmax(240px,1fr))`; top bar `flex-wrap`). Satisfies: Focus ring visible [M]; Wide viewport [M]; Narrow viewport [M]; Minimum size usable [M] (verified in Phase 9).
- [x] 2.4 `main.tsx`: import `fonts.css`, `tokens.css`, `base.css`, `layout.css` in that order (D1). `index.html`: remove the inline `<style>`. Satisfies: Theme Tokens requirement (inline style removed).
- [x] 2.5 `ui/public/licenses/OFL-IBM-Plex.txt` (OFL text); `ui/scripts/embed-assets.mjs`: make `stampInputs` also walk `public/` (D13a). Satisfies: Bundled Offline Fonts (OFL shipped). Check: `pnpm --dir ui build`, confirm woff2 and the license are in `dist` and the embed stamp changes when `public/` changes.
- [x] 2.6 `pnpm --dir ui test`, `typecheck`, `build`. Commit 2: `feat: add theme tokens, bundled IBM Plex fonts and layout CSS`.

## Phase 3: noteName and panelState (commit 3a, strict TDD)

Files: `ui/src/lib/{noteName,panelState}.ts` + `.test.ts`, `ui/src/components/PitchPanel.tsx`. Specs: web-ui-layout "Step View", "Collapsible Synth Panel With Persistence".

- [x] 3.1 RED: `noteName.test.ts`: 60 -> `C4`, 57 -> `A3`, 0 -> `C-1`, 127 -> `G9`, 48 -> `C3`. Confirm fails. Satisfies: Gate distinction [V] (naming).
- [x] 3.2 GREEN: `noteName.ts` (MIDI 60 = C4); move `ROOT_NAMES` out of `PitchPanel.tsx` into the shared module and import it back in `PitchPanel.tsx`. Existing Pitch tests still pass.
- [x] 3.3 RED: `panelState.test.ts`: `"1"` -> open; `"0"`/`null` -> closed; `getItem` throws -> closed; `getStorage` throws -> closed; write stores `"1"`/`"0"` under key `berlin.ui.synthPanelOpen`; a throwing `setItem` is swallowed. Confirm fails. Satisfies: Default closed [V]; Toggle persists [V]; Storage throws [V].
- [x] 3.4 GREEN: `panelState.ts` per D9 (`SYNTH_PANEL_KEY`, `readSynthPanelOpen(getStorage)`, `writeSynthPanelOpen(open, getStorage)`, `getStorage = () => window.localStorage` called inside `try`).

## Phase 4: Shell regions (commit 3b, strict TDD)

Files: `ui/src/components/{TopBar,SynthDrawer,Card,StepView}.tsx`, `ui/src/App.tsx`, `ui/src/App.test.tsx`. Specs: web-ui-layout "Shell Regions", "Settings Placeholder", "Collapsible Synth Panel", "Step View".

- [x] 4.1 RED: extend `App.test.tsx` (clear `localStorage` in `afterEach`):
  - master level within TopBar, step view within MainArea, FX toggle and delay/reverb controls within the synth drawer (Controls in regions [V]);
  - "Settings" button disabled; click dispatches nothing (Disabled placeholder [V]);
  - panel container `hidden` and `aria-expanded="false"` by default, labeled controls still found in the DOM (Default closed [V]);
  - toggle press writes `"1"`, remount with `"1"` opens it (Toggle persists [V]);
  - `localStorage` access throwing: starts closed, toggle press shows no error and does not throw (Storage throws [V]);
  - step 1 gated MIDI 48 shows LED on and "C3", step 2 rest shows LED off and `·` (Gate distinction [V]);
  - `playhead {step:4}` highlights only step 4 (Playhead [V]).
  Confirm fails for the right reasons.
- [x] 4.2 GREEN: create `Card.tsx` (`<section class="card" aria-labelledby>` + `<h2>`, D11), `StepView.tsx` (memoized `StepCell` keeping `data-step/active/note/gate`, number, LED, `noteName` or `·`, D10; driven only by snapshot and `playhead`), `TopBar.tsx` (wordmark, Transport, PresetPanel, ExportButton, disabled Settings, `button aria-label="Synth panel" aria-expanded aria-controls="synth-panel"` labelled "Synth", D9), `SynthDrawer.tsx` (`<aside id="synth-panel" aria-label="Internal synth" hidden={!open}>` containing `EngineToggles`, `SynthPanel`, `DelayPanel`, `ReverbPanel`, D3). Rewire `App.tsx` to the three-region shell with `useState(() => readSynthPanelOpen())` and `writeSynthPanelOpen` on toggle; `StatusLine` as sticky footer of MainArea. Panel bodies unchanged.
- [x] 4.3 Add region and step CSS to `layout.css` (and any `StepView` attribute-selector styling; component CSS for the Card/TopBar/step cells goes into a new `ui/src/styles/components.css`, imported last in `main.tsx` after `layout.css`).
- [x] 4.4 Parity check: run all Slice 2 control tests unchanged. Satisfies: Parity preserved [V]. Fix any label or dispatch regression in the shell, never by editing the old tests.
- [x] 4.5 `pnpm --dir ui test`, `typecheck`, `build` (no mock chunk). Commit 3: `feat: add three-region shell, collapsible synth drawer and step view`.

SPLIT BOUNDARY: PR A = commits 1-3 (Phases 1-4). PR B = commits 4-5 (Phases 5-6).

## Phase 5: Knob (commit 4, ~250 lines, strict TDD)

Files: `ui/src/lib/knob.ts` + `.test.ts`, `ui/src/components/controls/Knob.tsx` + `Knob.test.tsx`, `ui/src/components/controls/Slider.tsx`, `ui/src/components/{SynthPanel,DelayPanel,ReverbPanel}.tsx`, `ui/src/styles/components.css`. Specs: web-ui-layout "Knob Control"; web-ui-controls "Skewed Sliders".

- [x] 5.1 RED: `knob.test.ts`: linear and skewed `positionOf`/`valueAt` round trip; step snapping; clamping; `dragToPosition(0.5, 100)` = 1.0 (clamped) and 200 px up from min gives max; negative drag; `knobGeometry` at 0 and 1; `rangeAttrs` shape. Confirm fails. Satisfies: Drag math [V]; web-ui-controls Round trip and endpoints.
- [x] 5.2 GREEN: `knob.ts` per the design contract (`KNOB_TRAVEL_PX = 200`, `rangeAttrs`, `positionOf`, `valueAt`, `dragToPosition`, `knobGeometry`). Refactor `Slider.tsx` to use `rangeAttrs`; existing Slider tests stay green.
- [x] 5.3 RED: `Knob.test.tsx`: aria-label, min/max/step, skewed 0..1/`any`; cutoff Knob at midpoint maps to 1000 Hz via `SKEWS` (Skewed knob [V]; web-ui-controls Midpoint for all six skewed controls: cutoff 1000, resonance 2, attack 0.2, decay 0.3, release 0.5, LFO rate 2); input change -> `onChange` (skew `toValue`); ArrowRight dispatches `setPatch` with the next step (Keyboard [V]); `.disabled` input disabled and pointer drag dispatches nothing (Disabled [V]); pointer drag via `MouseEvent('pointerdown'/'pointermove')` -> expected value; wheel and double-click leave the value unchanged (No wheel or reset [V]). Confirm fails.
- [x] 5.4 GREEN: `Knob.tsx` per D6/D7 (`<label class="knob">` with SVG face `aria-hidden`, visually hidden `<input type=range>`, `<output>`, label text; drag state in `useRef`; `setPointerCapture?.()`; focus input on `pointerdown`; `pointerup`/`pointercancel` clear drag; no wheel or double-click handlers).
- [x] 5.5 Swap `Slider` for `Knob` in `SynthPanel`, `DelayPanel`, `ReverbPanel` (synth region only; main-area ranges stay Sliders). Existing panel tests pass unchanged. Satisfies: Parity preserved [V].
- [x] 5.6 Knob CSS in `components.css` (visually hidden input, `touch-action:none`, `.disabled` state, focus ring on the face via `:focus-within`). Satisfies: Pointer feel [M] (verified in Phase 9).
- [x] 5.7 `pnpm --dir ui test`, `typecheck`, `build` (no mock chunk). Commit 4: `feat: add vertical-drag Knob control for synth panel` (commit left to the orchestrator; Vitest 462 passing after PR B).

## Phase 6: Delay-times card (commit 5, ~170 lines, strict TDD)

Files: `ui/src/lib/clipboard.ts` (no unit test, covered via card), `ui/src/components/DelayTimesCard.tsx` + `DelayTimesCard.test.tsx`, `ui/src/lib/delayRecs.ts` + `.test.ts`, `ui/src/components/DelayPanel.tsx`, `ui/src/components/DelayPanel.test.tsx`, `ui/src/App.tsx`, `ui/src/styles/components.css`. Specs: web-ui-layout "Delay-Times Card"; web-ui-controls "Delay Recommendations".

- [x] 6.1 RED: `DelayTimesCard.test.tsx`: six buttons at 120 BPM with `textContent` exactly `"1/2 1000 ms"`, `"1/4 500 ms"`, `"1/8. 375 ms"`, `"1/8 250 ms"`, `"1/8T 167 ms"`, `"1/16 125 ms"` (At 120 BPM; Rendered as buttons [V]); chips follow BPM, `1/4` = `400 ms` at 150 BPM (Updates with BPM); click -> injected `copy("500")` called and status "Copied 500 ms" (Copy success [V]); `copy` rejecting and `copy` throwing synchronously -> alert "Copy failed", no `unhandledrejection` (Copy failure [V]); missing `navigator.clipboard` -> "Copy failed" with the default `copy`. Confirm fails.
- [x] 6.2 GREEN: `clipboard.ts` (`copyText = (t) => Promise.resolve().then(() => navigator.clipboard.writeText(t))`, type `CopyText`, D8); `DelayTimesCard.tsx` (optional `copy` prop defaulting to `copyText`, chip markup `<button><span>1/4</span> <span>500 ms</span></button>`, success/error status via store `setStatus`). Mount it in the MainArea in `App.tsx`.
- [x] 6.3 RED then GREEN: in `App.test.tsx` replace the `data-role="delay-recommendations"` assertion with `button(host,'1/4 500 ms')` and assert no `data-role="delay-recommendations"` remains (Rendered as buttons [V]); in `DelayPanel.test.tsx` drop the recommendation assertions. Remove the recommendations text and `data-role` from `DelayPanel.tsx`; remove the now-unused `formatDelayRecommendations` (and its test cases) from `delayRecs.ts`/`.test.ts`, keeping the pure computation tests (At 120 BPM, Updates with BPM).
- [x] 6.4 Chip CSS in `components.css`.
- [x] 6.5 `pnpm --dir ui test`, `typecheck`, `build` (no mock chunk). Commit 5: `feat: add delay-times card with copy-to-clipboard chips`.

## PR A review follow-ups (applied in PR B)

- [x] R.1 RES-001/REL-001 (C++, build-verified): `Source/Main.cpp` `MainWindow::applyEditorLimits()` adds the window frame (`getContentComponentBorder()` plus the native peer `getFrameSize()` once the window is visible) to the editor limits, so the CONTENT stays within 720x520..1920x1200; flag-off stays fixed 800x952. Message thread only.
- [x] R.2 READ-001/REL-003: `styles.test.ts` `declared()` uses `String.raw`; new test proves whitespace around ':' is tolerated.
- [x] R.3 READ-002: `#2a2418` and `#ff8a6b` moved to `--accent-active-bg` and `--error` in `tokens.css`; tests assert both tokens and that `components.css` has no raw hex.
- [x] R.4 READ-003: `App.tsx` `toggleSynth` computes `const next = !synthOpen` once.
- [x] R.5 READ-004: comments in `Source/ui/WebEditor.cpp` (kMinWidth) and `layout.css` tie the 720 px minimum to the 1100/900 px breakpoints.
- [x] R.6 REL-002: Settings placeholder has `aria-label="Settings"`; `App.test.tsx` finds it through the aria-label lookup.
## Phase 7: Build and test commands (run before each commit and again at the end)

- [x] 7.1 `pnpm --dir ui test` (all Vitest green; compare the count with the Phase 0 baseline plus new tests). (PR A: 422 tests, 32 files)
- [x] 7.2 `pnpm --dir ui typecheck` (tsc, no errors).
- [x] 7.3 `pnpm --dir ui build` with no mock chunk (existing `assertNoMockChunk`), and a scan of the built stylesheets: every `@font-face` URL is a local asset, no `http(s)` font URL, `--accent` is `#F0A238` (Tokens defined [V]; No network fonts [V], built half).
- [x] 7.4 `BerlinTests.exe --category=Berlin` (unchanged suite; pass count equals the Phase 0 baseline). (379 completed, exit 0)
- [x] 7.5 Flag-ON build: standalone (`Builds/VisualStudio2026/Berlin.sln`) and plugin (`Plugin/Builds/VisualStudio2026/Berlin.sln`), Debug x64, `/t:Rebuild`, `BERLIN_WEB_UI` defined; zero errors.
- [x] 7.6 Flag-OFF build: same two solutions with `BERLIN_WEB_UI` undefined; zero errors.

## Phase 8: Open-question handling

- [x] 8.1 Record in the apply-progress that note naming uses MIDI 60 = C4 (Sonar shows C5) and that knob values use the Slider's numeric format (unit formatting deferred); no code task. (recorded in apply-progress: MIDI 60 = C4, knob values use the Slider numeric format)

## Phase 9: Manual verification (all [M] scenarios; record result and date per line)

Run on a flag-ON standalone build and in Cakewalk Sonar (flag-ON plugin) unless noted.

- [ ] 9.1 Initial size and limits (web-editor): editor opens 960x680, resizable, limits 720x520 to 1920x1200 (standalone and Sonar).
- [ ] 9.2 Resize in host (web-editor): resize within and beyond the limits in standalone and Sonar; the view follows within limits; if the host ignores resize, the layout stays usable at its size.
- [ ] 9.3 Flag on standalone shell (plugin-host-integration): `WebEditor` hosted, window clamped to 720x520-1920x1200, audio and OS MIDI output still work.
- [ ] 9.4 Content follows window (plugin-host-integration): resize the flag-on standalone window; the editor fills the content area.
- [ ] 9.5 Focus ring visible (web-ui-layout): tab through every control; an accent focus ring shows on each.
- [ ] 9.6 Offline rendering (web-ui-layout): disconnect the network, load the editor, confirm IBM Plex renders and no font network requests occur.
- [ ] 9.7 Wide viewport (web-ui-layout): at 1200 px wide with the panel open, the panel is beside the main area, not covering it.
- [ ] 9.8 Narrow viewport (web-ui-layout): at 960 px wide with the panel open, the panel overlays the main area.
- [ ] 9.9 Minimum size usable (web-ui-layout): at 720x520 every control is reachable by scroll or focus, panel open then closed.
- [ ] 9.10 Pointer feel (web-ui-layout): drag knobs in the WebView2 editor in standalone and Sonar; tracking is smooth, keyboard arrows also work.
- [ ] 9.11 Clipboard in DAW (web-ui-layout): in Sonar click a delay chip; the ms value pastes elsewhere or "Copy failed" shows. Repeat in standalone.
- [ ] 9.12 Panel state survives a reopen: open the panel, close and reopen the app and the Sonar editor; the panel is still open.
- [ ] 9.13 Mockup fidelity: compare against the mockup (TopBar, MainArea, drawer, step view, cards).
- [ ] 9.14 Flag off, full legacy editor (plugin-host-integration): flag-OFF standalone launches at 800x952, non-resizable, no clipping; audio, OS MIDI and UI behave as before.
- [ ] 9.15 Flag off, plugin editor unchanged (web-editor Flag off keeps legacy editor; plugin-host-integration): flag-OFF plugin editor in a host has its original size, is non-resizable, and no WebView2 is used.
- [ ] 9.16 Record all results and the test counts in the apply-progress; hand off to `sdd-verify`.

## Spec Scenario Coverage Map

| Spec scenario | Task(s) |
|---|---|
| Tokens defined [V] | 2.1, 2.2, 7.3 |
| Focus ring visible [M] | 2.3, 9.5 |
| No network fonts [V] | 2.1, 7.3 |
| Offline rendering [M] | 2.5, 9.6 |
| Controls in regions [V] | 4.1, 4.2 |
| Parity preserved [V] | 4.4, 5.5 |
| Disabled placeholder [V] | 4.1, 4.2 |
| Default closed [V] | 3.3, 3.4, 4.1 |
| Toggle persists [V] | 3.3, 3.4, 4.1 |
| Storage throws [V] | 3.3, 3.4, 4.1 |
| Wide viewport [M] | 2.3, 9.7 |
| Narrow viewport [M] | 2.3, 9.8 |
| Minimum size usable [M] | 2.3, 9.9 |
| Drag math [V] | 5.1, 5.2 |
| Skewed knob [V] | 5.3, 5.4 |
| Keyboard [V] | 5.3, 5.4 |
| Disabled [V] (Knob) | 5.3, 5.4 |
| No wheel or reset [V] | 5.3, 5.4 |
| Pointer feel [M] | 5.6, 9.10 |
| Gate distinction [V] | 3.1, 3.2, 4.1, 4.2 |
| Playhead [V] | 4.1, 4.2 |
| Copy success [V] | 6.1, 6.2 |
| Copy failure [V] | 6.1, 6.2 |
| Clipboard in DAW [M] | 9.11 |
| web-editor: Flag off keeps legacy editor [M] | 1.2, 1.3, 9.15 |
| web-editor: Flag on selects WebEditor | 7.5 (build with flag on; existing behavior) |
| web-editor: Data folder created | unchanged behavior; existing tests (7.4) |
| web-editor: Initial size and limits [M] | 1.1, 9.1 |
| web-editor: Resize in host [M] | 1.1, 9.2 |
| plugin-host-integration: Flag off, full legacy editor [M] | 1.2, 1.3, 9.14 |
| plugin-host-integration: Flag off, plugin editor unchanged [M] | 1.3, 9.15 |
| plugin-host-integration: Flag on standalone shell | 1.3, 9.3 |
| plugin-host-integration: Content follows window [M] | 1.2, 9.4 |
| web-ui-controls: Midpoint | 5.3 |
| web-ui-controls: Round trip and endpoints | 5.1, 5.2 |
| web-ui-controls: At 120 BPM | 6.1 |
| web-ui-controls: Updates with BPM | 6.1, 6.3 |
| web-ui-controls: Rendered as buttons [V] | 6.1, 6.3 |
