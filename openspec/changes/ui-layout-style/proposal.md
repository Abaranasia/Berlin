# Proposal: Layout and Visual Style (UI Redesign Slice 3/6)

## Intent

Slice 2 reached functional parity, but the web UI is an unstyled flat column of ~40 controls. It is hard to scan, and the sequencer is not the visual focus. The standalone window is also fixed at 800x680, which clips the 800x952 legacy editor. This slice applies the mockup (Main, SynthOpen, Knob): a dark themed shell with the sequencer first, a collapsible synth panel, knobs, and a resizable editor that adapts to the window size.

## Scope

### In Scope
- Theme tokens in plain CSS (`ui/src/styles/`): colors, radii 6/8/12, 44 px targets, accent focus ring. Remove the inline `<style>` from `index.html`.
- IBM Plex Sans Condensed and IBM Plex Mono bundled offline via `@fontsource` (latin subset), with the OFL license text shipped.
- `App.tsx` becomes a shell:
  - **Top bar:** wordmark, Play/Stop, BPM, preset select + name + Load/Save, Export MIDI, master level, disabled settings placeholder, Synth toggle.
  - **Main area:** step view (step number, gate LED, note name, playhead), then Generation, Rhythm, Pitch, Evolution and the delay-times card.
  - **Synth panel:** Sound/FX switches, oscillator, filter & envelope, LFO, delay, reverb.
- Collapsible synth panel. It stays mounted when collapsed, and its open state is persisted in `localStorage`.
- `Knob` component wrapping a visually hidden native range input: pointer drag, wheel, double-click reset, keyboard. Pure math helper `ui/src/lib/knob.ts`.
- Delay-times card: click a value to copy its ms number (`navigator.clipboard` with an `execCommand` fallback), with a status message.
- Responsive CSS Grid layout (breakpoints about 900/720 px). Below a width threshold, the open synth panel overlays the main area instead of squeezing it.
- C++ sizing:
  - `WebEditor` becomes resizable with limits (initial about 960x680, minimum about 720x520; design sets the exact values).
  - `MainComponent` sizes itself to its editor on both paths, which fixes the legacy clipping.
  - `MainWindow` gets resize limits.

### Out of Scope
- Settings dialog, devices and `isStandalone` (Slice 4). Removing the legacy editor and the `BERLIN_WEB_UI` flag (Slice 5). The flag stays OFF.
- New bridge commands or engine work. Playwright (deferred). CSS-in-JS or Tailwind.
- A styled Save dialog. The existing name field and save flow are kept.

## Capabilities

### New Capabilities
- `web-ui-layout`: tokens, bundled fonts, shell regions, responsive rules, synth panel collapse and persistence, Knob interaction, step view, delay-card copy, settings placeholder.

### Modified Capabilities
- `web-editor`: "fixed-size, fit within 800x680" becomes "resizable with limits". The flag-off standalone window now matches the legacy editor size instead of staying unchanged.
- `plugin-host-integration`: the standalone shell sizes itself to its editor. The "window size unchanged" rule for flag-off is revised.
- `web-ui-controls`: delay recommendations are shown as clickable copy chips. Range controls may render as knobs, with the same labels, disabled states and dispatches.

## Decisions

| # | Decision | Rationale |
|---|---|---|
| D1 | Restyle in place with CSS tokens + Grid | Reuses the Slice 2 store, bridge, logic and tests. No runtime dependencies. |
| D2 | The Knob wraps a hidden native range input | Keeps `aria-label`, `.disabled`, the keyboard behavior and the existing tests |
| D3 | The collapsed panel stays mounted; open state lives in `localStorage` | Labeled controls stay reachable. UI-only state, no bridge change. |
| D4 | Fonts from `@fontsource`, embedded through the existing asset pipeline | No CDN. `WebAssets.cpp` already maps `woff2`. |
| D5 | Clipboard in TS only | The embedded origin is a secure context. A native fallback only if proven necessary. |
| D6 | The sizing fix applies to both paths | The legacy standalone window was clipped. The plugin legacy editor size is unchanged. |
| D7 | Vitest/jsdom tests plus a manual visual and responsive checklist (standalone + Sonar) | jsdom cannot check layout. Playwright is deferred. |

## Delivery forecast

Forecast: about 900-1100 changed lines (CSS ~250, shell ~220, Knob + tests ~230, other tests ~200, C++ ~40, specs ~80). Lockfile and generated font sources are excluded. That is **over the 800-line budget**.

Trims that keep the slice goal intact (about -120 to -150 lines):

| Trim | Saves | Effect |
|---|---|---|
| T1 | ~60 | Defer the wheel input and double-click reset (keep drag + keyboard). This also removes the need for per-control defaults. |
| T2 | ~25 | Drop the `execCommand` fallback; a clipboard failure shows an error status |
| T3 | ~40 | Use one responsive breakpoint instead of two |

Even with these trims, staying under 800 is uncertain. If a split is needed, use the explorer's split:
- **PR A:** shell, tokens, fonts, synth panel with the existing Sliders, step view, C++ sizing, delta specs.
- **PR B:** Knob, delay card + copy, remaining tests.

The workload guard decides between the split and a size exception.

## Affected Areas

| Area | Impact |
|---|---|
| `ui/src/App.tsx`, `ui/src/components/**`, `ui/src/styles/*.css` (new), `ui/src/lib/knob.ts` (new), `ui/src/main.tsx`, `ui/index.html` | Modified/New |
| `ui/package.json`, lockfile | Font devDependencies |
| `Source/ui/WebEditor.cpp`, `Source/MainComponent.cpp`, `Source/Main.cpp` | Sizing |
| `openspec/specs/{web-editor,plugin-host-integration,web-ui-controls}` | Delta specs |

## Risks

| Risk | Likelihood | Mitigation |
|---|---|---|
| Over the review budget | High | Trims T1-T3, or the PR A/B split |
| A host ignores the resize | Med | The minimum-size layout must be fully usable; check in Sonar |
| Tests depend on the DOM (labels, `data-role`) | Med | Keep the labels and keep the panel mounted. Update the delay-card assertions on purpose. |
| Clipboard blocked in a DAW WebView2 | Low-Med | Fallback/status, manual check |
| Font assets grow the generated sources | Low | Latin subset only; check that the generated sources stay gitignored |
| Knob math on skewed controls | Med | Route through the existing `SKEWS`; unit-test the helper |

## Rollback Plan

`BERLIN_WEB_UI` stays OFF by default, so the UI changes do not affect default builds. To roll back, revert the slice PR(s). The flag-off `MainComponent` sizing change is a separate commit that can be reverted alone; doing so restores 800x680.

## Dependencies

Slice 2 (`feat/ui-bridge-parity-pr3`, not yet merged; this branch is stacked on it), pnpm, Node 24, `@fontsource` packages.

## Success Criteria

- [ ] With the flag ON, the standalone and Sonar match the mockup layout and theme. All parity actions still work.
- [ ] The editor resizes within its limits and stays usable at the minimum size, with the synth panel open or closed.
- [ ] The synth panel open state survives closing and reopening the editor.
- [ ] Knobs work with the keyboard, report `aria-label`, and reflect the disabled state.
- [ ] Clicking a delay time copies its ms value.
- [ ] Flag-off standalone shows the full legacy editor (no clipping). `BerlinTests` and Vitest pass.

## Proposal question round

Auto mode, so no questions were asked. Assumptions for user review:
1. Master level stays in the top bar until Slice 4 moves it into Settings. The mockup omits it.
2. Accept the flag-off window growing to 800x952, or gate the fix behind the flag.
3. Below about 1100 px wide, the synth panel overlays the main area rather than pushing it.
4. Which trims (T1-T3) are acceptable, or whether you prefer a PR split.
