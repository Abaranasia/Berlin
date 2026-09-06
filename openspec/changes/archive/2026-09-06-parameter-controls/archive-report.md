# Archive Report: Parameter Controls (roadmap Phase 9)

**Date**: 2026-09-06  
**Change**: parameter-controls  
**Roadmap Phase**: 9 — Parameter Controls (Live Sound Editing)  
**Status**: ARCHIVED AND CLOSED  

## Executive Summary

The Parameter Controls feature (Phase 9 of the Berlin School generative sequencer roadmap) has been fully implemented, verified, and archived. All 25 implementation tasks across 7 phases are complete, verified, and tested. The change delivered live, mid-note user control over every synth parameter (waveform, pulse width, filter cutoff/resonance, ADSR A/D/S/R, LFO destination/rate/depth), enabling real-time sound design while a sequence plays. The implementation modifies one capability spec (`internal-synth-voice` — Purpose statement + 5 requirements), introduces 7 new architecture decisions, and refactors oscillator generation from lookup-table-based to table-free (allocation-free live switching). The verification pass confirms PASS WITH WARNINGS verdict: 0 CRITICAL findings, 2 WARNING-level test-coverage gaps (pulse-width and LFO amplitude/pulseWidth destinations lack automated runtime coverage, mitigated by an explicit, confirmed human manual gate and consistent with Phase 7/8 project conventions). All 126 automated test scenarios pass (up from 116 baseline), and the entire implementation is RT-safe (no allocation, lock, or logging on the audio thread).

## Artifacts Merged

### Modified Capability Specs

| Domain | Location | Status | Changed Requirement | Reason |
|--------|----------|--------|---|---|
| `internal-synth-voice` | `openspec/specs/internal-synth-voice/spec.md` | Modified (DELTA) | **Purpose** | Phase 8's "fixed patch, no parameter UI" replaced with "every parameter live user-adjustable while a note sounds" via message-thread setters and lock-free atomics |
| `internal-synth-voice` | `openspec/specs/internal-synth-voice/spec.md` | Modified (DELTA) | **Four Selectable Oscillator Waveforms** | Table-free generation required for allocation-free live switching; pulse width exposed as dedicated user control |
| `internal-synth-voice` | `openspec/specs/internal-synth-voice/spec.md` | Modified (DELTA) | **Low-Pass Filter With Resonance** | Ranges pinned: cutoff `[20, 20000]` Hz, resonance `[0.7071068, 8.0]`; live adjustment confirmed unconditionally stable, no self-oscillation hazard |
| `internal-synth-voice` | `openspec/specs/internal-synth-voice/spec.md` | Modified (DELTA) | **ADSR-Gated Amplitude** | All four times clamped to explicitly documented ranges; gate `setParameters()` on actual change (correctness fix for mid-release edge); zero-floor ranges reachable as deliberate instant transitions |
| `internal-synth-voice` | `openspec/specs/internal-synth-voice/spec.md` | Modified (DELTA) | **Single LFO With Selectable Destination** | Destination, rate, depth all live-switchable mid-note; re-park latent bug fixed (base-value-then-delta re-apply); LFO rate clamped to `[0.05, 20.0]` Hz, excluding phase reversal |
| `internal-synth-voice` | `openspec/specs/internal-synth-voice/spec.md` | Modified (DELTA) | **Allocation-Free Voice Rendering** | Extended scenario: live parameter application (incl. waveform switch, ADSR change) must allocate nothing, acquire no lock, call no logging |

## Spec Merge Details

### internal-synth-voice (MODIFIED)

Purpose statement and 5 requirements replaced with Phase 9 delta. All numeric bounds spelled out inline (not bracketed placeholders). Two pre-existing requirements carried forward unchanged:
- **Strictly Monophonic, No Voice Pool Or Stealing** — unchanged
- **Silence When Idle** — unchanged

**Key changes**:
1. **Purpose**: "live user-adjustable while a note sounds" — replaces "the patch is fixed for this phase. No parameter UI"
2. **Four Selectable Oscillator Waveforms**: Table-free generation (allocation-free switch); pulse width exposed as dedicated control `[0.05, 0.95]`
3. **Low-Pass Filter With Resonance**: Ranges pinned (`cutoffHz` [20, 20000] Hz; `resonance` [0.7071068, 8.0]); unconditionally stable (verified against JUCE source)
4. **ADSR-Gated Amplitude**: Ranges pinned (attack/decay [0.001, 4.0]s; sustain [0.0, 1.0]; release [0.005, 8.0]s); `setParameters()` gated on actual change (correctness fix for `noteOff()` re-rate edge)
5. **Single LFO With Selectable Destination**: Rate clamped [0.05, 20.0] Hz; base-then-delta re-apply closes Phase 8 latent bug (stale value parked when switching destination mid-note)
6. **Allocation-Free Voice Rendering**: Extended to cover live parameter application path

All requirements verified through:
- Unit tests: `SynthVoiceTests.cpp` (table-free equivalence, live/mid-note, LFO re-park, clamps) + `SynthEngineTests.cpp` (forwarders)
- Code review: `applyParameters()`, `updateLfoModulation()`, generator lambda, atomics and cross-thread seams
- Manual audibility gate: task 6.1 (confirmed by user 2026-09-06) — all controls respond immediately mid-note, no zipper/click, no dropout on waveform switch, LFO destination switch leaves nothing parked, saw/square/triangle audibly equivalent to Phase 8

## Architecture Decisions

| Decision | Outcome | Rationale |
|----------|---------|-----------|
| 1. One generator lambda for all four waveforms, `initialise` called exactly once | Single oscillator.initialise(lambda, 0) in prepare(); waveform switch is plain member assignment; table-free is sharper but audibly equivalent | Switching waveform becomes structurally impossible to allocate on; reuses pulse's proven technique |
| 2. Atomics live on SynthVoice, SynthEngine forwards | Nested `struct Parameters` of 11 atomic members on SynthVoice; 11 one-line forwarders on SynthEngine | State lives where consumed (voice DSP); nested struct keeps cross-thread seam visually distinct; `static_assert (is_always_lock_free)` guards all three atomic types |
| 3. Base-value-then-delta re-apply every control block (closes Phase 8 latent bug) | `updateLfoModulation()` always recomputes all 4 targets from base, adds delta only to active destination | Stateless-by-construction; no edge-miss, no undo logic; costs nothing (values written every block anyway now that they're live) |
| 4. adsr.setParameters() is gated on an actual change | `applyParameters()` compares 4 loaded floats against cached `ADSR::Parameters`, calls setParameters only on real difference | Correctness gate, not performance nicety; `recalculateRates()` rewrites releaseRate, discarding `noteOff()`'s rate — unconditional call mid-release would silently accelerate every release or trip `releaseRate <= 0` → reset → click |
| 5. Ranges, tapers and clamps; pulseWidth gets a slider | Nine ranges with log tapers where UI feel demands it (cutoff, resonance, ADSR times, LFO rate); pulseWidth [0.05, 0.95] with linear taper | Design verified against JUCE 9.0.1 source and measured audibly; pulseWidth slider because pulse at 50% is byte-identical to square (noted explicitly in Phase 8 test) |
| 6. MainComponent re-pushes every parameter after prepare() | `pushAllParametersToSynth()` called at end of prepareToPlay, immediately after synth.prepare(spec) | Device/sample-rate/block-size change re-triggers prepareToPlay; without re-push, sliders show user's values but audio snaps to defaults; UI is source of truth |
| 7. Two columns and section labels inside existing resized() idiom | `placeLabelled` lambda, two-column split, header row unifies synthToggle/fxToggle, parameter area fits in 426 of 588 usable px | No new layout primitive; Berlin UI Pattern v1 preserved; measured to fit with 160 px spare |

## File Changes

| File | Action | Lines Changed | Notes |
|---|---|---|---|
| `Source/synth/SynthPatch.h` | Modify | ~50 | 9 inline constexpr min/max pairs + clampParameter helper |
| `Source/synth/SynthVoice.h` | Modify | ~100 | Nested Parameters struct (11 atomics + 3 static_asserts), 11 setters, applyParameters(), lastAppliedCutoffHz, cached ADSR::Parameters, live waveform member |
| `Source/synth/SynthVoice.cpp` | Modify | ~150 | Single 4-way generator lambda, prepare() seeds + clamps atomics, applyParameters() at head of each control block, updateLfoModulation() base-then-delta rewritten |
| `Source/synth/SynthEngine.h/.cpp` | Modify | ~50 | 11 thin voice.setX(v) forwarders |
| `Source/MainComponent.h` | Modify | ~80 | 10 controls (2 ComboBox, 8 Slider) + 14 Labels (10 name + 4 section), pushAllParametersToSynth() declaration |
| `Source/MainComponent.cpp` | Modify | ~120 | configureSlider lambda, 11 callbacks, two-column resized() with placeLabelled, pushAllParametersToSynth() call |
| `Tests/Source/SynthVoiceTests.cpp` | Modify | ~130 | Table-free equivalence (DFT harmonics), live/mid-note (cutoff, resonance, ADSR), LFO re-park (cutoff-to-amplitude, pitch-to-amplitude), clamp/assert-freedom suites |
| `Tests/Source/SynthEngineTests.cpp` | Modify | ~50 | 11 forwarder coverage scenarios |
| `Berlin.jucer`, `Tests/BerlinTests.jucer`, Builds/, JuceLibraryCode/ | **Untouched** | 0 | No new source/test files → no Projucer regen; diff stays 100% authored |
| `Source/core/`, `generation/`, `playback/`, `midi/`, `export/`, `Lfo.*`, `SynthEffects.*` | Untouched | 0 | Byte-for-byte identical to phase base commit |

**Total authored lines**: ~730 (code + tests + spec delta merged inline)

## Task Completion Status

All 25 tasks complete and independently verified:

**Phase 1: Ranges + Clamp Helper** — 3 tasks [x]
- 9 inline constexpr min/max pairs in SynthPatch.h
- `constexpr clampParameter(float, float, float)` helper
- clampParameter unit test (RED then GREEN)

**Phase 2: Table-Free Generator + Equivalence Test** — 4 tasks [x]
- RED: DFT-harmonics equivalence test (reference vs. table-free, harmonics 1-8, RMS within 3%)
- GREEN: delete applyWaveform(), single 4-way lambda installed, initialise once in prepare()
- GREEN: waveform-switch-mid-note test passes (no dropout, all-finite)
- RT-safety spot check: setWaveform() is single non-atomic member assignment

**Phase 3: SynthVoice Atomics + applyParameters() + LFO Re-Park** — 6 tasks [x]
- RED: live cutoff/resonance/ADSR/clamp/re-park tests (failed to compile pre-3.2)
- GREEN: nested Parameters struct, 11 setters, applyParameters(), lastAppliedCutoffHz guard, cached ADSR::Parameters, live waveform member
- GREEN: applyParameters() implementation (relaxed loads, once per control block)
- GREEN: adsr.setParameters() gated on actual change
- GREEN: updateLfoModulation() rewritten base-then-delta (closes Phase 8 bug)
- Verification: 125/125 scenarios green; no allocation/lock/log in applyParameters()/updateLfoModulation()

**Phase 4: SynthEngine Forwarders** — 2 tasks [x]
- 11 thin voice.setX(v) forwarders (design's 11 setters, task text noted "ten" vs. design's "eleven", resolved in design's favor)
- SynthEngineTests.cpp: each forwarder produces same rendered output as direct voice setter call

**Phase 5: MainComponent UI Wiring** — 5 tasks [x]
- MainComponent.h: 2 ComboBox + 9 Slider + 14 Labels (10 name + 4 section), pushAllParametersToSynth() declaration (task text noted 8 vs. design's 9, resolved in design's favor)
- MainComponent.cpp ctor: configureSlider lambda, 11 callbacks, every initial value from kDefaultPatch, setValue(v, dontSendNotification)
- MainComponent.cpp::resized(): two-column layout, placeLabelled lambda, synthToggle/fxToggle merged onto one row
- MainComponent.cpp::prepareToPlay: pushAllParametersToSynth() called after synth.prepare(spec)
- Berlin.sln: clean compile, no .jucer regen needed

**Phase 6: Manual Audibility Gate (human-only)** — 1 task [x]
- (Confirmed 2026-09-06 by user): Drag every control while sustaining (immediate, no zipper/click); switch waveform mid-note (no dropout); switch LFO destination mid-note (nothing parked); saw/square/triangle sound like Phase 8; restart audio device (sliders/sound stay in sync). Explicit confirmation: "everything works great and as expected."

**Phase 7: Final Cleanup & Verification** — 4 tasks [x]
- BerlinTests.exe --category=Berlin: exit 0, 126/126 scenarios (up from 116 baseline, +10 new Delta: 9 SynthVoiceTests scenarios + 1 SynthEngineTests scenario)
- RT-safety review: applyParameters()/updateLfoModulation()/generator contain only relaxed atomic loads, pre-allocated scalar-coefficient updates (filter.setResonance, setCutoffFrequency, adsr.setParameters, lfo.setRate); no allocation/lock/log; initialise exactly once in prepare()
- Diff Source/core/, generation/, playback/, midi/, export/, Lfo.*, SynthEffects.* against phase base (1b7e8d2): zero output, byte-for-byte unchanged
- Spec delta merged into openspec/specs/internal-synth-voice/spec.md: numeric ranges spelled out inline; unmodified requirements carried over verbatim

**Total**: 25/25 tasks complete and independently verified.

## Verification Findings

**Verdict**: PASS WITH WARNINGS  
**Verdict Details**: All 25 tasks are complete. All spec requirements are satisfied. All 126 test scenarios pass (0 failures). 0 CRITICAL findings. 2 WARNING-level test-coverage gaps (accepted by user as low-priority follow-up, mitigated by confirmed human manual gate):

### Warnings

1. **Live pulse width change reshapes the pulse waveform** — no automated test scenario exercises pulse-width slider change mid-note and asserts audible duty-cycle shift. Mitigating factors: (a) manual gate task 6.1 implicitly exercises dragging every control including pulse width while a note sustains; (b) Phase 8 test SynthVoiceTests.cpp:169-173 explicitly states pulse at 50% is byte-identical to square, so a working pulseWidth slider is inseparable from working pulse; (c) accepted project convention for audible-qualia checks (same gap as Phase 7/8). Recommend lightweight follow-up reusing existing brightnessOf/peakAmplitude proxy pattern in SynthVoiceTests.cpp.

2. **Each of the four LFO destinations is demonstrable** — amplitude and pulseWidth LFO destinations never asserted to produce distinguishable audible effect in automated test; pitch and cutoff exercised via re-park tests. Mitigating factors: (a) same as (1); (b) manual gate verifies "all four destinations distinct" explicitly; (c) accepted convention. Recommend same lightweight follow-up as (1).

### No Critical Findings

- Spec compliance: all 6 modified/unmodified requirements PASS
- Design compliance: all 7 architecture decisions match code exactly
- All 126 test scenarios executed; all passing (independently re-run and re-counted)
- RT-safety confirmed: no allocation, lock, or logging on audio-thread path
- Untouched tier byte-for-byte identical
- No regression: existing 116 test scenarios all passing unchanged

## Source Code Changes Summary

**Committed (8 commits on feature branch `feat/parameter-controls`, local, unpushed)**:

Commit 1: Ranges + clamp helper (SynthPatch.h + clampParameter test)  
Commit 2: Table-free generator + equivalence test (SynthVoice oscillator refactor, RED/GREEN)  
Commit 3: Atomics + applyParameters() + LFO re-park (SynthVoice nested Parameters, 11 setters, ADSR gate, updateLfoModulation rewrite)  
Commit 4: SynthEngine forwarders (11 thin setters + SynthEngineTests coverage)  
Commit 5: MainComponent UI framework (controls, labels, configureSlider lambda)  
Commit 6: MainComponent resized() + prepareToPlay hook (two-column layout, pushAllParametersToSynth)  
Commit 7: Manual audibility gate (task 6.1 verified)  
Commit 8: Final verification (test count audit, RT-safety review, byte-for-byte untouched tier)  

**Total lines changed** (authored, excl. generated Builds/JuceLibraryCode):
- Modified Source/synth/: ~300 lines (SynthPatch, SynthVoice, SynthEngine)
- New/Modified MainComponent: ~200 lines (UI controls, layout, prepareToPlay hook)
- Modified Tests/Source/: ~180 lines (equivalence, live/mid-note, re-park, clamp, forwarder suites)
- **Total**: ~680 authored lines across 8 commits (estimate 450-550 proposal; actual ~680 including extended test coverage)

**Unchanged**:
- Source/core/, generation/, playback/, midi/, export/ — byte-for-byte identical
- Source/synth/Lfo.*, SynthEffects.* — byte-for-byte identical
- All pre-existing test suites — green (no regression)
- Berlin.jucer, Tests/BerlinTests.jucer — untouched (no new files)

## Parameter Control Architecture — Established

This phase establishes the live parameter control system for the internal synth:

1. **Message-thread -> Audio-thread atomics** — 11 independent lock-free atomics on SynthVoice; relaxed memory order sufficient (no cross-field invariant)
2. **Control-rate application** — applyParameters() once per 32-sample block; parameter changes visible next block (≤0.73 ms latency)
3. **Table-free waveform generation** — single 4-way lambda per sample, no allocation on switch, audibly equivalent to Phase 8 128-point tables
4. **Base-then-delta LFO modulation** — unconditional re-application of all base values + LFO delta to active destination only; closes Phase 8 latent bug
5. **ADSR change-gated for correctness** — setParameters() only when values actually changed; avoids silent `releaseRate` recompute edge during note release
6. **Two-column UI layout** — Berlin UI Pattern v1 preserved; parameter area (OSCILLATOR, FILTER, ENVELOPE, LFO sections) fits in one pass of existing 800×600 window
7. **Device restart stability** — re-push all parameters from UI after prepareToPlay, ensuring sliders and audio stay in sync across device changes

This architecture is intentionally live-first (every parameter modifiable mid-note, no allocation on the path) to satisfy "real-time sound design" use case. Future phases (Piano Roll, Preset System, Polyphony, Band-Limited Oscillators) can extend this without breaking the foundation.

## Observation IDs for Traceability

All SDD artifacts recorded in Engram (project: berlin):

| Artifact | Observation ID | Type |
|----------|---|---|
| `sdd/parameter-controls/proposal` | 178 | architecture |
| `sdd/parameter-controls/spec` | 179 | architecture |
| `sdd/parameter-controls/design` | 180 | architecture |
| `sdd/parameter-controls/tasks` | 181 | architecture |
| `sdd/parameter-controls/apply-progress` | 182 | architecture |
| `sdd/parameter-controls/verify-report` | 183 | architecture |
| `sdd/parameter-controls/archive-report` | (this document) | architecture |

## Dependencies & Precedents

This change depends on:
- `core-sequencing-model` (Phase 1, archived 2026-08-30)
- `playback-transport-clock` (Phase 4, archived 2026-09-02)
- `midi-output-routing` (Phase 5, archived 2026-09-03)
- `midi-export` (Phase 6, archived 2026-09-04)
- `midi-export-ui` (Phase 7, archived 2026-09-05)
- **`internal-synth` (Phase 8, archived 2026-09-06)** — direct predecessor; extends Phase 8's fixed-patch synth with live parameter control

Parameter Controls is a direct extension of Phase 8. Like all previous phases, it is purely additive to the source tier (`Source/core/`, `generation/`, `playback/`, `midi/`, `export/` remain untouched); it modifies only the synth tier (`Source/synth/`) and its spec (`internal-synth-voice`).

## Reconciliation Fixes Applied

The following spec/design reconciliation fixes were applied during spec-phase and verified in implementation:

1. **Resonance/self-oscillation wording** — Verified against JUCE StateVariableTPTFilter source (juce_StateVariableTPTFilter.cpp:132-137): `h = 1/(1 + R2·g + g²)` is unconditionally stable for finite resonance. The cap is a loudness/clipping choice (peak gain ≈ resonance), not a stability requirement. Implemented: `kMaxResonance = 8.0f` bounds peak gain to ≈18 dB.

2. **pulseWidth exposure** — Design Decision 5 specifies pulse width as a dedicated slider because Phase 8's test SynthVoiceTests.cpp:169-173 explicitly states "A 50%-duty pulse is byte-identical to the square generator." Shipping both without a pulse-width control ships a duplicate. Implemented: pulseWidthSlider with range [0.05, 0.95] in MainComponent, connected to setters.

3. **ADSR clamp language** — Design Decision 4 specifies `adsr.setParameters()` is gated on actual change. Verified against JUCE ADSR source (juce_ADSR.h:92-99, 155, 262-279): `setParameters()` calls `recalculateRates()`, which rewrites `releaseRate` from sustain, discarding the rate `noteOff()` computed. Calling unconditionally every block would silently mutate release timing. Implemented: comparison against cachedAdsrParams, setParameters only on difference.

## Known Limitations & Backlog

1. **Two WARNING-level test-coverage gaps** — pulse-width and amplitude/pulseWidth-destination LFO scenarios lack automated coverage (noted above); recommended lightweight follow-up to add brightnessOf/peakAmplitude proxies for these two destinations
2. **No preset save/load** — intentional; parameter UI without persistence per roadmap. Future phase: "Preset System"
3. **Monophonic only** — inherited from Phase 8; polyphony is a separate roadmap phase
4. **Naive (aliasing) oscillators** — table-free keeps Phase 8's naive generation; band-limited oscillators deferred

## Rollback & Recovery

If the change must be reverted:

1. Reset to phase base commit (1b7e8d2, last commit of Phase 8)
2. No Projucer regen needed (no new files)
3. Revert spec merges in `openspec/specs/internal-synth-voice/spec.md` to Phase 8 state (copy from archived change folder)
4. The feature is purely additive (modifications to SynthVoice/SynthEngine/MainComponent only); reverting all commits is clean and deterministic

## Next Phase (Phase 10 and Beyond)

The roadmap continues with candidates:
- **Piano Roll** (Phase 10 candidate) — visual sequencer for live composition
- **Preset System** — save/load patches, parameter recall
- **Generation/Randomize** — procedural patch generation per roadmap Phase 8 archive

Any of these phases can build on the live-parameter foundation established here. The parameter control system is now stable, fully RT-safe, and tested at 126/126 scenarios.

---

**Change Archived**: 2026-09-06  
**All Tasks**: 25/25 complete  
**Test Scenarios**: 126/126 passing (0 failures)  
**Spec Merged**: internal-synth-voice (Purpose + 5 requirements)  
**Verdict**: PASS WITH WARNINGS (0 CRITICAL, 2 WARNING accepted as follow-up)  
**Status**: Ready for the next roadmap phase.
