# Apply Progress: step-gate

Mode: Strict TDD. Status: Phases 0-9 done (32/39 tasks); Phase 10 (manual) and Phase 11 (archive) are for the user/orchestrator.

## Results

| Check | Result |
|---|---|
| C++ `BerlinTests.exe --category=Berlin` | 558 tests, 0 failures (baseline 522, +36) |
| Vitest `pnpm test` | 583 passed (baseline 567, +16) |
| `pnpm tsc` | exit 0 |
| `Builds/VisualStudio2026/Berlin.sln` Debug x64 | builds (only pre-existing C4100 warnings in Main.cpp), no LNK1168 |
| `git diff --cached --stat` | unchanged from before apply (diffed against a saved copy) |

## Size (step-gate only: working tree vs the staged velocity index)

- `git diff --stat`: 1303 insertions, 119 deletions over 34 tracked files, plus one new untracked file `Tests/Source/GateStreamSupport.h` (55 lines). About 1477 authored lines against the 800 budget (forecast was 530).
- Split: production about 231 lines (183 added, 48 deleted); tests about 1250. C++ 1001+/22-; UI 302+/97- (about 49 of the UI deletions are the mechanical fixture type fix).
- Cause: the stream-order, block-size, tempo, handoff, parity and gate-100 identity tests (all on the NEVER-cut list) are expansive, plus Processor/UiBridge/StepView/mock/store coverage. The cut order saves under 100 lines (item 1 is already one parameterized loop; item 4 already asserts only the first off), so it was not applied: it cannot reach 800.
- Recommended if the 800 budget is binding: split as tasks.md suggests (PR A = Phases 1-7 C++ engine, bridge and tests, about 1020; PR B = Phase 8 React, about 400). A is still over 800; a size:exception or further split inside the player tests would be needed.

## TDD Cycle Evidence

| Task | Test File | Layer | Safety Net | RED | GREEN | TRIANGULATE | REFACTOR |
|------|-----------|-------|------------|-----|-------|-------------|----------|
| 1.1/1.2 | `Tests/Source/StepTests.cpp` | Unit | 522/522 baseline | Written (but see deviation 1) | Passed (compiled and ran inside the 541 run) | 3 tests (defaults/aggregate, equality, predicate) | None needed |
| 2.1-2.6 | `SequencePlayerTests.cpp`, `SequencePlayerHandoffTests.cpp` | Unit | 522 (+3 Step) | Run: gate stream, block-size, block-end, rounding and handoff tests failed before the player change | Passed (541) | Gate levels 25/50/75, block sizes 1/64/511/4096, off on block end, sps 60/10 rounding, gate 99 onto the boundary, slower and faster tempo, rest/skipped, gate-100 legacy stream | None needed |
| 2.7 | `SequencePlayerStopTests.cpp` | Unit | same | Run: flush-after-gate-off failed | Passed | direct stop, flush after fired off, prepare, reset | None needed |
| 2.8 | (production) `SequencePlayer.{h,cpp}` | n/a | same | n/a | 541 pass | n/a | `kNoGateOff` moved to the header (needed by the free helper) |
| 3.1 | `MutationEngineTests.cpp` | Unit | 541 | Pin test, expected to pass at once | Passed (542) | add/remove/reverse/rotate/stretch | None needed |
| 4.1-4.3 | `MidiExportTimelineTests.cpp` | Unit | 542 | Run: gate/rounding/parity tests failed | Passed (548) | 25/50/75/100, 1-tick clamp (3 cases), mixed repeats ordering, short last step, skipped/rest, parity g=1..99 against the real player | None needed |
| 5.1/5.2 | `StepEditTests.cpp` | Unit | 548 | Written; compiled-failing (no `gateOutOfRange`) | Passed (551) | bounds, order vs index/note/velocity, muted/skipped | None needed |
| 6.1-6.3 | `BerlinAudioProcessorTests.cpp` | Unit/Integration | 551 | Run: compile errors (6-arg `editStep`) | Passed (556) | absent/present, out of range, busy publish and restore, regenerate/randomize/loadPreset reset, processBlock off sample 3000 and export tick 120 | None needed |
| 7.1/7.2 | `UiBridgeTests.cpp` | Integration | 556 | Run: the new gatePercent tests failed | Passed (558) | set/absent, 0/101, "x"/1.5/Infinity, order vs step/note/velocity/busy, 5 keys | None needed |
| 8.1/8.4 | `ui/src/lib/dynamics.test.ts` | Unit | 567/567 | Run: failed | Passed | both tables, ties, wrap, off-table | Generic `Level<N>` |
| 8.2/8.4 | `mock.test.ts`, `protocol.test.ts`, `store.test.ts`, `status.test.ts` | Unit | 567 | Run: 36 failed total with the fixture updates | Passed | default/keep/0/101/1.5, order, coalesce, mapped message | None needed |
| 8.3/8.5 | `StepView.test.tsx` | Component | 567 | Run: failed | Passed (583) | wrap, 4-click cycle, keeps fields, roving tabindex, sibling order | None needed |

### Test Summary
- Total tests written: 52 (36 C++ + 16 Vitest)
- Total passing: 558 C++ + 583 Vitest
- Layers: Unit, Integration (processor, bridge), Component (StepView)
- Approval tests: none (no refactoring tasks; existing velocity expectations were updated only by adding `gatePercent: 100`)
- Pure functions created: `gateOffAbs` (file-local), `nearestLevel`/`nextLevel` generic

## Deviations and notes

1. Phase 1: the Step.h GREEN edit landed in the same step as its tests, before the failing run was executed (the constants and field did not exist, so the tests were guaranteed to fail to compile). The RED was written first but not run separately.
2. `nearestLevel`/`nextLevel` now take `(levels, value)` per D8; the velocity call sites and tests pass `VELOCITY_LEVELS`. No separate velocity wrapper exports were kept (they would collide with the old names). Expectations are unchanged.
3. `kNoGateOff` lives in `SequencePlayer.h` (namespace scope) instead of file-local, because the member initializer needs it.
4. D5 (`prepare()`/`reset()` clearing `pendingOffAbs`) is defensive: a surviving note is always cut by the boundary at position 0, so no test can tell it from the stale value. The behavior is pinned, not the line itself.
5. Enter/Space on the gate button: it is a native `<button type="button">`; jsdom does not synthesize click from keydown, so the test asserts the element type instead.
6. Status text for `gate-out-of-range` was added in `store/status.ts` ("That gate length is not valid."). `velocity-out-of-range` still has no mapped text (pre-existing, left alone).
7. New file `Tests/Source/GateStreamSupport.h` (header-only helper, not registered in the .jucer). It is untracked and not staged.
8. 44 px target: the global `button { min-height: 44px }` already applies; `.step-gate` joins the `.step-vel, .step-skip` rule exactly as the siblings do.
9. `data-gate-length` sits on `.step`; `step-controls` is a 2-column grid with skip spanning; a direct `stop()` fires no gate off and flush emits exactly one if still sounding (orchestrator reconciliation).

## Files changed (working tree only, nothing staged)

C++: `Source/core/Step.h`, `Source/playback/SequencePlayer.{h,cpp}`, `Source/export/MidiExportTimeline.{h,cpp}`, `Source/generation/StepEdit.{h,cpp}`, `Source/plugin/BerlinAudioProcessor.{h,cpp}`, `Source/bridge/UiBridge.cpp`; tests `StepTests`, `SequencePlayerTests`, `SequencePlayerHandoffTests`, `SequencePlayerStopTests`, `MutationEngineTests`, `MidiExportTimelineTests`, `StepEditTests`, `BerlinAudioProcessorTests`, `UiBridgeTests`, new `GateStreamSupport.h`.

UI: `lib/dynamics.ts`, `bridge/{limits,protocol,mock}.ts`, `store/{store,status}.ts`, `components/StepView.tsx`, `styles/components.css`; tests `dynamics`, `mock`, `protocol`, `store`, `status`, `StepView`, `App`.
