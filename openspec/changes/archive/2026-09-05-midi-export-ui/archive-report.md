# Archive Report: MIDI Export UI (roadmap Phase 7)

**Date**: 2026-09-05  
**Change**: midi-export-ui  
**Roadmap Phase**: 7 — Export UI  
**Status**: ARCHIVED AND CLOSED  

## Executive Summary

The MIDI Export UI feature (Phase 7) has been fully implemented, verified, and archived. All 20 implementation tasks are complete, including the manual DAW-import gate (task 6.5), confirmed by the user on 2026-09-05. The change introduced one new domain spec (`midi-export-trigger`), added the first interactive UI controls to `MainComponent` (`TextButton` and `Label`), established the "Berlin UI Pattern v1" precedent for all subsequent Phase 7 UI work, and removed the temporary Phase 6 export hook. The implementation is confined to two files (`Source/MainComponent.h` and `.cpp`), with zero changes to `Source/export/*`, audio tiers, or test infrastructure.

## Artifacts Merged

### New Specs Created

| Domain | Location | Status |
|--------|----------|--------|
| `midi-export-trigger` | `openspec/specs/midi-export-trigger/spec.md` | Created (NEW) |

The spec is a complete capability definition covering user-initiated export: button affordance, async file chooser, cancel semantics, write destination handling, outcome feedback, and structural proof of audio-thread safety (no locks, no allocations on the real-time path).

## Spec Merge Details

### midi-export-trigger (NEW)

6 requirements covering the user-driven export trigger:
1. Export Button Affordance — visible button, laid out in `resized()`, only trigger for export
2. Async FileChooser With Member Lifetime — non-blocking dialog, member `unique_ptr` ownership
3. Cancel Is Not a Failure — silent no-op, no error feedback
4. Chosen Destination Overwrite Behavior — writes to exact user path, replaces existing files
5. Outcome Feedback Mapped From MidiFileWriteResult — visible feedback on success/failure, no log-only silence
6. Export Stays Read-Only and Off the Audio Path — structural proof of no locks/allocations on audio callback

All requirements verified through:
- Codebase review (Decisions 1, 2, 4, 6 documented in design.md and implemented in source)
- Regression test: `BerlinTests.exe --category=Berlin` exits 0 (89 test cases, all passing)
- Human manual gate (task 6.5): app launch no-export, button visible, dialog seeded at `Documents/Berlin/berlin-export.mid`, cancel silent, export to real path with DAW confirmation (120 BPM, 4/4, notes, 4 bars), read-only folder shows dialog, export during playback with no audible glitch

The entire domain is manual-gate-backed, per spec's "Known Coverage Gap" — same as `midi-file-output` (Phase 6) and accepted precedent.

## Artifacts Archived

The entire change folder has been moved to:
```
openspec/changes/archive/2026-09-05-midi-export-ui/
```

Contents of archive:
- `proposal.md` — original proposal with scope, decisions, risks, and success criteria (all checked [x])
- `design.md` — 6 architecture decisions with ownership, technical approach, file changes, and testing strategy
- `tasks.md` — 20 implementation tasks across 6 phases (all [x] complete, including manual gate 6.5)
- `specs/`
  - `midi-export-trigger/spec.md` — new capability spec merged into main

## Task Completion Status

All 20 tasks complete and verified:

**Phase 1: Header Member Additions (`Source/MainComponent.h`)** — 4 tasks
- [x] 1.1 Add UI members `exportButton`, `statusLabel`
- [x] 1.2 Add `exportChooser` member `unique_ptr`
- [x] 1.3 Add private methods `launchExportChooser()`, `exportSequenceTo()`
- [x] 1.4 Rename `resolveExportFile()` → `defaultExportFile()`, drop TEMPORARY comment

**Phase 2: Extracted Export Method + Flag Fix (`Source/MainComponent.cpp`)** — 4 tasks
- [x] 2.1 Rename method definition
- [x] 2.2 Add `describeWriteFailure()` helper, mapping results to user-facing text
- [x] 2.3 Implement `exportSequenceTo()`: build timeline, write file, two-tier feedback
- [x] 2.4 Implement `launchExportChooser()`: member chooser, flags corrected (no `canOverwriteExisting`), async launch with callback

**Phase 3: Ctor/onClick Wiring, Destructor Reset (`Source/MainComponent.cpp`)** — 2 tasks
- [x] 3.1 Add `exportChooser.reset()` first statement in `~MainComponent()`
- [x] 3.2 Add `addAndMakeVisible()`, `onClick` lambda wiring, `setSize()` last in ctor UI section

**Phase 4: `resized()` Layout (`Source/MainComponent.cpp`)** — 2 tasks
- [x] 4.1 Add layout constants (`kMargin = 12, kControlHeight = 28, kButtonWidth = 140`)
- [x] 4.2 Implement `resized()` using `removeFromTop`/`removeFromLeft`

**Phase 5: Ctor Hook Removal (`Source/MainComponent.cpp`)** — 3 tasks
- [x] 5.1 Delete ctor one-shot export block (lines 55-73), including TEMPORARY comments
- [x] 5.2 Confirm unchanged: includes, constants, usage
- [x] 5.3 Grep for TEMPORARY/Phase 7: zero matches remaining

**Phase 6: Verification (Code Review + Manual Gate)** — 5 tasks
- [x] 6.1 Build Berlin standalone (clean compile, 0 errors)
- [x] 6.2 Run `BerlinTests.exe --category=Berlin` (exit 0, 89/89 passing)
- [x] 6.3 Diff `Source/export/*` and `Tests/BerlinTests.jucer` (byte-for-byte unchanged vs. Phase 6 baseline)
- [x] 6.4 Code review: member ownership, dtor reset, no audio-thread interaction, failure feedback, untouched export tier
- [x] 6.5 Manual DAW-import gate: **CONFIRMED 2026-09-05** — app launch no export, button visible/positioned, dialog seeded at Documents/Berlin/berlin-export.mid, cancel silent no-op, export to real path opens in DAW (120 BPM, 4/4, notes, 4 bars verified), export to read-only folder shows error dialog without crash, export during playback has no audible glitch

## Verification Findings

**Verdict: PASS WITH WARNINGS** (from sdd/midi-export-ui/verify-report, obs. id 159)

All 6 requirements of `midi-export-trigger` verified:
- Requirements 1-6: code review compliant, manual DAW-import gate confirms correctness
- 11/11 spec scenarios code-reviewed compliant
- No CRITICAL findings; 0 blockers

**Warnings (pre-accepted, same as Phase 6)**:
1. Task 6.5 (human manual DAW-import gate) was intentionally left `[ ]` during apply, then confirmed during archive (same pattern as `midi-export` phase 3.16 and `midi-output-routing` phase 6.12, both archived PASS WITH WARNINGS).
2. All 11 spec scenarios verified through code review only, with zero automated runtime coverage — a pre-accepted, spec-documented tradeoff (`Known Coverage Gap`), identical to `midi-file-output`'s gap.

None of these are CRITICAL per the skill file's definition. Archive proceeds with these findings noted.

## Source Code Changes Summary

**Committed in `1e64c6f` (feat: add midi export UI):**
- Modified `Source/MainComponent.h` (+3 members, +2 method decls, rename, comment update)
- Modified `Source/MainComponent.cpp` (+94 insertions, -22 deletions; ctor block removed, wiring added, layout added, helper renamed, dtor reset added)
- Unchanged: all files in `Source/export/`, `Source/playback/`, `Source/midi/`, `Source/core/`, `Source/generation/`, `Berlin.jucer`, `Tests/BerlinTests.jucer`

**Committed in `7f367cb` (docs: confirm midi-export manual verification gate (task 3.16)):**
- (Commit from prior Phase 6, included for context)

**Committed in `72b24e6` (docs: archive midi-export change (roadmap Phase 6)):**
- (Commit from prior Phase 6 archive, included for context)

**Archive time (this operation, 2026-09-05):**
- Created `openspec/specs/midi-export-trigger/spec.md` (merged delta spec into main)
- Moved `openspec/changes/midi-export-ui/` → `openspec/changes/archive/2026-09-05-midi-export-ui/`

## Berlin UI Pattern v1 — Established

This change establishes the 8-rule pattern for all remaining Phase 7 UI work (piano roll, parameter controls, generate, randomize, mutate, presets):

1. Controls owned by value as `MainComponent` members (declared after domain members)
2. `setSize()` last statement of ctor UI section
3. Layout only in `resized()` via `removeFromTop`/`removeFromLeft`, never absolute coords
4. Callbacks are `[this]` lambdas on widget `std::function` members, never `Listener` subclasses
5. Async work owned by member `std::unique_ptr`, reset in dtor
6. Domain work in named private methods taking plain value types
7. Two-tier feedback: status label always, native async message box on failure only
8. Audio thread never consulted; UI reads `MainComponent`-owned `const` state

All 8 rules are implemented and verified in this change. Future Phase 7 work should copy this pattern.

## Observation IDs for Traceability

All SDD artifacts recorded in Engram (project: berlin):

| Artifact | Observation ID | Type |
|----------|---|---|
| `sdd/midi-export-ui/proposal` | 154 | architecture |
| `sdd/midi-export-ui/spec` | 155 | architecture |
| `sdd/midi-export-ui/design` | 156 | architecture |
| `sdd/midi-export-ui/tasks` | 157 | architecture |
| `sdd/midi-export-ui/verify-report` | 159 | architecture |
| `sdd/midi-export-ui/archive-report` | (this document) | architecture |

## Dependencies & Precedents

This change depends on:
- `core-sequencing-model` (Phase 1, archived 2026-08-30)
- `playback-transport-clock` (Phase 4, archived 2026-09-02)
- `midi-output-routing` (Phase 5, archived 2026-09-03)
- `midi-export` (Phase 6, archived 2026-09-04) — provides stable `Source/export/` API, `buildMidiExportTimeline`, `MidiFileWriter::writeToFile`

The manual-gate-only `midi-export-trigger` follows the exact precedent of Phase 6's `midi-file-output`: GUI components in `MainComponent` require `juce_gui_basics`, so they are unreachable from the `juce_core`-only test harness and verified via falsifiable manual gates instead of automated tests. This is an accepted tradeoff recorded in `midi-export-trigger`'s "Known Coverage Gap", matching Phases 5-6's pattern.

## Known Limitations Carried Forward

None. All design decisions and acceptance criteria are satisfied. No open gaps except the pre-accepted manual-gate-only coverage model, which is by design (not a defect).

## Rollback & Recovery

If the change must be reverted:
1. `git revert 1e64c6f` — removes UI code
2. `Source/MainComponent.h/.cpp` return to Phase 6 state: export button removed, ctor hook restored, `resized()` empty, `resolveExportFile()` restored
3. App returns to Phase 6 state: export runs once on startup to `Documents/Berlin/berlin-export.mid`; no user-visible export control
4. `openspec/specs/midi-export-trigger/` may be deleted (or kept as a reference for Phase 7 design decisions)
5. All suites remain green — no test-visible code changes in revert

The revert is clean because the feature is purely additive (UI controls only; no changes to domain tiers).

## Next Phase (Phase 8 and Beyond)

The design explicitly establishes the "Berlin UI Pattern v1" (8 rules documented above) as the template for all remaining Phase 7 UI work:
- Piano roll (visual sequencer grid)
- Parameter controls (probability, step-select, velocity)
- Generate, randomize, mutate commands
- Preset/save-restore UI

Each of these will be a separate roadmap change, each following the same pattern: controls as members, callbacks as lambdas, layout in `resized()`, async work in member `unique_ptr`, domain work in named private methods, two-tier feedback.

---

**Change Archived**: 2026-09-05  
**All Tasks**: 20/20 complete  
**Specs Merged**: 1 new  
**Status**: Ready for the next roadmap phase (Phase 8 or Phase 7 continuation).
