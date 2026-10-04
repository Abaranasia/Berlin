# Exploration: ui-layout-style (UI Redesign Slice 3)

Engram mirror: `sdd/ui-layout-style/explore` (obs #381). Mockup artifact (https://claude.ai/artifact/2btudrMwb5C2PBJgfwSp2P) was not readable by the explorer; this relies on docs/proposal/ui-redesign-plan.md and recorded requirements (dark theme, single amber accent #F0A238, IBM Plex Sans Condensed + IBM Plex Mono; artboards Main, SynthOpen, Settings, Knob).

## 1. Current State

**Web UI (ui/, Vite + React 18 + TS):**
- No CSS files. About 10 lines of inline styles in `ui/index.html` (dark body, `.bpm`, `.steps`, `.step`). Panels are unstyled `<section>`s.
- `App.tsx` renders a flat `<main>`: Transport, EngineToggles, GenerationPanel, RhythmPanel, PitchPanel, EvolutionPanel, SynthPanel, DelayPanel, ReverbPanel, PresetPanel, ExportButton, Steps, StatusLine.
- Controls (`ui/src/components/controls/`): `Slider` (native `<input type=range>` with optional skew via `ui/src/lib/skew.ts`), `Select`, `Toggle`, `Button`.
- Store (`ui/src/store/store.ts`): dependency-free external store, optimistic overlay, per-key coalescing; `useShown`, `sendPatch`, `sendGen`. Bridge: `native.ts` / `mock.ts` behind `bridge/index.ts` (mock is dev-only).
- Tests: Vitest, per-file jsdom, hand-rolled harness (`ui/src/testing/harness.tsx`). No testing-library, no Playwright. `App.test.tsx` finds controls by `[aria-label]` and asserts `.disabled`.

**Reuse as-is:** store, bridge, protocol; all panel logic (delay sync/restore, preset save/overwrite, seed validation, BPM entry, export); skew mappings; `delayRecommendations(bpm)` in `ui/src/lib/delayRecs.ts`; test harness.

**Restructure:**
- `App.tsx` becomes a shell: TopBar, main area (steps, generation/rhythm/pitch/evolution, delay-times card), collapsible SynthDrawer.
- Delay and Reverb move into the synth panel; a separate DelayTimesCard stays in the main area and replaces `<small data-role="delay-recommendations">` (tests assert that attribute and "1/4 500 ms").
- Transport, PresetPanel, ExportButton, master level move to the top bar. Synth/FX toggles move to top bar / synth panel.
- New CSS in `ui/src/styles/*.css` imported from `main.tsx`; remove the inline `<style>` from `index.html`.

## 2. Live step view and playhead (no new bridge work)
- `WebEditor::timerCallback` (30 Hz, message thread) reads relaxed atomics `getPlayheadStep()` / `isPlaying()` and emits `playhead {step, playing}` on change via `PlayheadChangeDetector`; `snapshot` events via `PendingEventGate`.
- `snapshot.steps[{note, active}]` and `playheadStep` exist; `Steps()` already renders 16 steps with playhead highlight and `data-note` / `data-gate`.
- Slice 3 only restyles (note names via pure TS, gated vs rest look). Audio thread untouched. 30 Hz is sufficient; memoize per step; CSS transitions for smoothness.

## 3. Fonts
- `@fontsource/ibm-plex-sans-condensed` and `@fontsource/ibm-plex-mono` as devDependencies; import latin subsets only (e.g. 400/600).
- Vite emits woff2 into `dist/assets` (`base: './'`); `ui/scripts/embed-assets.mjs` embeds all of `dist`; `WebAssets.cpp` already maps `woff2` → `font/woff2`. No C++ change, no runtime CDN.
- License: SIL OFL 1.1 (embedding/redistribution allowed); ship the license text.
- ~6 files × 15–25 KB → ~0.5 MB generated C++ (hex). Confirm generated sources stay gitignored and the stamp pipeline picks them up.
- CSS fallback: `'IBM Plex Sans Condensed', system-ui, sans-serif`; consider `font-display: block` / preload to avoid FOUT.

## 4. Knob

| Approach | Pros | Cons | Effort |
|---|---|---|---|
| A. Custom Knob wrapping a visually-hidden native range input + SVG arc; pointer drag, wheel, double-click reset | Keeps `aria-label`, `.disabled`, native keyboard; existing tests keep working; reuses skew | Unusual DOM; needs `touch-action: none` and pointer capture | Low–Med |
| B. `div role=slider` with custom ARIA/keys | Full control | Reimplements keyboard; `.disabled` → `aria-disabled` breaks tests | Med |
| C. Third-party knob library | Fast | New dep, uncertain a11y/style fit | Low, risky |

Recommend A. Pure helper `ui/src/lib/knob.ts` (drag delta → 0..1, wheel steps, reset) unit-tested without DOM; jsdom tests cover keyboard/aria; drag verified manually. Double-click reset needs per-control defaults (reuse the defaults SynthPanel already passes to `useShown`). Skewed controls (cutoff, resonance, ADSR times, LFO rate) go through existing `SKEWS`.

## 5. Window sizing and clipping fix
- `WebEditor`: fixed 800×680 (`WebEditor.cpp:17-18,36`).
- Legacy editor: `setSize(800, 680 + 8*(kControlHeight + kMargin/2))` = 800×952 (`BerlinAudioProcessorEditor.cpp:474`).
- `MainComponent`: hard-coded `setSize(800, 680)` (`MainComponent.cpp:9`) → clipping exists only on the legacy (flag OFF) path.
- `MainWindow`: `setResizable(true, true)` with no limits (`Main.cpp:73`).
- Specs freeze sizes: web-editor ("fixed-size and fit within 800×680", legacy "unchanged"), plugin-host-integration ("window size unchanged" with flag off) → delta specs required.
- Proposed C++ (~30–40 lines): MainComponent sizes to its editor; MainWindow `setResizeLimits`; WebEditor `setResizable(true, true)` + `setResizeLimits`, initial ~960×680, min ~720×520. Legacy editor stays non-resizable (removed in Slice 5). Hosts may ignore resizing → the min-size layout must be usable.
- Decision: apply the MainComponent sizing fix to both paths (recommended) or gate behind `BERLIN_WEB_UI`.
- Responsive layout in plain CSS (grid/flex, breakpoints ~900/720 px); verified manually.
- Synth-panel open/closed persisted in `localStorage` (WebView2 user-data folder is persistent); UI-only, no bridge change. Per-origin: dev server and embedded origin don't share it (acceptable).

## 6. Clipboard (click a delay time to copy)
- `navigator.clipboard.writeText(String(ms))` on click; embedded origin `https://juce.backend/` is a secure context. Verify manually in standalone and one DAW.
- Fallbacks: `execCommand('copy')` via temporary textarea; last resort a native `copyToClipboard` using `juce::SystemClipboard` (avoid unless needed).
- try/catch, status "Copied 500 ms" via `store.setStatus`; inject the clipboard function for tests. Copy the ms number only.

## 7. Scope boundaries
- **Slice 3:** tokens/theme, bundled fonts, responsive shell, top bar, step view restyle, collapsible synth panel (Delay + Reverb inside), delay-times card with copy, Knob, resize + limits, MainComponent clipping fix, delta specs (web-editor, plugin-host-integration), new layout spec.
- **Slice 4:** settings dialog, device/channel config, `isStandalone` bridge flag. Slice 3 shows a disabled settings placeholder. Master volume stays in Slice 3's layout; Slice 4 may move it.
- **Slice 5:** remove legacy editor and the `BERLIN_WEB_UI` flag. The flag stays OFF in Slice 3.

## 8. Approaches and size forecast

| Approach | Pros | Cons | Effort |
|---|---|---|---|
| 1. Restyle in place: plain CSS tokens + Grid, hidden-input Knob | Reuses logic and tests; no runtime deps; responsive via CSS | Needs manual visual pass | Med |
| 2. CSS-in-JS / Tailwind | Scoped styles | New dep/config for little gain | Med–High |
| 3. Rebuild components with new state shape | Clean slate | Discards Slice 2 parity logic/tests | High |

Forecast ~900–1100 changed lines (over the 800 budget): CSS ~250, shell components ~220, Knob + helper + tests ~230, tests ~200, C++ ~40, specs/docs ~80. Lockfile and generated font sources excluded.

Possible split: PR A shell/layout (tokens, fonts, top bar, grid, synth drawer with existing Sliders, step restyle, C++ sizing, delta specs); PR B components (Knob, delay-times card + copy, a11y polish, remaining tests).

## Recommendation
Approach 1. Keep native range inputs under the Knob; collapsed synth panel stays mounted (CSS `hidden`) so aria-labelled controls remain reachable; `localStorage` for panel state. Defer Playwright; use Vitest/jsdom plus a manual responsive checklist.

## Risks
- Spec conflicts on fixed/unchanged sizes; changing the legacy window height must be explicit.
- Tests coupled to DOM/labels; unmounting the collapsed drawer would break `field(host, 'Waveform')`-style lookups.
- Responsiveness untestable in jsdom; hosts may ignore resize.
- Clipboard in WebView2 / DAW hosts unverified.
- Mockup fidelity not confirmed by this exploration.
- Generated sources grow with fonts; check build time and stamp pipeline.
- Knob math with skewed controls; double-click reset needs defaults.
