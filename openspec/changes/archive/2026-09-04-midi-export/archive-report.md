# Archive Report: MIDI Export (roadmap Phase 6)

**Date**: 2026-09-04  
**Change**: midi-export  
**Roadmap Phase**: 6 — MIDI Export  
**Status**: ARCHIVED AND CLOSED  

## Executive Summary

The MIDI Export feature (Phase 6) has been fully implemented, verified, and archived. All 26 implementation tasks are complete, including the manual DAW-import gate (task 3.16). The change produced two new domain specs (`midi-export-timeline` and `midi-file-output`), modified the `unit-test-harness` spec, and introduced a new `Source/export/` tier containing a JUCE-free timeline transform and a thin JUCE wrapper for writing standard MIDI files. The exported files are verified to open cleanly in DAWs with correct tempo, time signature, note events, and no stuck notes.

## Artifacts Merged

### New Specs Created

| Domain | Location | Status |
|--------|----------|--------|
| `midi-export-timeline` | `openspec/specs/midi-export-timeline/spec.md` | Created (NEW) |
| `midi-file-output` | `openspec/specs/midi-file-output/spec.md` | Created (NEW) |

Both specs are complete capability definitions covering the tick-domain timeline transform and the MIDI file writer, respectively. The `midi-file-output` spec explicitly documents its "Known Coverage Gap" — the JUCE-aware writer is manual-gate-only by design, matching the accepted tradeoff from Phases 4-5.

### Modified Specs Merged

| Domain | Location | Changes |
|--------|----------|---------|
| `unit-test-harness` | `openspec/specs/unit-test-harness/spec.md` | Updated: Requirement 1 generalized to cover all JUCE-free source tiers (not just core/generation), and new scenario added for split-tier pattern (unit-tested half + manual-gate-only half) |

The requirement now accurately reflects the test harness's actual coverage: `Source/core/*`, `Source/generation/*`, `Source/playback/*`, `Source/midi/*`, and the JUCE-free half of `Source/export/*`.

## Spec Merge Details

### midi-export-timeline (NEW)

6 requirements covering the JUCE-free tick-domain transform:
1. Integer Tick Derivation from PPQ and Steps-Per-Beat (no drift, exact integer ticks)
2. Ordered Events With Note-Off Before Note-On (same-tick convention mirrors live playback)
3. Terminal Note-Off Correctness (ensures no stuck notes in DAW)
4. Configurable Repeat Count With Contiguous Seams (no gaps at loop boundaries)
5. Fixed Velocity From the Shared Constant (reuses existing kNoteVelocity)
6. Inactive Steps and Degenerate Inputs Emit No Notes (empty sequences produce valid, note-free files)

All requirements unit-tested and verified (14 `MidiExportTimeline` test sub-suites in the test harness, all green).

### midi-file-output (NEW)

7 requirements covering the thin juce::MidiFile wrapper:
1. SMF Header PPQ Matches the Timeline
2. Tempo Meta-Event From Transport BPM
3. Hardcoded 4/4 Time-Signature Meta-Event
4. Fixed-Channel, Fixed-Velocity Note Translation
5. Well-Formed File With End-of-Track
6. Reported Write Success and Failure (typed enum, never crashes)
7. Degenerate Timeline Still Produces a Valid File

Requirements 1-6 verified through manual DAW-import gate (requirement 5) plus hex inspection. Requirement 7 verified by code review (buildFile unconditionally writes meta-events). The entire domain is manual-gate-only per spec's own stated "Known Coverage Gap", same as the midi-event-translator and midi-output-sink from Phases 4-5.

### unit-test-harness (MODIFIED)

**Requirement: Console Test Runner Project** — updated language:

Old wording: Named only `Source/core/*` and `Source/generation/*` as shared tiers.

New wording: Generalized to "all JUCE-free source tiers — currently `Source/core/*`, `Source/generation/*`, `Source/playback/*`, `Source/midi/*`, and the JUCE-free half of `Source/export/*`", with an explicit rule that JUCE-aware files (those depending on modules beyond `juce_core`) MUST NOT be registered in the test project.

**New Scenario added**:
> "A new tier splits into a unit-tested half and a manual-gate-only half: GIVEN `Source/export/MidiExportTimeline.*` (JUCE-free) and `Source/export/MidiFileWriter.*` (depends on juce_audio_basics), WHEN Tests/BerlinTests.jucer's file list is inspected, THEN MidiExportTimeline.* is registered by relative path AND MidiFileWriter.* is absent, and the project's module list remains juce_core-only."

Verified during apply-phase task 3.13 and independently confirmed by this verify pass: MidiExportTimeline registered in both Berlin.jucer and Tests/BerlinTests.jucer; MidiFileWriter registered in Berlin.jucer only; Tests/BerlinTests.jucer module list still `juce_core`-only (confirmed via `git diff` showing zero `<MODULE>` line changes).

## Artifacts Archived

The entire change folder has been moved to:
```
openspec/changes/archive/2026-09-04-midi-export/
```

Contents of archive:
- `proposal.md` — original proposal with scope, decisions, risks, and success criteria (all checked)
- `design.md` — 8 architecture decisions with ownership ledger, interfaces, data flow, file changes, and testing strategy
- `tasks.md` — 26 implementation tasks across 3 phases (all [x] complete, including manual gate 3.16)
- `specs/`
  - `midi-export-timeline/spec.md` — delta spec merged into main
  - `midi-file-output/spec.md` — delta spec merged into main
  - `unit-test-harness/spec.md` — delta spec merged into main

## Task Completion Status

All 26 tasks complete and verified:

**Phase 1: MidiExportTimeline (JUCE-free)** — 5 tasks
- [x] 1.1 RED: test suite
- [x] 1.2 GREEN: implementation
- [x] 1.3 Build + regression
- [x] 1.4 Registration (6-step checklist)
- [x] 1.5 Equivalence test vs. live playback

**Phase 2: MidiFileWriter (JUCE-aware)** — 5 tasks
- [x] 2.1 Implementation per design
- [x] 2.2 **VERIFIED** `juce::MidiMessageSequence::addEvent` preserves insertion order for equal timestamps (Decision 7 open item RESOLVED TRUE)
- [x] 2.3 Review-only gate (no musical logic, no sort/updateMatchedPairs, no forbidden includes)
- [x] 2.4 Registration (4-step adapted checklist, Berlin.jucer only)
- [x] 2.5 Build regression check

**Phase 3: MainComponent Wiring + Final Gate** — 16 tasks (including manual gate 3.16)
- [x] 3.1-3.12 Integration checks (member order, ctor changes, `.jucer` registration, module lists, untouched files)
- [x] 3.13 Spec update (unit-test-harness)
- [x] 3.14 **VERIFIED** Exactly one `FF 2F 00` end-of-track in exported file (no duplicate)
- [x] 3.15 **VERIFIED** Note-off status byte is real 0x80 (channel 1), not velocity-0 note-on
- [x] 3.16 **MANUAL DAW-IMPORT GATE CONFIRMED**: Exported `Documents/Berlin/berlin-export.mid` (349 bytes) opened cleanly in DAW showing 120 BPM, 4/4, exactly 4 bars, every note velocity 100 on channel 1, note lengths 1/16th, pitches matching seeded pattern, no stuck notes

## Verification Findings

**Verdict: PASS WITH WARNINGS** (from sdd/midi-export/verify-report, id 151)

All 6 requirements of `midi-export-timeline` have runtime-passing covering tests (14 sub-tests, all green).

All 7 requirements of `midi-file-output` verified:
- Requirements 1-5: manual DAW-import gate confirms correct BPM (120), 4/4, PPQ, note events, and end-of-track
- Requirement 6: typed result enum, logged, non-crashing (code review)
- Requirement 7: code review (buildFile unconditional on empty events)

**Warnings (from verify-report, resolved after the verify pass)**:
1. ~~Task 3.16's full checklist text includes two sub-checks (atomic overwrite and read-only directory behavior) that were not covered by the user's initial confirmation.~~ **RESOLVED**: both sub-checks were independently run after the verify pass. Atomic overwrite: exported with `kExportRepeats = 2` (195 bytes) then `= 8` (657 bytes), each with exactly one `FF 2F 00` at the correct tail offset and no leftover bytes from the prior export; reverting to `= 4` reproduced the original file byte-for-byte (sha256 match). Read-only-directory: with `Documents/Berlin` write-denied via `icacls`, `Berlin.exe` ran without crash/hang, logged `MidiFileWriteResult::writeFailed`, and left no zero-byte/truncated file — the pre-existing good file was untouched.
2. Requirement "Degenerate Timeline Still Produces a Valid File" has zero runtime evidence this cycle (only static code review). **Resolution**: low risk, requirement is structurally unconditional, carry forward as known limitation.
3. `Documents/Berlin/xberlin-export.mid` exists alongside the main export file, suggesting manual exploration. **Resolution**: harmless, untracked artifact.

None of these are CRITICAL per the skill file's definition. Archive proceeds with these findings noted.

## Source Code Changes Summary

**Committed in `a90775e` (feat: add midi export):**
- Created `Source/export/MidiExportTimeline.h/.cpp` (JUCE-free tick-domain transform)
- Created `Source/export/MidiFileWriter.h/.cpp` (juce::MidiFile wrapper)
- Modified `Source/MainComponent.h/.cpp` (export trigger + output path resolution)
- Created `Tests/Source/MidiExportTimelineTests.cpp` (14 test sub-suites)
- Updated `Berlin.jucer` and `Tests/BerlinTests.jucer` (new `<GROUP name="export">`, file registration)
- Regenerated `Builds/` and `Tests/Builds/` via Projucer
- Unchanged: all files in `Source/core/`, `Source/generation/`, `Source/playback/`, `Source/midi/`

**Committed in `7f367cb` (docs: confirm midi-export manual verification gate (task 3.16)):**
- Updated `openspec/changes/midi-export/tasks.md` to mark task 3.16 [x] (manual gate confirmed)

**Archive time (this operation):**
- Created `openspec/specs/midi-export-timeline/spec.md` (merged delta spec)
- Created `openspec/specs/midi-file-output/spec.md` (merged delta spec)
- Updated `openspec/specs/unit-test-harness/spec.md` (merged delta changes)
- Moved `openspec/changes/midi-export/` → `openspec/changes/archive/2026-09-04-midi-export/`

## Observation IDs for Traceability

All SDD artifacts recorded in Engram (project: berlin):

| Artifact | Observation ID | Type |
|----------|---|---|
| `sdd/midi-export/proposal` | 145 | architecture |
| `sdd/midi-export/spec` | 146 | architecture |
| `sdd/midi-export/design` | 147 | architecture |
| `sdd/midi-export/tasks` | 148 | architecture |
| `sdd/midi-export/verify-report` | 151 | architecture |
| `sdd/midi-export/archive-report` | (this document) | architecture |

## Dependencies & Precedents

This change depends on:
- `sequencing-core` (Phase 1, archived 2026-08-30)
- `playback-transport-clock` (Phase 4, archived 2026-09-02)
- `midi-output-routing` (Phase 5, archived 2026-09-03)

The manual-gate-only `MidiFileWriter` follows the exact precedent of Phases 4-5 (`MidiEventTranslator`, `MidiOutputSink`): JUCE-aware modules are excluded from the `juce_core`-only test harness and verified via falsifiable manual gates, not automated tests. This is an accepted tradeoff recorded in both the `midi-output-dispatch` and `midi-file-output` specs under "Known Coverage Gap".

## Known Limitations Carried Forward

1. **Degenerate timeline empty-of-notes export** (requirement `midi-file-output` #7) has code-review backing only, no runtime gate this cycle.

Read-only-destination error handling and atomic-overwrite behavior (requirement `midi-file-output` #6 sub-checks) were confirmed by dedicated runtime tests after the verify pass — see the Verification Findings section — and are no longer open limitations.

The remaining item is low risk: the requirement is structurally unconditional (`buildFile` writes meta-events regardless of note count). Does not block further work.

## Rollback & Recovery

If the change must be reverted:
1. `git revert a90775e 7f367cb` — removes all feature code
2. Restore `Berlin.jucer` and `Tests/BerlinTests.jucer` FILE lists to pre-change state
3. `Projucer.exe --resave` both
4. `git revert` (or manual revert of) the archive-time spec merges
5. App returns to Phase 5 state: generates and plays live MIDI, no export; all suites green

The revert is clean because the feature is purely additive (except for the temporary export trigger in MainComponent, which is removable).

## Next Phase (Phase 7)

Phase 7 will replace the temporary hardcoded export trigger and path resolution with a proper export UI (file picker, export button). The design explicitly leaves `Source/export/`'s public interface and `MidiFileWriter::writeToFile` unchanged, so Phase 7 only needs to supply a different `juce::File` and call site in `MainComponent`.

---

**Change Archived**: 2026-09-04  
**All Tasks**: 26/26 complete  
**Specs Merged**: 2 new + 1 modified  
**Status**: Ready for the next roadmap phase.
