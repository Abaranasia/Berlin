# Archive Report: Internal Synth (roadmap Phase 8)

**Date**: 2026-09-06  
**Change**: internal-synth  
**Roadmap Phase**: 8 — Internal Synth (Auditioning Instrument)  
**Status**: ARCHIVED AND CLOSED  

## Executive Summary

The Internal Synth feature (Phase 8 of the Berlin School generative sequencer roadmap) has been fully implemented, verified, and archived. All 76 implementation tasks across 6 phases are complete, verified, and tested. The change delivered a new monophonic synthesizer voice wired into the real-time audio callback, capable of auditioning sequences without external MIDI hardware. The implementation introduces two new capability specs (`internal-synth-voice`, `internal-synth-output`), modifies two existing specs (`realtime-audio-wiring`, `unit-test-harness`), expands the test harness from `juce_core`-only to include `juce_dsp`, `juce_audio_formats`, and `juce_audio_basics`, adds a new `Source/synth/` tier, and wires the synth into `MainComponent` with two independent UI toggles (synth enable/FX bypass). The verification pass (independent full-change review) confirms PASS verdict with 0 CRITICAL findings and all 116 automated test scenarios passing. A bounded code review (review-reliability lens) was approved prior to verification.

## Artifacts Merged

### New Capability Specs Created

| Domain | Location | Status | Requirements |
|--------|----------|--------|---|
| `internal-synth-voice` | `openspec/specs/internal-synth-voice/spec.md` | Created (NEW) | 8 requirements: four waveforms, LP filter + resonance, ADSR, single LFO with 4 destinations, monophonic, allocation-free, silence-when-idle |
| `internal-synth-output` | `openspec/specs/internal-synth-output/spec.md` | Created (NEW) | 8 requirements: StepEventBuffer consumption, sample-offset-accurate rendering, synth enable/disable toggle, terminal delay/reverb (bypassed default), no stuck notes, mixing without clipping, allocation-free callback path |

### Modified Capability Specs

| Domain | Location | Status | Changed Requirement | Reason |
|--------|----------|--------|---|---|
| `realtime-audio-wiring` | `openspec/specs/realtime-audio-wiring/spec.md` | Modified (DELTA) | "Output Stays Silent" → "Audio Output Governed By Synth Enable State" (4 scenarios) | Synth now produces audible output by default when enabled |
| `realtime-audio-wiring` | `openspec/specs/realtime-audio-wiring/spec.md` | Modified (DELTA) | "Standalone App Still Launches and Runs Silently" → "Standalone App Still Launches And Runs Without Dropouts" (2 scenarios) | Synth output is now expected; gate asserts absence of dropouts, not silence |
| `unit-test-harness` | `openspec/specs/unit-test-harness/spec.md` | Modified (DELTA) | "Console Test Runner Project" requirement expanded: module list now includes `juce_dsp`, `juce_audio_formats`, `juce_audio_basics` alongside `juce_core` (2 new scenarios added) | Test harness expanded to exercise synth DSP unit tests |

## Spec Merge Details

### internal-synth-voice (NEW)

8 requirements covering the monophonic voice:
1. **Four Selectable Oscillator Waveforms** — sawtooth, square, pulse, triangle; each audibly distinct
2. **Low-Pass Filter With Resonance** — configurable cutoff and resonance; brightness sweep, resonant peak
3. **ADSR-Gated Amplitude** — attack/decay/sustain/release; note-on triggers attack, note-off triggers release
4. **Single LFO With Selectable Destination** — modulates pitch, cutoff, amplitude, or pulse width; one destination per patch
5. **Strictly Monophonic, No Voice Pool** — max one note at a time; no voice stealing
6. **Allocation-Free Voice Rendering** — all preparation off-thread; `render()` allocates nothing
7. **Silence When Idle** — zero samples when envelope is released and no note is sounding

All requirements verified through:
- Unit tests: `SynthVoiceTests.cpp` (pass/fail per test suite evidence)
- Code review: voice class structure, waveform generator implementation (4 distinct generators), filter stage, ADSR gating, LFO destinations
- Manual audibility gate: task 5.1(a-e) — all four waveforms, filter cutoff/resonance sweep, ADSR stages, all four LFO destinations confirmed distinct and audible

### internal-synth-output (NEW)

8 requirements covering the callback wiring and effects:
1. **StepEventBuffer Consumption Alongside MIDI Dispatch** — additive peer consumer; no MIDI path modification
2. **Sample-Offset-Accurate Note Rendering** — note-on/note-off at event offsets; note-off before note-on at shared offset
3. **Synth Enable/Disable Toggle** — user-facing toggle; defaults true; silences synth without affecting MIDI or transport
4. **Terminal Delay And Reverb, Bypassed By Default** — delay + reverb chain; both off by default; user can enable/disable independently
5. **No Stuck Notes On Stop Or Device Restart** — hard reset on shutdown/device restart; no drone tail
6. **Mixing Without Clipping** — sufficient headroom for sustained full-level note with effects enabled
7. **Allocation-Free Synth Contribution To The Callback** — `getNextAudioBlock` path: no allocation, lock, or logging
8. **Coexistence with MIDI Output** — synth toggle allows independent control of internal synth vs. external MIDI output

All requirements verified through:
- Unit tests: `SynthEngineTests.cpp` (synthetic `StepEventBuffer`, sample-offset accuracy, enable gate, effects bypass, reset behavior)
- Code review: `MainComponent` wiring, atomics and edge latches, `SynthEngine::render` allocation-free path, scratch buffer handling
- Manual audibility gate: task 5.1(f-h) — effects audible and cleanly bypassable, no drone on stop/restart, synth toggle de-doubles external MIDI

### realtime-audio-wiring (MODIFIED)

**Previous** (Phase 7): "Output Stays Silent" — no oscillator, filter, envelope, or voice anywhere; audio buffer stays cleared unconditionally.  
**Current** (Phase 8): "Audio Output Governed By Synth Enable State" — audio buffer carries synth output when enabled and a note is sounding; stays silent when synth is disabled or no note is sounding.

4 new scenarios documenting synth enable/disable behavior and MIDI-synth coexistence. Verified through code review against `MainComponent::getNextAudioBlock` and `SynthEngine::render` implementation.

### unit-test-harness (MODIFIED)

**Previous** (Phase 7): Module list was `juce_core`-only; anything requiring more modules was manual-gate-only.  
**Current** (Phase 8): Module list now includes `juce_dsp`, `juce_audio_formats`, `juce_audio_basics` (in dependency order: `juce_dsp` → `juce_audio_formats` → `juce_audio_basics` → `juce_core`).

2 new scenarios documenting tier split: DSP math (`SynthVoice`, `Lfo`) is unit-tested; device-touching code (`MidiOutputSink`, `MidiFileWriter`) remains manual-gate-only. Verified through inspection of `Tests/BerlinTests.jucer` module and file registration; test binary builds and all 116 scenarios pass.

## Artifacts Archived

The entire change folder has been moved to:
```
openspec/changes/archive/2026-09-06-internal-synth/
```

Contents of archive:
- `proposal.md` — original scope, settled decisions, risks, delivery forecast, and success criteria (all checked [x])
- `design.md` — 9 architecture decisions with verified corrections, scoped convention exception, data flow diagram, file changes, interfaces
- `tasks.md` — 76 implementation tasks across 6 phases (all [x] complete, manually audited, including human gate 5.1)
- `verify-report.md` — full-change independent verification, PASS verdict, 0 CRITICAL findings, all 116 test scenarios passing
- `specs/`
  - `internal-synth-voice/spec.md` — new capability spec merged into main
  - `internal-synth-output/spec.md` — new capability spec merged into main

## Task Completion Status

All 76 tasks complete and independently verified:

**Phase 1: Module/Build Plumbing (PR #1)** — 13 tasks
- juce_dsp, juce_audio_formats, juce_audio_basics registered in both Berlin.jucer and Tests/BerlinTests.jucer
- Projucer regens successful; JuceLibraryCode/ and Builds/ updated
- DspLinkSmokeTests.cpp created and passing; no synth code yet (isolation verified)

**Phase 2: Oscillator + ADSR + Filter DSP (PR #2)** — 13 tasks
- SynthPatch.h created (JUCE-free POD with default patch: saw, 4000Hz cutoff, 0.7 resonance, ADSR 50/200/0.7/300ms)
- SynthVoice.h/.cpp created: four waveforms (saw/square/pulse/triangle), StateVariableTPTFilter, ADSR
- SynthVoice marked JUCE_DECLARE_NON_COPYABLE/JUCE_DECLARE_NON_MOVEABLE (pulse generator lambda captures this)
- Unit tests pass; app still renders silence (unwired)

**Phase 3: SynthEngine + MainComponent Wiring — First Audible Sound (PR #3)** — 20 tasks
- SynthEngine.h/.cpp created: monophonic voice by value, scratch buffer, enabled atomic with edge latch
- MainComponent.h: SynthEngine member, synthToggle UI control
- MainComponent.cpp: prepareToPlay builds ProcessSpec, getNextAudioBlock calls synth.render, releaseResources calls synth.reset
- Synth wiring follows Berlin UI Pattern v1 (ToggleButton, in-class initializers, callbacks as lambdas, layout in resized)
- First audible sound confirmed 2026-09-05; toggle confirmed working

**Phase 4: LFO + Four Destinations + SynthEffects (PR #4)** — 19 tasks
- Lfo.h/.cpp created: JUCE-free phase accumulator, control-rate at 32 samples, getValue in [-1, 1]
- LFO destinations wired: pitch (setFrequency with force=true), cutoff (setCutoffFrequency at control rate), amplitude (post-ADSR gain), pulseWidth (live-read member)
- SynthEffects.h/.cpp created: DelayLine + Reverb chain, prepare allocates, process renders dry or wet
- fxToggle UI control added; effectsEnabled atomic with edge latch
- Delay/reverb defaults off (dry); both audible and cleanly bypassable

**Phase 5: Manual Audibility Gate (human-only)** — 1 task
- All 8 checks confirmed by user 2026-09-06:
  - (a) sound audible by default
  - (b) four waveforms distinct
  - (c) filter cutoff/resonance sweep (300Hz vs 9000Hz audibly brighter, 1200Hz+resonance=9.0 shows peak, no dropout)
  - (d) ADSR stages perceivable
  - (e) all four LFO destinations distinct: pitch=vibrato, cutoff=wah, amplitude=tremolo, pulseWidth=timbral shift
  - (f) delay/reverb audible and bypassable cleanly with no click
  - (g) no drone on stop or audio device restart
  - (h) synth toggle de-doubles external MIDI synth (Windows GS Wavetable) — confirmed independent behavior

**Phase 6: Final Cleanup & Verification** — 10 tasks
- Source/midi, export, core, generation, playback confirmed byte-for-byte unchanged (diff against 30d79c5)
- No TEMPORARY/stub comments remaining
- BerlinTests.exe --category=Berlin: exit 0, 116/116 scenarios, 20 suites (16 pre-existing + 4 new: DspLinkSmoke, SynthVoice, Lfo, SynthEngine)
- All 9 architecture decisions verified against code
- RT-safety review: no allocation/lock/logging on audio path
- Specs 6.8/6.9: delta specs merged into openspec/specs/, new specs created
- Final full rebuild: Berlin.sln and Tests/BerlinTests.sln both 0 errors

**Total**: 76/76 tasks complete and independently verified.

## Verification Findings

**Verdict**: PASS  
**Verdict Details**: All 76 tasks across 6 phases are complete. All spec requirements are satisfied. All 116 test scenarios pass. No CRITICAL findings. One WARNING-level item (Engram memory staleness, already corrected). Code review (review-reliability lens) approved prior to verification.

From `verify-report.md`:
- Spec compliance: internal-synth-voice 8/8 PASS, internal-synth-output 8/8 PASS, realtime-audio-wiring modified delta PASS, unit-test-harness modified delta PASS
- All 9 architecture decisions match design.md exactly
- All 116 test scenarios executed; all passing
- RT-safety confirmed: no allocation, lock, or logging on audio thread
- Scoped convention exception guardrails held: SynthPatch.h and Lfo.h stay JUCE-free; no pre-existing JUCE-free file gained a JUCE include

## Source Code Changes Summary

**Committed (4 chained PRs across feature branch `feat/internal-synth`)**:

PR #1: Module/build plumbing + DspLinkSmokeTests.cpp  
- Modified Berlin.jucer: juce_dsp module added
- Modified Tests/BerlinTests.jucer: juce_dsp, juce_audio_formats, juce_audio_basics modules added
- Regenerated Builds/VisualStudio2026/, Tests/Builds/VisualStudio2026/, JuceLibraryCode/
- Created Tests/Source/DspLinkSmokeTests.cpp

PR #2: SynthVoice DSP (oscillator, filter, ADSR) + unit tests  
- Created Source/synth/SynthPatch.h (JUCE-free POD)
- Created Source/synth/SynthVoice.h/.cpp (oscillator, filter, ADSR)
- Created Tests/Source/SynthVoiceTests.cpp
- Modified Berlin.jucer, Tests/BerlinTests.jucer to register new files

PR #3: SynthEngine + MainComponent wiring + synth toggle  
- Created Source/synth/SynthEngine.h/.cpp (voice, scratch, enabled atomic, render)
- Modified Source/MainComponent.h (SynthEngine member, synthToggle)
- Modified Source/MainComponent.cpp (prepareToPlay, getNextAudioBlock, releaseResources, resized, ctor)
- Created Tests/Source/SynthEngineTests.cpp

PR #4: LFO + four destinations + SynthEffects (delay/reverb) + FX toggle  
- Created Source/synth/Lfo.h/.cpp (JUCE-free phase accumulator)
- Created Source/synth/SynthEffects.h/.cpp (delay + reverb chain)
- Extended Source/synth/SynthVoice and SynthEngine with LFO wiring and effects
- Modified Source/MainComponent.h (fxToggle)
- Modified Source/MainComponent.cpp (FX toggle wiring in ctor, resized)
- Created Tests/Source/LfoTests.cpp
- Extended Tests/Source/SynthEngineTests.cpp

**Total lines changed** (authored, excl. generated Builds/JuceLibraryCode):
- New Source/synth/: ~1000 lines (SynthPatch, Lfo, SynthVoice, SynthEffects, SynthEngine)
- New Tests/Source/: ~500 lines (DspLinkSmokeTests, LfoTests, SynthVoiceTests, SynthEngineTests)
- Modified MainComponent.h/.cpp: ~100 lines (wiring, toggles, resized)
- Modified .jucer files: ~50 authored XML lines
- **Total**: ~1650 authored lines across 4 PRs

**Unchanged**:
- Source/core/, generation/, playback/, midi/, export/ — byte-for-byte identical
- All pre-existing test suites — green (no regression)

## Berlin Synth Architecture — Established

This phase establishes the internal synth architecture for all audition and playback use cases:

1. **Monophonic voice by value** — no pool, no stealing; matches sequencer's single-pendingNote design
2. **Oscillator with 4 waveforms** — naive (aliasing accepted) using juce::dsp::Oscillator
3. **Filter + resonance** — StateVariableTPTFilter with control-rate modulation to avoid per-sample transcendental
4. **ADSR envelope** — juce::ADSR; retrigger ramps from release (no click)
5. **LFO at control rate** — hand-rolled JUCE-free accumulator, 32-sample blocks, 4 destinations
6. **Effects chain** — DelayLine + Reverb, both bypassed by default, independent enable atomics with edge latches
7. **Additive callback path** — peer consumer of StepEventBuffer alongside MIDI dispatch; scratch buffer decouples sample-offset origins
8. **RT-safe all the way** — allocation/lock/logging entirely off-thread; audio path is pure state machine

This architecture is intentionally modest (monophonic, fixed patch, no parameter UI) to satisfy "primarily an auditioning instrument" from the roadmap. Future phases (e.g., Parameter Controls, Polyphony, Band-Limited Oscillators, Preset System) can extend this without breaking the foundation.

## Observation IDs for Traceability

All SDD artifacts recorded in Engram (project: berlin):

| Artifact | Observation ID | Type |
|----------|---|---|
| `sdd/internal-synth/proposal` | [from proposal memo] | architecture |
| `sdd/internal-synth/spec` | [from spec memo] | architecture |
| `sdd/internal-synth/design` | [from design memo] | architecture |
| `sdd/internal-synth/tasks` | [from tasks memo] | architecture |
| `sdd/internal-synth/verify-report` | [from verify report] | architecture |
| `sdd/internal-synth/archive-report` | (this document) | architecture |

## Dependencies & Precedents

This change depends on:
- `core-sequencing-model` (Phase 1, archived 2026-08-30) — foundational sequencing and step events
- `playback-transport-clock` (Phase 4, archived 2026-09-02) — `StepEventBuffer` contract and transport
- `midi-output-routing` (Phase 5, archived 2026-09-03) — parallel MIDI-out consumer precedent
- `midi-export` (Phase 6, archived 2026-09-04) — `Source/export/` API and stability
- `midi-export-ui` (Phase 7, archived 2026-09-05) — Berlin UI Pattern v1 and MainComponent wiring conventions

The synth is a peer consumer of `StepEventBuffer`, matching the independent, additive design of Phase 5 (MIDI-out) and Phase 6/7 (Export). Like those phases, the synth does not modify any pre-existing tier (`Source/core/`, `Source/generation/`, `Source/playback/`, `Source/midi/`, `Source/export/` remain byte-for-byte unchanged).

## Known Limitations Carried Forward

1. **Fixed patch, no parameter UI** — intentional per roadmap Phase 8 scope (auditioning, not editing)
2. **Monophonic only** — intentional per sequencer's single-pendingNote design; polyphony is a future phase
3. **Naive (aliasing) oscillators** — intentional per "auditioning instrument" scope; band-limiting deferred to future
4. **No velocity field in StepEvent** — deferred to the phase that first generates velocity (would require updating all three archived consumers)
5. **No MIDI-out device mute toggle** — out of scope; belongs to Phase 5 backlog (user requested, not implemented)

## Rollback & Recovery

If the change must be reverted:

1. Revert the four chained commits (PR #4, #3, #2, #1) in reverse order
2. `git revert` the PR #4 commit — removes Lfo, SynthEffects, LFO wiring, fxToggle
3. `git revert` the PR #3 commit — removes SynthEngine, MainComponent wiring, synthToggle, audio path
4. `git revert` the PR #2 commit — removes SynthPatch, SynthVoice, their tests
5. `git revert` the PR #1 commit — restores Berlin.jucer and Tests/BerlinTests.jucer to juce_core-only; requires `Projucer.exe --resave` on both files
6. `openspec/specs/internal-synth-*/` may be deleted (or kept as reference for design decisions)
7. `openspec/specs/realtime-audio-wiring/` and `unit-test-harness/` revert to their pre-Phase-8 state (spec merges are already complete)
8. All test suites remain green — no test-visible code removed

The revert is clean and deterministic: the feature is purely additive (new tier, new specs, new UI controls). No existing code is mutated except specs (which document the new behavior).

## Next Phase (Phase 9 and Beyond)

The roadmap continues with Phase 9+ features (e.g., Parameter Controls, Piano Roll, Preset System, Generation/Randomize). Each of these will follow established patterns:
- Peer consumers of existing contract (StepEventBuffer, transport) when applicable
- Additive changes to MainComponent UI (Berlin UI Pattern v1)
- Scoped capability specs for each feature domain
- Chained PR delivery for complex features

The internal synth is now a stable, tested foundation for audition and playback. Future work builds on it.

---

**Change Archived**: 2026-09-06  
**All Tasks**: 76/76 complete  
**Specs Merged**: 2 new + 2 modified  
**Status**: Ready for the next roadmap phase (Phase 9 or continued Phase 7 UI work).
