# Verify Report: step-gate

Mode: Strict TDD. Verdict: PASS WITH WARNINGS (0 CRITICAL, 4 WARNING, 4 SUGGESTION). Ready for archive after the user decides on size and stages the new helper.

## Execution (re-run by verify, not trusted from apply)

| Check | Result |
|---|---|
| BerlinTests.sln Debug x64 | builds; GateStreamSupport.h resolved via same-directory include |
| `BerlinTests.exe --category=Berlin` | 558 tests, "All tests completed successfully", 0 failures (baseline 522, +36) |
| Berlin.sln Debug x64 | builds (Berlin.exe linked) |
| `pnpm test` (ui) | 583 passed / 40 files (baseline 567, +16) |
| `pnpm tsc` (ui) | exit 0 |
| Bit-identity guards | `git diff` empty for Reproducibility/Golden tests, MutationEngine.cpp, MidiFileWriter.cpp, MidiEventTranslator, synth, StepEvent.h |

## Scenario to test map (all passing)

- Step core: StepTests (defaults/aggregate, equality, isValidGatePercent). Generated steps gate 100 + mutation carry: MutationEngineTests "gate: generated steps default to 100...".
- Player stream: "gate 50 on two active steps...", "gate levels 25/50/75 -> 6000/12000/18000", block sizes 1/64/511/4096 (drain() flags any decreasing or out-of-range offset, so non-decreasing ordering is asserted on the full event vector), block-end off at offset 0 plus later-block off once, 1-sample gate and gate-99 boundary (exactly one off), rest/skipped ignore gate + gate-100 equals legacy stream.
- Tempo mid-gate: slower keeps absolute off; faster gives exactly one off at the boundary before the next on.
- keepPosition: gate-only edit (no adopt event, old off holds, new gate next pass), gate+note change off at 0, note change after fired off no extra off; full swap and adopted stop emit one off at 0.
- Direct stop() vs flush: "direct stop() fires no gate off, flush emits exactly one", "flush emits nothing once gate off fired"; prepare()/reset() fallback test.
- Export: 25/50/75/100 -> 60/120/180/240; 1-tick and tps-1 clamp; mixed gates non-decreasing, off before shared-tick on; short last step no terminal off, grid unchanged; skipped/inactive; parity g=1..99 at 24000 samples / 240 ticks against the real player; processor export tick 120 and processBlock off at sample 3000.
- Validation order: StepEdit (index, note, velocity before gate), Processor (gate before busy incl. pending restore), UiBridge (step, note, velocity, gate, busy asserted in sequence); type/non-finite errors; 5 keys encoded; kCommands 16 test retained.
- React: dynamics (both tables, ties, wrap, off-table), mock (default/keep/0/101/order), protocol type test, limits, store coalesce + mapped revert, StepView (`.step-gate` sibling in order, aria-label, `data-gate-length` on `.step`, 25/50/75/100 cycle, roving tabindex, 16/16/8 wrap unchanged).
- Audio-thread safety: SequencePlayer diff adds one long long member, a noexcept free function, no allocation, lock or container; MidiExportTimeline is offline.

## Findings

CRITICAL: none.

WARNING
1. Size: 1303 insertions + 119 deletions (34 tracked files) + 55-line untracked helper = about 1477 authored lines vs the 800 budget (forecast 530). Production is only about 231 lines; tests about 1250, mostly never-cut items. Decision belongs to the user: size exception, or split PR A (C++ engine + bridge) / PR B (React) per tasks.md (A still about 1020).
2. `Tests/Source/GateStreamSupport.h` is untracked and not in the .jucer. The build works (same-directory quoted include, header-only, anonymous namespace), but it MUST be staged with the change or a clean checkout will not compile three test files. Consider adding it to the .jucer for IDE visibility.
3. Spec "Three buttons per cell" / 2-column `step-controls` grid and `.gate-bar` widths are CSS only; no automated test asserts `grid-template-columns` (jsdom has no layout). Covered only by manual 10.5 (open). Test asserts DOM order only.
4. "Gate button is operable by keyboard [V]" is satisfied indirectly (native `<button type="button">` asserted); jsdom cannot synthesize click from Enter/Space. Acceptable, weak evidence.

SUGGESTION
1. No gate-specific test for "event buffer capacity unchanged"; only the legacy gate-100 capacity test exists. By construction each note still has at most one off, so risk is low; one test with gate 50 at sps 1.0-ish would close it.
2. D5 `pendingOffAbs` clear in prepare()/reset() is not observable (a surviving note is cut at the boundary at position 0 anyway). Defensive line, behavior pinned; accept.
3. `kNoGateOff` is namespace-scope in SequencePlayer.h (design said file-local); needed by the member initializer. Accept; a `static constexpr` class member would avoid namespace exposure.
4. `nearestLevel/nextLevel (levels, value)`: follows D8; contradicts the task 8.4 phrase "keep velocity wrappers" but all call sites/tests updated and expectations unchanged. Accept. The new `gate-out-of-range` status text is required by task 8.2 ("mapped status message"); `velocity-out-of-range` still unmapped (pre-existing).

## TDD compliance
Apply-progress has a TDD cycle table covering every phase. Deviation 1 (Step.h GREEN before the RED run; compile-failing by construction) is disclosed and acceptable. RED runs recorded for the player, export, StepEdit, processor, bridge and UI layers.

## Manual (open, user)
Phase 10 (regenerate web assets, listening, MIDI monitor, DAW export, 16-step layout at minimum width, 44 px targets) and Phase 11 archive order (step-velocity first, then step-gate) remain.
