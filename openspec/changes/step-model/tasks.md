# Tasks: Step Model (Variable Length + True Skip)

## Review Workload Forecast

| Field | Value |
|-------|-------|
| Estimated changed lines | ~685 (range 650-780; prod ~385, tests ~420 incl. Vitest) |
| 400-line budget risk | High (800-line budget: fits, Medium) |
| Chained PRs recommended | No |
| Suggested split | Single PR, 5 commits |
| Delivery strategy | single-pr (800-line budget) |
| Chain strategy | size-exception not needed under 800 budget |

Decision needed before apply: No
Chained PRs recommended: No
Chain strategy: pending
400-line budget risk: High

Cut order if over 800: RhythmPanel dynamic caps, then mock resize.

### Suggested Work Units (commits in one PR)

| Unit | Goal | Likely PR | Focused test command | Runtime harness | Rollback boundary |
|------|------|-----------|----------------------|-----------------|-------------------|
| 1 | Core + generation (P1-2) | PR 1 | `BerlinTests.exe --category=Berlin` | Reproducibility goldens green | core/, generation/ + tests |
| 2 | Player + export (P3-4) | PR 1 | `BerlinTests.exe --category=Berlin` | Manual playback (P9) | playback/, export/ + tests |
| 3 | Mutation, edit, processor, bridge (P5-6) | PR 1 | `BerlinTests.exe --category=Berlin` | N/A: covered by bridge tests | plugin/, bridge/, MutationEngine, StepEdit |
| 4 | React (P7) | PR 1 | `pnpm test` in `ui/` | Manual UI (P9) | ui/src only |

## Phase 0: Baseline
- [x] 0.1 Run `Tests/Builds/VisualStudio2026/x64/Debug/ConsoleApp/BerlinTests.exe --category=Berlin`; record pass count. (450 test cases, all passed)
- [x] 0.2 `pnpm test` in `ui/`; record pass count (541 expected). (541 passed)

## Phase 1: Core model + StepCursor (sequencing-core)
- [x] 1.1 RED: `Tests/Source/StepTests.cpp`: `skipped` defaults false, `==` sees it, `Step{note,active}` compiles; `SequenceTests`: `kMaxSteps == 64`.
- [x] 1.2 RED: new `StepCursorTests.cpp` (add to `Tests/BerlinTests.jucer`): from -1 first playable; skipped step 0 wraps; single playable wraps every call; all-skipped/empty = -1; `countPlayableSteps`.
- [x] 1.3 GREEN: `Source/core/Step.h` add `skipped`; `Sequence.h` add `kMaxSteps`; create `Source/core/StepCursor.h` (header-only, noexcept); register in `Berlin.jucer`.

## Phase 2: Generation (deterministic-generation, euclidean-rhythm)
- [x] 2.1 RED: `SequenceBuilderTests`: `randomActiveCount` 16→11, 1→1, 64→44; lengths 1/8/64 per mode; euclid pulses/rotation clamped; no generated skips; length 16 equals default.
- [x] 2.2 GREEN: `GenerationParams.h` `length=16`; `SequenceBuilder.{h,cpp}` clamp 1..64, `randomActiveCount`, pass length to generators.
- [x] 2.3 Check `ReproducibilityTests` unchanged and green.

## Phase 3: Player (step-event-scheduling, auto-evolution)
- [x] 3.1 RED: `SequencePlayerTests`: skipped takes no boundary; inactive does; loop at wrap with skipped step 0; restart-adopt no count; all-skipped silent, loopCount 0, playhead holds; length 1 counts every boundary.
- [x] 3.2 RED: `SequencePlayerHandoffTests`: cursor continues; shrink below cursor goes to first playable, no loop; skipped cursor needing wrap no count; never resets to 0; skipped sounding step note-off at offset 0.
- [x] 3.3 GREEN: `SequencePlayer.{h,cpp}` `cursor=-1`; boundary via `nextPlayableStep`; D4 adopt fix; reset in `prepare()`/`reset()`; drop `stepCounter % size`.

## Phase 4: Export (midi-export-timeline)
- [x] 4.1 RED: `MidiExportTimelineTests`: skipped zero ticks; all-skipped ok/empty/endTick 0; extend player-vs-export equivalence with skip/rest mix.
- [x] 4.2 GREEN: `MidiExportTimeline.cpp` `repeats * countPlayableSteps`, cursor via `nextPlayableStep`.

## Phase 5: Mutation + step edit (sequence-mutation, step-editing)
- [x] 5.1 RED: `MutationEngineTests`: add/removeNote never pick skipped; stretch/palindrome carry flag.
- [x] 5.2 RED: `StepEditTests`: index 16..63 valid within size; index == size rejected; `skipped` applied.
- [x] 5.3 GREEN: `MutationEngine.cpp` (use `kMaxSteps`, exclude skipped); `StepEdit.{h,cpp}` optional `skipped`, drop `kEditableSteps`.

## Phase 6: Processor + bridge (generation-live-control, ui-bridge)
- [x] 6.1 RED: `BerlinAudioProcessorTests`: absent `skipped` keeps flag; `setGenerationParams{length}` leaves playing sequence; next Generate rebuilds at length; Randomize/loadPreset keep length.
- [x] 6.2 RED: `UiBridgeTests`: length 0→1, 99→64; pulses/rotation clamped to length; `skipped:"x"` gives `invalid type: skipped`; snapshot has `skipped`/`length`; still 16 commands.
- [x] 6.3 GREEN: `BerlinAudioProcessor.{h,cpp}` `editStep(..., optional<bool>)`, clamp staged length, no new busy path; `UiBridge.cpp` int field, widened ranges, `encodeSteps`.

## Phase 7: React (web-ui-controls)
- [x] 7.1 RED Vitest: `stepRow` with count; StepView 24 cells for 24 steps, `data-skipped`, skip button sends `setStep{skipped:true}`; Length slider sends `gen.length`; mock resize/skipped; protocol typing; out-of-range playhead index (>= steps.length) highlights nothing (stepRow and StepView).
- [x] 7.2 GREEN: `protocol.ts`, `limits.ts`, `mock.ts`, `steps.ts` (`ROW_SIZE`), `actions.ts`, `store.ts`, `StepView.tsx`, `RhythmPanel.tsx`, `styles/components.css`.

## Phase 8: Build and test
- [x] 8.1 MSBuild `Builds/VisualStudio2026/Berlin.sln` and `Tests/Builds/VisualStudio2026/BerlinTests.sln` (Debug x64); run `BerlinTests.exe --category=Berlin`; 0 failures.
- [x] 8.2 `pnpm tsc` and `pnpm test` in `ui/`; 0 failures.
- [x] 8.3 `git diff --stat` under 800 lines. NOT MET (~1333 added / 172 deleted; tests ~1030, prod ~290, project files 16). The user accepted `size:exception` for a single PR on 2026-10-17.

## Phase 9: Manual [M]
- [ ] 9.1 Lengths 1/7/64 in standalone and plugin; skip while playing; export matches what is heard.

## Phase 10: Archive (separate)
- [ ] 10.1 `sdd-verify`, then `sdd-archive`; diff archive output (folder-duplication risk).
