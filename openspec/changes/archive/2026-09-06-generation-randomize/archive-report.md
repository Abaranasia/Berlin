# Archive Report: Generation / Randomize (roadmap Phase 10)

**Date**: 2026-09-06  
**Change**: generation-randomize  
**Status**: Archived and closed

## Executive Summary

The generation-randomize Phase 10 change has been successfully completed, verified, and archived. All 28 implementation tasks passed, spec merges are complete (4 delta specs merged into base specs), and the verify gate passed with a PASS WITH WARNINGS verdict (0 critical, 1 format-reporting warning only).

## Artifacts Archived

All change artifacts have been copied to: `openspec/changes/archive/2026-09-06-generation-randomize/`

**Core artifacts**:
- `proposal.md` — Phase intent, scope, semantics, approach, open decisions, risks
- `exploration.md` — Current state analysis, affected areas, approaches, recommendation
- `design.md` — Verified findings, 5 architecture decisions, data flow, file changes, testing strategy
- `tasks.md` — Work phases (6 phases: Sequence::swap, SkipMaskGenerator, SequencePlayer handoff, MainComponent wiring, manual gate, cleanup), all 28/28 complete
- `verify-report.md` — PASS WITH WARNINGS: 24/24 spec scenarios compliant, 143/143 tests green, all 5 design decisions confirmed

**Spec delta artifacts** (4 total, merged into base specs):
- `specs/realtime-audio-wiring/spec.md` — MODIFIED requirement: "Replaceable Only Via Published Handoff"
- `specs/deterministic-generation/spec.md` — ADDED: Production composition, user-editable seed, Randomize/Lock Seed, SkipMaskGenerator
- `specs/sequencing-core/spec.md` — ADDED: Sequence::swap O(1) contract
- `specs/generation-live-control/spec.md` — NEW spec file: Generate/Randomize/Lock Seed UI and audio-thread-safe handoff

## What Shipped

This phase delivered the first live-regeneration capability for the Berlin sequencer app (roadmap Phase 10):

1. **Generation UI** — Generate button, Randomize button, seed text field (editable), Lock Seed toggle, placed in header block per Decision 5
2. **SequencePlayer handoff** — Audio-thread-safe publish-into-staging-slot, adopt-by-swap mechanism (Decision 1); no allocation/lock/logging on audio thread
3. **SkipMaskGenerator** — New deterministic skip-mask rhythm generator (Decision 2/3); produces exact active-step count from full pulse train minus anchored-step-0 Fisher-Yates draws; produces 11-step displacement from 16-step grid per research
4. **Sequence::swap** — O(1) constant-time buffer exchange for audio-thread-safe adoption (Decision 4)
5. **Export freshness fix** — Regeneration correctly updates both player AND export source (MainComponent::currentSequence); Export always reflects current pattern

## Architecture Decisions (5 total)

| Decision | Rationale | Impact |
|----------|-----------|--------|
| Decision 1: publish-into-staging / adopt-by-swap in `process()` | Naive stop-then-swap races with live reads; this design keeps playback running, publishes into separate staging slot, audio thread adopts atomically at block top (acquire-load, note-off-first, sequence.swap, reset, release-store) | Relax `const` on `SequencePlayer::sequence`; accept brief window of `publishSequence` returning false (unreachable at human click rate) |
| Decision 2: new SkipMaskGenerator, RhythmGenerator stays untouched zero-production-callers | Mask produces guaranteed exact active-step count; RhythmGenerator's per-step probability is different contract; reusing RhythmGenerator would supersede its existing spec requirement | New file `SkipMaskGenerator.h/.cpp`; `RhythmGenerator` retained as spec'd/tested, no production call site |
| Decision 3: full pulse train, step-0-anchored scratch, partial Fisher-Yates over numSkips | Source: MIDIbox research; displacement pattern from full train (not independent coins) feels rhythmic; step 0 anchor gives odd-count a stable phase; exactly `numSkips` RNG draws for reproducibility | Algorithm: initialize all active, Fisher-Yates shuffle over [1..numSteps-1] scratch indices to select which steps to skip |
| Decision 4: three controls + seed field, message-thread-only seed state | Fewer UI controls (Randomize/New Seed collapse to one button); seed as plain `juce::int64`, not atomic (message-thread only); Berlin UI Pattern v1 seam via `player.publishSequence` | Simple, fits layout budget (96px of 180px spare); no cross-thread seed scalar needed |
| Decision 5: GENERATION section in header block, no window resize | Fits 96px into 180px spare header budget; belongs with Export/status/toggles (global actions); left column has more slack but would visually file Generateunder oscillator controls | Placed after toggle row, before two-column split; setSize(800, 600) unchanged |

## Spec Merge Summary

**Task 6.5 (already complete during apply)**:  
All four delta specs merged into base specs per openspec-convention.md:

| Spec | Action | Details |
|------|--------|---------|
| `realtime-audio-wiring` | MODIFIED | Req: "Sequence Built Before Audio Starts, Replaceable Only Via Published Handoff" — replaces prior "never modified afterward" |
| `deterministic-generation` | MODIFIED | ADDED: Production composition, user-editable seed, Randomize/Lock Seed, SkipMaskGenerator requirements |
| `sequencing-core` | MODIFIED | ADDED: Sequence::swap O(1) requirement |
| `generation-live-control` | NEW | Created at `openspec/specs/generation-live-control/spec.md` (was nonexistent before this change) |
| `step-event-scheduling` | NO DELTA | Correctly left untouched — swap-edge note-off lives inline in process(), not as second flushPendingNoteOff call site |

**Reconciliation note** (from sdd-spec memory #189):  
Orchestrator found 4 real spec/design divergences during parallel phase runs and fixed all on disk:
1. Spec had incorrect `step-event-scheduling` delta (not needed) — deleted
2. Spec missing `sequencing-core` delta (Sequence::swap) — added
3. Spec had stale RhythmGenerator references in multiple files — corrected to SkipMaskGenerator
4. Spec prose still incorrectly claimed no production composition — updated to match design/code

All fixes verified correct during verify phase via source inspection.

## Task Completion (28/28, 100%)

**Phase 1**: Sequence::swap — RED/GREEN ✓ (1.1-1.3)  
**Phase 2**: SkipMaskGenerator — RED/GREEN ✓ (2.1-2.6, 8 test cases, end-to-end golden)  
**Phase 3**: SequencePlayer handoff — RED/GREEN ✓ (3.1-3.7, 6 test cases, RT-safety review)  
**Phase 4**: MainComponent wiring — Implementation ✓ (4.1-4.6, generation UI, regenerate(), Export fix)  
**Phase 5**: Manual gate — Human-only ✓ (5.1-5.5, internal synth + external MIDI Focusrite hardware via loopMIDI, seed round-trip, Export freshness)  
**Phase 6**: Cleanup — ✓ (6.1-6.5, full test suite 143/143 green, untouched tiers verified, spec merge)

## Verify Verdict

**PASS WITH WARNINGS** (from verify-report.md, Engram id 193):

| Metric | Result |
|--------|--------|
| Blockers | 0 |
| Critical findings | 0 |
| Requirements compliant | 14/14 (all 4 delta specs) |
| Spec scenarios compliant | 24/24 (all GIVEN/WHEN/THEN verified) |
| Test count | 143/143 green (126 baseline + 17 new) |
| Design decisions | 5/5 confirmed matching code exactly |
| RT-safety | Confirmed: no alloc/lock/logging on audio-thread adoption path |
| TDD compliance | 5/6 checks full, 1 partial (reporting-format gap only, not substantive) |

**Warnings (non-blocking)**:
1. apply-progress artifact (Engram) lacks formatted RED/GREEN/TRIANGULATE/etc. TDD evidence table; equivalent evidence exists in tasks.md and was independently verified
2. MIDI device-selection gap (pre-existing since Phase 5, left out of scope, documented, scheduled for future phase)

## Source Changes Summary

**Changed lines (estimate)**: 550-750 (size-exception single-PR strategy, 800-line authorized budget)

**New files**: 2  
- `Source/generation/SkipMaskGenerator.h` / `.cpp`
- `Tests/Source/SkipMaskGeneratorTests.cpp`, `SequencePlayerHandoffTests.cpp` (test files)

**Modified files**: 6  
- `Source/core/Sequence.h` / `.cpp` (additive: swap method)
- `Source/playback/SequencePlayer.h` / `.cpp` (drop const, add handoff)
- `Source/MainComponent.h` / `.cpp` (regenerate, UI, layout)
- `Tests/Source/SequenceTests.cpp`, `ReproducibilityTests.cpp` (add/modify test scenarios)
- `Berlin.jucer`, `Tests/BerlinTests.jucer` (`<FILE>` registrations)

**Unchanged tiers** (verified git diff --stat zero):
- `Source/synth/*`, `Source/midi/*`, `Source/export/*`
- `Source/generation/RhythmGenerator.*`, `PitchGenerator.*`, `DeterministicRandom.h`
- `Source/playback/Transport.h/.cpp` (explicitly unchanged per Decision 1)

**Commits**: 7 on branch `feat/generation-randomize`, single PR strategy

## Next Phase Candidates

**Roadmap Phase 8+ still open**:
- Piano Roll (UI for step-by-step note editing)
- Preset System (save/load/manage generation + synthesis configurations)

**Explicit backlog items** (noted during this phase):
1. **MIDI device-selection UI gap** — Known since Phase 5; MidiOutputSink::openFirstAvailableDevice() always grabs first enumerated device, no selector UI. Scheduled for future phase once MIDI device selection is explicitly scoped.
2. **TDD evidence format** — apply-progress should include formatted RED/GREEN/TRIANGULATE/SAFETY-NET/REFACTOR table (per Strict TDD Mode), not just phase labels. Minor improvement for future apply phases, not blocking.

## Traceability

**Engram observation IDs** (all artifacts persisted):
- `sdd/generation-randomize/proposal` (id 188)
- `sdd/generation-randomize/spec` (id 189) — reconciled against design
- `sdd/generation-randomize/design` (id 190)
- `sdd/generation-randomize/tasks` (id 191) — 28/28 complete
- `sdd/generation-randomize/apply-progress` (id 192)
- `sdd/generation-randomize/verify-report` (id 193) — PASS WITH WARNINGS
- `sdd/generation-randomize/archive-report` (id TBD) — THIS DOCUMENT

## Status: ARCHIVED AND CLOSED

All phases complete. Specs merged. Tests passing. Ready for production merge.
