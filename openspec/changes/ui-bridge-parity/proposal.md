# Proposal: Full Bridge + Functional Parity (UI Redesign Slice 2/6)

## Intent

Slice 1 proved the WebView2 stack, but `WebEditor` only offers BPM −/+ and a playhead row. Every legacy-editor action must now work through `UiBridge` in a plain, unstyled React UI, so Slices 3-5 (styling, MIDI devices, legacy removal) can build on a complete, tested foundation. The 15 existing bridge commands already cover every legacy action; the gaps are host-side (snapshot event, native dialogs, preset-list cost, `loadPreset` busy) and UI-side (protocol, store, mock, ~40 controls).

## Scope

### In Scope
- **Host (C++):**
  - `WebEditor` becomes a `ChangeListener`. The callback only sets a dirty flag, held in a gui-free `WebEditorSupport` helper. The existing 30 Hz timer emits a `snapshot` event when the browser is visible and the flag is set, and clears the flag only after sending. The destructor removes the listener first.
  - Native functions `chooseExportFile()` → `{cancelled, path}` and `confirm(title, message)` → `bool`. They reset the chooser in the destructor, guard callbacks with `SafePointer`, and reject re-entry with a busy result.
  - Preset validity cache in `PresetManager`: the directory is still listed on every snapshot (cheap), and only the XML parse result is cached, keyed by file name + size + modification time. `presetNames` stays in the snapshot and is always current.
  - `loadPreset` returns `ok:false, error:"busy"` when `regenerate(false)` fails (G6).
  - Remove the duplicate `owner` member in `WebEditor`. Optional bridge error logging.
- **UI (TS):**
  - Typed `protocol.ts`: commands, snapshot, events, error tokens, hand-written limits and enum lists.
  - A `Bridge` interface with `nativeBridge` and `mockBridge`. The mock loads only in DEV with no host, through a dynamic import.
  - An external store built on `useSyncExternalStore`, with optimistic updates, per-key latest-wins coalescing, a sequence counter that drops stale responses, and separate playhead, status and draft slices. It replaces the Slice 1 `changeBpm` stale closure.
  - A `"dev": "vite"` script with a pinned `strictPort`.
  - Parity controls, including the UI-only legacy behaviors listed in `explore.md` §1, the TS delay recommendations and the export/save dialog flows.
  - Minimal Play/Stop and master level controls.

### Out of Scope
- The pre-build lock and timeout issues, range normalization in `setGenerationParams`, and the enum-decode duplication.
- MIDI device selection (Slice 4), styling and layout (Slice 3), removing the legacy editor (Slice 5).
- New `UiBridge` commands, and a snapshot `revision` field.

## Capabilities

### New Capabilities
- `web-ui-controls`: the TS protocol, the store (ordering, coalescing, optimistic rules), the mock bridge, the parity controls and their UI-only rules, the status messages and the dialog flows.

### Modified Capabilities
- `web-editor`: the native functions `chooseExportFile` and `confirm`, the `snapshot` event, and the ChangeListener lifetime. "PoC UI Content" (which forbids other controls) is replaced by a pointer to `web-ui-controls`.
- `ui-bridge`: `loadPreset` returns `busy` when regeneration fails. `presetNames` stays current, including preset files added, changed or removed outside the app (no observable change; behavior note only).
- `plugin-host-integration`: no delta.

## Decisions (assumptions the user may revisit)

| # | Decision |
|---|---|
| D1 | Generation params are sent live, not staged until Generate. The behavior is equivalent because auto-evolve uses `currentSequence`. |
| D2 | Minimal Play/Stop and master level controls go in Controls A (PR3). |
| D3 | No snapshot `revision`. Ordering relies on the message thread plus a UI sequence counter. |
| D4 | The export default path (`~/Documents/Berlin/berlin-export.mid`) is owned by C++ in `chooseExportFile`. |
| D5 | Preset validity cache keyed by file name + size + modification time; the directory is listed on every snapshot, only the XML parse is cached. `presetNames` stays in the snapshot and current. User-approved 2026-10-03 (replaces the save/load-invalidated name cache). |
| D6 | TS limits are hand-written, plus a Vitest regex drift guard that reads `Source/synth/SynthPatch.h` `kMin*/kMax*` through a relative `fs` path. If design finds that needs build plumbing, limits stay hand-written only and the drift risk is accepted. |
| D7 | Spec deltas as listed under Capabilities. |
| D8 | Manual dialog checks run in both standalone and Cakewalk Sonar. |

## Approach

Exploration approach B: a dependency-free external store with selectors, so the ~40 controls don't all re-render on every change. `UiBridge` stays pure, and the dialogs live in `WebEditor`. TS orchestrates the flows:
- **Export:** `chooseExportFile`, then `exportMidi`.
- **Save:** `savePreset{overwrite:false}`; on `exists`, `confirm`, then `savePreset{overwrite:true}`.

Strict TDD:
- C++ tests for the dirty-flag gate, the cache and `loadPreset` busy.
- Vitest (jsdom@^25, no `@testing-library`, no new runtime dependencies) for the store, the protocol and the mock, plus DOM smoke tests.
- pnpm only.

## Delivery (feature-branch-chain, 800-line budget)

| PR | Branch target | Scope | Lines |
|---|---|---|---|
| PR1 C++ host | `feat/ui-webview-poc-pr2` | Host items, tests, `web-editor` + `ui-bridge` deltas | ~350-450 |
| PR2 TS foundation | PR1 | Protocol, limits, `native.ts` rewrite, store, mock, `dev` script, Vitest | ~650-800 |
| PR3 Controls A | PR2 | Status line, transport, BPM, level, Synth/FX, generation, rhythm, pitch, evolution, export flow | ~700-800 |
| PR4 Controls B | PR3 | Synth panel + skew helper, delay Sync memory + recommendations, reverb, presets UI | ~700-800 |

PR1 and PR2 are logically independent but stacked. PR3 and PR4 need both.

## Affected Areas

| Area | Impact |
|---|---|
| `Source/ui/WebEditor.{h,cpp}`, `Source/ui/WebEditorSupport.*` | Modified |
| `Source/plugin/BerlinAudioProcessor.*`, `Source/preset/PresetManager.*` | Modified: cache, `loadPreset` busy |
| `Source/bridge/UiBridge.cpp` | Modified: `loadPreset` result |
| `Tests/` | New tests |
| `ui/src/bridge/`, `ui/src/store/`, `ui/src/components/`, `ui/src/App.tsx`, `ui/package.json`, `ui/vite.config.ts` | New/Modified |

## Risks

| Risk | Likelihood | Mitigation |
|---|---|---|
| A stale snapshot overwrites an optimistic value | Med | Sequence counter plus per-key coalescing |
| Dialog lifetime, re-entry, WebView2 modality and focus | Med | `SafePointer`, busy result, manual checks in standalone and Sonar |
| Preset cache returns a stale parse if a file changes within the filesystem timestamp resolution without a size change | Low | Key includes size and mtime; tested with an injected clock/file stub where feasible |
| Mock or TS limits drift from C++ | Med | Shared fixtures, D6 drift guard |
| The `native.ts` rewrite breaks the Slice 1 tests | High | Rewrite them in PR2 |
| A PR goes over budget | Med | Split strictly per the table |

## Rollback Plan

`BERLIN_WEB_UI` stays off by default, so flag-off builds are unchanged. Revert the PRs in reverse order (PR4 → PR1). Each PR can be reverted alone: PR1's C++ fixes are backward-compatible with the Slice 1 UI.

## Dependencies

Slices 0 and 1 (archived), pnpm, Node 24, the WebView2 runtime.

## Success Criteria

- [ ] Every legacy-editor action works in `WebEditor` with the flag on, in standalone and Sonar, per a manual parity checklist.
- [ ] Export and overwrite dialogs behave correctly, including closing the editor mid-dialog, in both hosts.
- [ ] Auto-evolve and state restore update the UI through the `snapshot` event.
- [ ] Slider drags do not scan the preset folder.
- [ ] Flag-off builds are unchanged. `BerlinTests` and Vitest pass, and `pnpm --dir ui dev` runs against the mock.

## Proposal question round

Auto mode, so no questions were asked. Assumptions for user review: decisions D1-D8, plus:
1. Resolved 2026-10-03: the user approved the mtime-keyed validity cache, so external preset changes stay visible.
2. A Sonar-only dialog failure blocks the slice.
