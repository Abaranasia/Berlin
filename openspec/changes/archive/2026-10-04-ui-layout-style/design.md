# Design: Layout and Visual Style (UI Redesign Slice 3/6)

## Technical Approach

Restyle in place (proposal D1). `App` becomes a three-region shell (TopBar, MainArea, SynthDrawer) that composes the existing Slice 2 panels unchanged, apart from swapping `Slider` for `Knob` inside the synth region and moving the delay recommendations into a new `DelayTimesCard`. Plain CSS files under `ui/src/styles/` are imported only from `main.tsx`, so Vitest never loads CSS. Store, bridge and protocol stay as they are. In C++, `WebEditor` becomes host-resizable with limits, `MainComponent` takes its size from its editor, and `MainWindow` copies the editor's limits (or stays fixed for the legacy editor).

## Architecture Decisions

| # | Choice | Rejected | Rationale |
|---|---|---|---|
| D1 | CSS is split into `fonts.css`, `tokens.css`, `base.css`, `layout.css` and `components.css`, imported in that order from `main.tsx`. Components use class names and `data-*` attribute selectors. No CSS modules. | CSS-in-JS, Tailwind, a per-component CSS import | No new dependency. jsdom tests stay CSS-free. One cascade order. |
| D2 | Hand-written `@font-face` rules point at `@fontsource/*/files/*-latin-{w}-normal.woff2` (Sans Condensed 400/600, Mono 400/500), with `font-display: block`. | Importing fontsource's `latin-400.css` | Their CSS also references `.woff`, which Vite would emit and embed. `WebAssets` maps only `woff2`. |
| D3 | The shell region "SynthPanel" is implemented as `SynthDrawer` (`<aside id="synth-panel" aria-label="Internal synth" hidden={!open}>`). It contains `EngineToggles` (Synth, FX), then the existing `SynthPanel`, `DelayPanel` and `ReverbPanel`. | Renaming the existing `SynthPanel` | Keeps `SynthPanel.tsx` and its test untouched. The drawer stays mounted, so `field()` lookups still work. |
| D4 | Base CSS has `[hidden]{display:none!important}`. | Relying on the UA style | Author `display:grid/flex` on `.drawer` would otherwise override `hidden`. |
| D5 | Layout: `.app` is a grid with rows `auto 1fr` and columns `minmax(0,1fr) auto`. At `max-width:1099.98px`, the main area and the drawer both occupy row 2, columns `1/-1`. The drawer gets `justify-self:end; width:min(420px,100%); z-index:1`, so it overlays the main area. The cards use `repeat(auto-fit,minmax(240px,1fr))`, and the top bar uses `flex-wrap`. | Absolute positioning; a second breakpoint | One media query (trim T3). Overlapping in the same grid cell avoids hard-coding the top-bar height. |
| D6 | The `Knob` keeps the Slider's props (`SliderProps`). It renders `<label class="knob">`, containing an SVG face (`aria-hidden`), a visually hidden `<input type=range>` with the same attributes as the Slider, an `<output>` and the label text. Only range controls in the synth region use it. Main-area ranges stay Sliders. | `role=slider` div; knobs everywhere | Labels, `.disabled`, native keyboard behavior and the existing tests carry over (proposal D2). The contract scopes knobs to the synth panel. |
| D7 | Drag: `pointerdown` on the face records `startY` and `startPos`, then calls `setPointerCapture?.()` and focuses the input. `pointermove` computes `dragToPosition(startPos, startY - y)` → `valueAt` → `onChange` when the value changes. `pointerup`/`pointercancel` clear the drag. The drag state lives in a `useRef`. CSS sets `touch-action:none`. No wheel and no double-click handling (T1). | Incremental deltas | An absolute delta from the start does not drift. The optional call keeps jsdom (no capture API) working. |
| D8 | Clipboard: `copyText(text)` = `Promise.resolve().then(() => navigator.clipboard.writeText(text))`. A missing API rejects. `DelayTimesCard` takes an optional `copy` prop that defaults to `copyText`. Success sets status `{text:'Copied 500 ms', error:false}`; any rejection sets `{text:'Copy failed', error:true}`. | `execCommand`; a native fallback (T2); injecting through context | A prop is the smallest seam. A synchronous throw also becomes a rejection. |
| D9 | Panel state is handled by `readSynthPanelOpen(getStorage)` and `writeSynthPanelOpen(open, getStorage)`, with `getStorage = () => window.localStorage` called inside `try`. Only `"1"` reads as open. Write errors are swallowed. `App` holds `useState(() => readSynthPanelOpen())`. The TopBar toggle is `<button aria-label="Synth panel" aria-expanded aria-controls="synth-panel">Synth</button>`. | A store slice | This is UI-only state. Accessing `localStorage` itself can throw a SecurityError. The aria-label avoids clashing with the `Synth` checkbox. |
| D10 | `StepView` comes out of `App.tsx`. A memoized `StepCell` keeps `data-step/active/note/gate` and renders the step number, an LED, and `noteName(note)` when gated or `·` for a rest. Styling uses attribute selectors. `noteName` uses MIDI 60 = C4 and reuses `ROOT_NAMES`, which moves from `PitchPanel`. | New classes, velocity bars | The existing App tests keep passing. The snapshot has no velocity. |
| D11 | A `Card` wrapper (`<section class="card" aria-labelledby>` + `<h2>`) holds each main-area panel. The panels' bodies are not edited. `StatusLine` is a sticky footer of the main area. | Editing every panel | Fewer changed lines. Status stays visible while scrolling. |
| D12 | `WebEditor` calls `setResizable(true,false)`, `setResizeLimits(720,520,1920,1200)` and `setSize(960,680)`, in that order, in the constructor (message thread). | A bottom-right corner resizer | The WebView2 HWND paints above lightweight JUCE children, so the corner would be hidden. The host or the standalone window border does the resizing. |
| D13 | `MainComponent::editor` becomes `std::unique_ptr<juce::AudioProcessorEditor>` (the factory already returns that type). The constructor calls `setSize(editor->getWidth(), editor->getHeight())`. A new `getEditorConstrainer()` returns `editor->isResizable() ? editor->getConstrainer() : nullptr`. When it is non-null, `MainWindow` calls `setResizable(true,false)` and `setResizeLimits(...)` from it; otherwise it calls `setResizable(false,false)`. All of this runs in the message-thread constructors, and the audio path is untouched. | Gating behind `BERLIN_WEB_UI` | Contract decision 2: flag-off becomes 800x952, fixed. The legacy plugin editor is untouched. |
| D13a | The OFL text goes in `ui/public/licenses/OFL-IBM-Plex.txt` (copied to `dist`, so it is embedded). `embed-assets.mjs` `stampInputs` also walks `public/`. | `licenses/` outside the build | The license ships with the binary. Without the stamp change, edits to `public/` would not re-embed. |

## Component Tree and Data Flow

    App(StoreProvider) ─ Shell div.app [open state ⇄ localStorage]
     ├ header.topbar: wordmark · Transport(Play,-,BPM,+,Master level) · PresetPanel · ExportButton · Settings(disabled) · Synth toggle
     ├ main.main-area: StepView ← playhead/snapshot · Card×4(Generation,Rhythm,Pitch,Evolution) · DelayTimesCard · StatusLine
     └ aside#synth-panel[hidden]: EngineToggles · SynthPanel(Knob) · DelayPanel(Knob) · ReverbPanel(Knob)
    Knob drag ─▶ knob.ts ─▶ onChange ─▶ sendPatch (unchanged store coalescing)
    Chip click ─▶ copy("500") ─▶ store.setStatus

## File Changes

| File | Action |
|---|---|
| `ui/src/styles/{fonts,tokens,base,layout,components}.css` | Create |
| `ui/src/lib/{knob,noteName,panelState,clipboard}.ts` + `.test.ts` (no clipboard test) | Create |
| `ui/src/components/controls/Knob.tsx`, `Knob.test.tsx` | Create |
| `ui/src/components/{TopBar,SynthDrawer,Card,StepView,DelayTimesCard}.tsx`, `DelayTimesCard.test.tsx` | Create |
| `ui/src/App.tsx` | Modify: shell, D9 state |
| `ui/src/components/{SynthPanel,DelayPanel,ReverbPanel}.tsx` | Modify: `Slider`→`Knob`. DelayPanel drops `data-role="delay-recommendations"` |
| `ui/src/components/controls/Slider.tsx` | Modify: use `rangeAttrs` from `knob.ts` |
| `ui/src/components/PitchPanel.tsx` | Modify: import `ROOT_NAMES` |
| `ui/src/lib/delayRecs.ts`, `.test.ts` | Modify: remove the now-unused `formatDelayRecommendations` |
| `ui/src/App.test.tsx`, `components/DelayPanel.test.tsx` | Modify |
| `ui/src/main.tsx`, `ui/index.html`, `ui/package.json`, `ui/scripts/embed-assets.mjs`, `ui/public/licenses/OFL-IBM-Plex.txt` | Modify/Create |
| `Source/ui/WebEditor.cpp`, `Source/MainComponent.{h,cpp}`, `Source/Main.cpp` | Modify: D12, D13 |

## Interfaces / Contracts

```ts
// lib/knob.ts
export const KNOB_TRAVEL_PX = 200;
export function rangeAttrs(value: number, min: number, max: number, step?: number, skew?: Skew): { min: number; max: number; step: number | 'any' | undefined; value: number };
export function positionOf(value: number, min: number, max: number, skew?: Skew): number;      // 0..1, clamped
export function valueAt(position: number, min: number, max: number, step?: number, skew?: Skew): number; // clamped, step-snapped (linear)
export function dragToPosition(startPos: number, upPx: number): number;                         // clamp(startPos + upPx / 200)
export function knobGeometry(position: number): { track: string; arc: string; pointer: [number, number, number, number] }; // 135°, 270° sweep, r 22
// lib/panelState.ts
export const SYNTH_PANEL_KEY = 'berlin.ui.synthPanelOpen';
export function readSynthPanelOpen(getStorage?: () => Storage): boolean;
export function writeSynthPanelOpen(open: boolean, getStorage?: () => Storage): void;
// lib/clipboard.ts
export type CopyText = (text: string) => Promise<void>;
```
```cpp
const juce::ComponentBoundsConstrainer* getEditorConstrainer() const noexcept;   // MainComponent; nullptr = fixed size
```
The chip markup is `<button><span>1/4</span> <span>500 ms</span></button>`, so `textContent` is exactly `"1/4 500 ms"`.

## Testing Strategy

| Layer | Cases (RED first) |
|---|---|
| Vitest unit | `knob`: linear and skewed `positionOf`/`valueAt` round trip, step snapping, clamping, a 200 px drag covering the full range, a negative drag, geometry at 0 and 1. `panelState`: "1"→open, "0"/null→closed, `getItem` throws→closed, `getStorage` throws→closed, write stores "1"/"0", a throwing `setItem` is swallowed. `noteName`: 60→C4, 57→A3, 0→C-1, 127→G9. |
| Vitest DOM | `Knob`: aria-label, min/max/step, skewed 0..1/any, `.disabled`, an input change → `onChange` (skew `toValue`), pointer drag (`MouseEvent('pointerdown'/'pointermove')`) → expected value, no drag when disabled. `DelayTimesCard`: 6 chips at 120 BPM, chips follow BPM, click → `copy("500")` + status "Copied 500 ms", a rejection or throw → alert "Copy failed". `App.test`: replace the `data-role` assertion with `button(host,'1/4 500 ms')`; panel closed by default (`hidden`, `aria-expanded="false"`); toggle opens it and writes "1"; remounting with "1" opens it; hidden controls are still found; Settings is disabled; step cells show the note name or `·`. Clear `localStorage` in `afterEach`. `DelayPanel.test`: drop the recommendation assertions. |
| BerlinTests | No change. The sizing code is GUI wiring with no headless seam (same convention as `WebEditor.h`). |
| Manual | Mockup fidelity. Resize to min/max in the standalone app and in Sonar. Overlay below 1100 px. Usable at 720x520 with the drawer open and closed. Knob drag and keyboard in WebView2. Clipboard in the standalone app and in Sonar. No font network requests. Panel state survives a reopen. Flag-off standalone is 800x952, fixed, not clipped. |

## Threat Matrix

N/A: no routing, shell, subprocess, VCS/PR automation, executable-file classification or process-integration boundary. Clipboard and localStorage are in-page browser APIs.

## Work Units and Size Forecast

| Commit | Content | Lines |
|---|---|---|
| 1 | C++ sizing (D12, D13), revertable on its own | ~35 |
| 2 | Fonts, tokens, base and layout CSS, index.html, license, stamp | ~200 |
| 3 | `noteName` and `panelState` with their tests; shell (TopBar, SynthDrawer, Card, StepView); App and App.test | ~330 |
| 4 | `knob.ts` and `Knob` with their tests, panel swaps, Slider refactor, knob CSS | ~250 |
| 5 | Clipboard, `DelayTimesCard` with its test, `delayRecs` and DelayPanel cleanup, chip CSS | ~170 |

The total is about 985 authored lines, excluding the lockfile, the OFL text and the delta specs. That is over the 800-line budget. A clean split is PR A = commits 1–3 (~565) and PR B = commits 4–5 (~420).

## Migration / Rollout

No migration is required. `BERLIN_WEB_UI` stays OFF. If commit 1 is reverted alone, flag-off goes back to 800x680.

## Open Questions

- [ ] Note-name octave convention (C4 = 60 chosen; Cakewalk/Sonar shows C5).
- [ ] Knob value display uses the Slider's numeric format; unit formatting such as "2.4 kHz" is deferred.
