# Exploration: ui-bridge-parity (UI redesign Slice 2/6)

Full report in Engram: `sdd/ui-bridge-parity/explore` (#362). Key claims spot-checked by the orchestrator: `sendChangeMessage` at `BerlinAudioProcessor.cpp:170` and `:401`, no `dev` script in `ui/package.json`, `PresetManager::listPresetNames` fully loads every preset.

## Summary

The 15 existing UiBridge commands already cover every action in the legacy editor, so no new bridge command is needed. The work:

- a typed TS protocol;
- a small external store built on `useSyncExternalStore`;
- a mock bridge;
- a host-side `snapshot` event;
- two native dialog functions;
- about 40 plain controls;
- two small C++ fixes: preset-list caching and the `loadPreset` busy result.

Estimated at 2,400-2,900 authored lines, so it must be chained.

## 1. Legacy editor inventory

The editor is `berlin::BerlinAudioProcessorEditor` (`Source/plugin/BerlinAudioProcessorEditor.*`). It has about 40 raw JUCE widgets and no child components.

| Control | Processor call | Bridge |
|---|---|---|
| Export MIDI (FileChooser) | `exportMidiTo(file)` | `exportMidi{path}`. GAP: native chooser |
| Synth / FX toggles | `setSynthEnabled` / `setEffectsEnabled` | same |
| Tempo | `setBpm` | `setBpm` (the engine recomputes the synced delay) |
| Delay Sync, Division, Time, Feedback, Mix | `setPatch` | `setPatch` |
| Delay recommendations label | `formatDelayRecommendations(bpm)` | GAP. Compute in TS |
| Reverb Room, Damping, Wet, Dry | `setPatch` | `setPatch` |
| Seed (validated `-?digits`) | `setSeed` | `setSeed{seed:string}` |
| Generate / Randomize / Mutate | `setGenerationParams` + `regenerate(false/true)` / `mutate` | same |
| Lock Seed, Rhythm mode, Pulses, Rotation, Chance %, Scale, Root, Range Lo/Hi | `setGenerationParams` | `setGenerationParams` |
| Preset name + Save (overwrite prompt) | `save(name)` | `savePreset{name,overwrite}`. GAP: native confirm |
| Preset box + Load | `loadPreset(name)` | `loadPreset` |
| Auto-Evolve, Rate | `setAutoEvolveEnabled` / `setAutoEvolveRate` | same |
| Waveform, Pulse Width, Cutoff, Resonance, ADSR, LFO Dest/Rate/Depth | `setPatch` | `setPatch` |
| Status label | local | UI-local; map error tokens to the legacy messages |

The legacy editor has no master level, Play/Stop, or MIDI device controls. The bridge supports master level and Play/Stop. MIDI devices belong to Slice 4.

UI-only behaviors that must move to TS:

- Sync to Free restores the last manual delay time.
- Delay time is disabled while Sync is on.
- Delay and reverb are disabled while FX is off.
- Randomize is disabled while Lock Seed is on.
- Save is disabled with an empty name, and Load is disabled with no selection.
- After a load, the name field shows the loaded name.
- An invalid seed shows a red "Seed must be a whole number." message.
- Skewed sliders. Midpoints: cutoff 1000, resonance 2, attack 0.2, decay 0.3, release 0.5, LFO rate 2.
- Status messages from `describePresetFailure` / `describeWriteFailure`.

Semantic differences:

- The legacy editor staged rhythm params until Generate. The bridge commits them immediately. The behavior is equivalent because auto-evolve uses `currentSequence`.
- Legacy widgets reset on reopen. The snapshot restores the real values, which is an improvement.

## 2. Gaps

- **G1:** there is no `snapshot` event.
- **G2:** there is no native file chooser.
- **G3:** there is no native overwrite confirm.
- **G4:** the UI must send `rangeLow` and `rangeHigh` together to get `normalizePitchRange`.
- **G5:** a snapshot calls `listPresetNames()`, which parses every preset on every dispatch.
- **G6:** `loadPreset` ignores the result of `regenerate(false)`, so it reports ok while busy.

## 3. Snapshot

The snapshot already has:

- `patch`, `generationParams`, `seed`, `bpm`;
- `playing`, `playheadStep`, `loopCount`;
- `synthEnabled`, `effectsEnabled`, `masterLevel`;
- the auto-evolve fields and `mutationCount`;
- `steps[16]` and `presetNames`.

What's missing, and how to cover it:

- **Delay recommendations:** compute them in TS. Factors are 2, 1, 0.75, 0.5, 1/3 and 0.25; labels are `1/2, 1/4, 1/8., 1/8, 1/8T, 1/16`; values are rounded to whole ms.
- **Limits and enum lists:** hand-write them in TS.
- **Revision counter:** optional.

## 4. Change notification

`sendChangeMessage()` fires only after `setStateInformation` and after the auto-evolve mutate. Both run on the message thread. Manual commands rely on the snapshot in their response.

Proposal:

- `WebEditor` becomes a `ChangeListener`. Its destructor calls `removeChangeListener` first.
- The callback only sets a dirty flag.
- The existing 30 Hz timer emits `snapshot` when the browser is visible and the flag is set, then clears the flag only after sending.
- The flag lives in a gui-free helper in `WebEditorSupport`, with unit tests.
- The playhead stays a separate event and a separate store slice.

## 5. Native dialogs

Add WebEditor native functions and keep `UiBridge` pure:

- `chooseExportFile()` resolves to `{cancelled, path}`. The default file is `~/Documents/Berlin/berlin-export.mid`.
- `confirm(title, message)` resolves to `bool`.

The UI orchestrates the flows:

- **Export:** `chooseExportFile` → `dispatch('exportMidi')`.
- **Save:** `savePreset{overwrite:false}`. On `exists`, call `confirm`, then send `savePreset{overwrite:true}`.

Lifetime and safety:

- Reset the chooser in the destructor.
- Guard callbacks with `SafePointer`, and never call `done` once the editor is gone.
- Reject re-entry with a busy result.
- Manual checks are needed: destroying the chooser mid-dialog, and focus inside WebView2 in standalone and in Sonar.

## 6. Mock bridge and dev server

- Add `"dev": "vite"`, and pin `server.port` / `strictPort` so the port matches `BERLIN_WEB_UI_DEV_URL`.
- Define one `Bridge` interface with `dispatch`, `getSnapshot`, `onPlayhead`, `onSnapshot`, `chooseExportFile` and `confirm`.
- Provide two implementations: `nativeBridge` and `mockBridge`.
- Load the mock only when there is no host and `import.meta.env.DEV` is true, using a dynamic import so it never lands in the embedded assets.
- A Debug JUCE build with the dev URL uses the real bridge plus Vite hot reload.
- The mock stays minimal and is tested against the same fixtures and error tokens.

## 7. Approaches

- **Store, recommended B:** a small external store with `useSyncExternalStore`. It needs no dependency, is testable without a DOM, and uses selectors.
  - It includes optimistic updates, per-key latest-wins coalescing (one request in flight, one pending), a sequence counter that drops stale responses, and separate playhead, status and draft slices.
  - Rejected: `useReducer` + Context (re-renders all ~40 controls on every change) and Zustand (a new dependency).
- **Protocol:** hand-written TS (`protocol.ts`), with an optional Vitest drift guard against the `kMin*/kMax*` values in `SynthPatch.h`. Rejected: generating from the C++ D9 tables (needs a C++ emitter or parser and build plumbing) and a runtime `getSchema` (the mock would have to duplicate it).

## 8. Size and split (800-line budget)

| PR | Scope | Lines |
|---|---|---|
| PR1 C++ host | ChangeListener + `snapshot` event + gate helper; `chooseExportFile`/`confirm`; lifetime guards; remove the duplicate `owner`; `loadPreset` busy fix; preset-list cache; tests; `web-editor` spec delta | ~350-450 |
| PR2 TS foundation | protocol, limits, tokens; `native.ts` rewrite; store; mock; `dev` script; Vitest | ~650-800 |
| PR3 Controls A | status line, transport/BPM, Synth/FX, generation/rhythm/pitch/evolution, export flow | ~700-800 |
| PR4 Controls B | synth panel with the skew helper, delay Sync memory + recommendations, reverb, presets UI | ~700-800 |

PR1 and PR2 don't depend on each other. PR3 and PR4 depend on both. The gate is a manual parity checklist, run in standalone and in Sonar with the flag on.

## 9. Follow-ups that belong here

- **Include:**
  - the Slice 1 `changeBpm` stale closure (replaced by the store);
  - the duplicate `WebEditor` `owner`;
  - the Slice 0 `loadPreset` ok-while-busy;
  - optionally, bridge error logging.
- **Defer:**
  - the pre-build lock and timeout items;
  - the `setGenerationParams` range normalization (the bridge already normalizes);
  - the enum-decode duplication.

## Risks

1. Snapshot cost: a preset disk scan on every dispatch. Mitigate with a cache plus UI coalescing.
2. Stale or out-of-order snapshots overwriting optimistic values. Mitigate with a sequence counter and an optional revision.
3. Snapshot events missed while the browser is hidden. Mitigate with the dirty flag.
4. Dialog lifetime and re-entrancy, plus WebView2 modality quirks (manual-only).
5. The mock drifting from the engine, and TS limits drifting from C++.
6. Rewriting `native.ts` breaks the current mocks in the tests. There's no `@testing-library`, so prefer store tests plus DOM smoke tests.
7. Size: split strictly.

## Open questions

1. Live or staged generation params? Recommended: live.
2. Add Play/Stop and master level? Recommended: yes, minimal, in PR3.
3. Add a `revision` field to the snapshot, or rely on message-thread ordering plus the sequence counter?
4. Should the export default path live in C++ or in TS?
5. Preset-list fix: a processor cache, or take `presetNames` out of the per-command snapshot?
6. TS limits: add the regex drift test, or not?
7. Spec deltas: `web-editor` (new functions + event); `ui-bridge` only if snapshot or cache behavior changes.
8. Manual dialog checks: standalone only, or also Sonar?
