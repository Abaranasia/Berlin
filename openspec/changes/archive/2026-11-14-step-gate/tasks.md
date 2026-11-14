# Tasks: Step Gate (Per-Step Gate Length End to End)

## Review Workload Forecast

| Field | Value |
|-------|-------|
| Estimated changed lines | ~530 (range 500-700; prod ~150, tests ~380 per design; step-velocity landed above its forecast, so watch the upper range) |
| 800-line budget fit | Yes (comfortable at 530, still fits at 700) |
| Chained PRs recommended | No |
| Suggested split | None. If the diff exceeds 800, split after Phase 6: PR A = Phases 1-6 (C++ engine), PR B = Phases 7-8 (React) |
| Delivery strategy | single-pr (cached for the session; size:exception was only needed by the predecessor) |
| Chain strategy | not applicable |

Decision needed before apply: No
Chained PRs recommended: No
400-line budget risk: High (530 forecast is over the 400 default; the 800 review budget for this session is respected)

Cut order if over 800 (design; apply in this order):
1. Merge the block-size variants (1/64/511/4096) into one parameterized loop.
2. Drop the protocol/App fixture-only edits to the minimum type fix.
3. Merge the mock cases into the StepView tests.
4. Reduce the Processor export assertion to the first note-off.

NEVER cut: stream-order tests, tempo-change tests, keepPosition tests, the sample/tick parity test, the gate-100 identity tests.

Strict TDD: every phase lists RED (failing test) before GREEN.

### HARD CONSTRAINT: git state (applies to every phase)

`step-velocity` is STAGED in git as a reviewed snapshot (index holds the receipt-bound content). The apply phase MUST NOT stage, unstage, commit, stash, reset, checkout or restore anything. No `git add`, `git restore --staged`, `git commit`, `git stash`, `git reset`. Read-only git commands (`git status`, `git diff`, `git diff --stat`) are allowed. All gate edits land as unstaged working-tree changes on top of the staged velocity tree; the user stages and commits. New files (none expected in Source/) also stay untracked/unstaged.

### Suggested Work Units

| Unit | Goal | Phases | Focused test command | Rollback boundary |
|------|------|--------|----------------------|-------------------|
| 1 | Core model + player | 1-3 | `BerlinTests.exe --category=Berlin` | core/Step.h, playback/ |
| 2 | Export + mutation pins | 4-5 | same | export/ |
| 3 | Edit, processor, bridge | 6-7 | same | generation/StepEdit, plugin/, bridge/ |
| 4 | React | 8 | `pnpm test` in `ui/` | ui/src only |

## Phase 0: Baseline
- [x] 0.1 Build `Tests/Builds/VisualStudio2026/BerlinTests.sln` (Debug|x64) and run `Tests/Builds/VisualStudio2026/x64/Debug/ConsoleApp/BerlinTests.exe --category=Berlin`; record count (expected 522, on top of the uncommitted step-velocity work). Record `git status --short` as the reference (read-only).
- [x] 0.2 `pnpm test` and `pnpm tsc` in `ui/`; record count (expected 567).

## Phase 1: Core model (sequencing-core)
- [x] 1.1 RED `StepTests`: `gatePercent` defaults to 100; `==` sees it; aggregate `Step{note,active}`, `{note,active,skipped}` and `{note,active,skipped,velocity}` still compile with gate 100; five-field aggregate reads back gate 25; static_assert `isValidGatePercent` at -1/0/1/50/100/101; constants 100/1/100.
- [x] 1.2 GREEN `Source/core/Step.h`: `kDefaultGatePercent` (100), `kMinGatePercent` (1), `kMaxGatePercent` (100), `constexpr bool isValidGatePercent (int)`, `Step::gatePercent` declared last, `operator==` compares it. No change to `StepEvent.h`.

## Phase 2: Player (step-event-scheduling: Gate-Scaled Note-Off)
All player tests use 96 kHz, 60 BPM, 4 steps per beat (sps = 24000) and compare the full `StepEvent` vector.
- [x] 2.1 RED `SequencePlayerTests` gate stream: two active steps gate 50 in one 48000 block give `[on@0, off@12000, on@24000, off@36000]`; levels 25/50/75/100 give lengths 6000/12000/18000/24000.
- [x] 2.2 RED `SequencePlayerTests` block independence: same stream at block sizes 1/64/511/4096 yields identical absolute times, non-decreasing offsets per block (cut order 1: one parameterized loop); an off landing exactly on a block end appears at offset 0 of the next block; gate 75 off in a later block emitted once at the right offset.
- [x] 2.3 RED `SequencePlayerTests` rounding: sps 60 with gate 1 gives off at on+1 (never at the on sample); gate 99 rounding onto the boundary gives exactly one off (D3 strict less-than).
- [x] 2.4 RED `SequencePlayerTests` rest and skipped steps with gate 25 produce the same events as gate 100; gate-100 stream equals the legacy expectation (bit-identity, never cut); event buffer capacity not newly exceeded.
- [x] 2.5 RED `SequencePlayerTests` tempo: slower mid-gate keeps the absolute off sample; faster so the boundary comes first gives exactly one off at that boundary, before the next on, none lost or duplicated.
- [x] 2.6 RED `SequencePlayerHandoffTests`: keepPosition gate-only edit on the sounding step emits nothing at adopt, old off time holds, next pass uses the new gate (D11 pin); keepPosition gate plus note change still emits note-off at offset 0; keepPosition note change after the gate off already fired emits no extra off; full swap and adopted stop before the gate off emit one off at 0 and none later.
- [x] 2.7 RED stop/flush/reset (SequencePlayer stop tests): a direct `stop()` then `process` emits no gate off (D4), `flushPendingNoteOff` still emits exactly one off if the note is still sounding and none if the gate already fired; after `prepare()`/`reset()` the note falls back to the legacy off at the first boundary (D5).
- [x] 2.8 GREEN `SequencePlayer.{h,cpp}`: file-local `kNoGateOff = LLONG_MAX`, member `long long pendingOffAbs { kNoGateOff }`, `gateOffAbs (onAbs, sps, g)` helper (`g >= kMaxGatePercent` returns sentinel, else `onAbs + max (1LL, llround (sps * g / kMaxGatePercent))`), gate off before the boundary when `pendingOffAbs < boundaryAbs`, end-of-block gate off only while `transport.isRunning()`, set `pendingOffAbs` at every note-on, reset it in `prepare()` and `reset()`. No allocation, no lock, no new event for gate 100.

## Phase 3: Mutation pins (sequence-mutation, no production change)
- [x] 3.1 `MutationEngineTests` (merge into one test if needed): generated/mutated steps have gate 100; `addNote` keeps the target step's gate; `removeNote`, reverse, rotate and stretch carry it; goldens and ReproducibilityTests pass unmodified. Expected to pass at once; if any fails, fix `MutationEngine.cpp` (gate must consume no random draw).

## Phase 4: Export (midi-export-timeline)
- [x] 4.1 RED `MidiExportTimelineTests`: tps 240 with gates 25/50/75/100 gives off 60/120/180/240 after the on; gate 1 with tps 10 gives off at +1; tps 1 clamp gives 1; full list stays non-decreasing with mixed gates across repeats and note-off before note-on at shared ticks.
- [x] 4.2 RED `MidiExportTimelineTests`: last non-skipped step with gate 50 has a single off inside the step and no terminal off, `endTick` and grid length unchanged; every on has exactly one matching off; skipped and inactive steps with gate 25 emit nothing and leave other ticks unchanged.
- [x] 4.3 RED `MidiExportTimelineTests` parity (never cut): at 96 kHz, 60 BPM, 4 spb (sps 24000, tps 240), `llround (samples * tps / sps) == ticks` for g = 1..99; gate-100 timeline and written file bytes identical to before (existing export suites unchanged).
- [x] 4.4 GREEN `MidiExportTimeline.{h,cpp}`: for gate < 100 push the off right after the on at `tick + clamp ((gate * tps + kMaxGatePercent / 2) / kMaxGatePercent, 1, tps)` and clear `pendingNote`; gate 100 keeps the pending/terminal path; update the header comment. `MidiExportEvent` and `MidiFileWriter` untouched.

## Phase 5: Edit validation (step-editing)
- [x] 5.1 RED `StepEditTests`: gate 0/101/-5 -> `gateOutOfRange`; 1/100 ok; nullopt keeps the current gate; muted/skipped round trip keeps gate 25; index, note and velocity each precede gate; only the target step changes.
- [x] 5.2 GREEN `StepEdit.{h,cpp}`: `StepEditError::gateOutOfRange`, trailing `std::optional<int> gatePercent = {}` on `validateStepEdit` and `editStep`; order index, note, velocity, gate; existing call sites untouched.

## Phase 6: Processor (step-editing, step-event-scheduling end to end)
- [x] 6.1 RED `BerlinAudioProcessorTests`: absent gate keeps; present sets it; out of range rejected with sequence and `edited` unchanged; validation precedes busy (publish pending and restore pending); regenerate/randomize/loadPreset reset gate to 100.
- [x] 6.2 RED `BerlinAudioProcessorTests`: an edited gate-50 note-off reaches `processBlock` MIDI at the expected sample; `exportMidiTo` places the off at the expected tick (cut order 4: first off only); existing export tests unchanged.
- [x] 6.3 GREEN `BerlinAudioProcessor.{h,cpp}`: `editStep(..., optional<int> gatePercent = {})` with field-by-field assignment, `StepEditResult::gateOutOfRange`.

## Phase 7: Bridge (ui-bridge)
- [x] 7.1 RED `UiBridgeTests`: `setStep gatePercent:50` appears in the snapshot; absent keeps; 0 and 101 -> `gate-out-of-range` with state unchanged and no `snapshot` key; `"x"` and 1.5 -> `invalid type: gatePercent`; Infinity -> `non-finite: gatePercent`; validation order against step-out-of-range, note-out-of-scale, velocity-out-of-range and busy; `encodeSteps` emits all 5 keys, generated steps report 100; `kCommands` still 16.
- [x] 7.2 GREEN `UiBridge.cpp`: reuse `readOptionalInt` for `gatePercent`; map `gateOutOfRange` -> `"gate-out-of-range"`; `encodeSteps` adds `gatePercent`.

## Phase 8: React (web-ui-controls, web-ui-layout)
- [x] 8.1 RED Vitest `ui/src/lib/dynamics.test.ts`: `nearestLevel`/`nextLevel` over both tables; velocity cases unchanged and green; gate 1/37/38/62/63/87/88/100 -> staccato/staccato/short/short/long/long/full/full; 60 -> short(50), 63 -> long(75); full wraps to staccato; off-level 90 -> next staccato (25), 38 -> next long (75).
- [x] 8.2 RED Vitest mock/protocol/store: mock steps carry `gatePercent` (default 100), keep when absent, reject 0/101 with `gate-out-of-range`, validation order velocity before gate (cut order 3: may merge into StepView tests); `sendStep` sends all five absolute values; a gate cycle and velocity cycle on one step coalesce into one `setStep`; `gate-out-of-range` shows a mapped status message and reverts; type test: `setStep` omitting `gatePercent` compiles; `GATE` limits 1/100/default 100.
- [x] 8.3 RED Vitest `StepView`: `.step-gate` is a sibling of the cell, in `step-controls` order velocity, gate, skip; `aria-label="Gate step N: <name>"` contains the level; `data-gate-length` on the `.step` cell (not `data-gate`); roving tabindex with one tabbable control; click from full sends `setStep{...,gatePercent:25}` keeping note/active/skipped/velocity; four clicks cycle 25/50/75/100/25; Enter and Space operate it; `active` and `data-gate` unchanged; 40 steps still wrap 16/16/8.
- [x] 8.4 GREEN `ui/src/lib/dynamics.ts`: generalize to `Level<N>`, `nearestLevel (levels, value)`, `nextLevel (levels, value)`, add `GATE_LEVELS`; keep velocity wrappers/exports. `bridge/limits.ts`: `DEFAULT_VELOCITY`, `DEFAULT_GATE`, `GATE = {min:1,max:100}`; `bridge/protocol.ts` (`gatePercent` on steps and `setStep.args?`, `gate-out-of-range` token); `bridge/mock.ts`; `store/store.ts` lane type and error message. No literal 100 in StepView or the mock seed. `store/actions.ts` needs no change (`StepState` is derived); touch only if the type check demands it.
- [x] 8.5 GREEN `components/StepView.tsx` (`gatePercent` prop, `.step-gate` button with `<span class="gate-bar" aria-hidden>`, `data-gate-length`) and `styles/components.css` (`.step-controls { grid-template-columns: 1fr 1fr }`, `.step-skip { grid-column: 1 / -1 }`, gate-bar width per level, 44 px target treatment kept as the sibling buttons do).

## Phase 9: Build and test
- [x] 9.1 MSBuild (via vswhere) `Builds/VisualStudio2026/Berlin.sln` and `Tests/Builds/VisualStudio2026/BerlinTests.sln` Debug|x64; run `BerlinTests.exe --category=Berlin`; 0 failures, count >= 522 plus new tests; goldens, ReproducibilityTests, export and synth bit-identity suites unchanged.
- [x] 9.2 `pnpm tsc` and `pnpm test` in `ui/`; 0 failures, count >= 567 plus new tests.
- [x] 9.3 Read-only `git diff --stat` against the working tree (gate edits are unstaged; compare with `git diff --cached --stat` to confirm the staged velocity snapshot is untouched); record the gate-only line count vs the 800 budget and apply the cut order if over.

## Phase 10: Manual [M] (user)
- [x] 10.1 Regenerate the web assets first (rebuild the UI so `Source/ui/generated` web assets are refreshed from `ui/`), then rebuild and launch standalone (and plugin).
- [x] 10.2 Listening: edit a pattern with staccato/short/long/full; notes are audibly shorter inside the step at the default tempo; full gate sounds as before; a gate-only edit while playing causes no retrigger and takes effect on the next pass.
- [x] 10.3 External MIDI monitor: note-offs arrive early for gate < 100 and exactly one per note; none stuck after stop; tempo change mid-gate loses no off.
- [x] 10.4 Export a file and open it in a DAW: note lengths match the gate levels, last short note has no extra off, default pattern exports as before.
- [x] 10.5 Layout: 16 steps in one row at the minimum supported window width: velocity and gate side by side, skip spanning below, nothing clipped, overlapped or overflowing; the four gate levels are distinguishable by bar width; the 44 px target requirement is honored or any shortfall is noted.

## Phase 11: Archive (separate)
- [x] 11.1 Archive order is mandatory: archive `step-velocity` first (it creates the capability baseline that the gate deltas modify), then `step-gate`. Gate deltas reference the pending velocity text (Purpose lines, Step Value Type, setStep, grid requirements).
- [x] 11.2 `sdd-verify`, then `sdd-archive` for `step-gate`; apply the Purpose-line archive notes (sequencing-core and step-editing); diff the archive output (folder-duplication and dropped-file risk). Archive/commit actions belong to the orchestrator and user, not the apply phase.
