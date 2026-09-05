# Proposal: Internal Synth (roadmap Phase 8)

Artifact store: hybrid. File: `openspec/changes/internal-synth/proposal.md`. Delivery: **chained-pr**, 400-line review budget (forecast below).

## Intent

The app schedules notes correctly but outputs silence — `getNextAudioBlock` (`Source/MainComponent.cpp:109-115`) clears the buffer and only forwards MIDI to an external device. Without hardware there is nothing to hear, so generation cannot be evaluated. Roadmap deliverable: "Application can sound sequences without external hardware". The roadmap frames the synth as "primarily an auditioning instrument in the initial versions" — scope stays modest deliberately.

## Settled decisions (user-made, not open questions)

1. **Use `juce::dsp` primitives** (`Oscillator`, `StateVariableTPTFilter`, `ADSR`, `DelayLine`, `Reverb`) rather than hand-rolled math. This is an **intentional departure** from the `core/`/`playback/`/`export/` JUCE-free-core convention. Rationale: battle-tested, correct DSP beats re-deriving oscillator/filter/envelope math. Cost: `Source/synth/` lives in a JUCE-coupled tier.
2. **Expand `Tests/BerlinTests.jucer`** beyond `juce_core` to include `juce_dsp` (+ transitive `juce_audio_basics`). First-of-its-kind change: the harness has been `juce_core`-only by design since Phase 0. Accepted so decision 1 does not cost coverage.
3. **Additive**: the synth is a parallel consumer of `StepEventBuffer` alongside the Phase 5 MIDI-out and Phase 6/7 export paths. Neither is modified.

## Velocity question — resolved: DEFER

Investigated. No velocity source exists anywhere: `Step` has no velocity field, the generators produce none, `MidiExportEvent` is `{tick, note, noteOn}`, and `MidiEventTranslator::kNoteVelocity` is a single constant also handed to `MidiFileWriter`. Adding the field now would force delta specs on three archived capabilities to carry a constant. Phase 8 uses a fixed per-note level shaped by ADSR; the field lands in the phase that first *generates* velocity, updating all consumers at once.

## Scope

### In Scope
- New `Source/synth/` tier: oscillator (saw/square/pulse/triangle), low-pass filter + resonance, ADSR, one LFO (destinations: pitch, filter cutoff, amplitude, pulse width).
- Single monophonic voice, allocation-free, consuming `StepEventBuffer` note-on/note-off (matches the sequencer's existing one-`pendingNote` behavior — no voice pool, no voice-stealing).
- Delay + reverb as a terminal per-block effects pass, bypassed by default (dry).
- Rendering into `bufferToFill` in `getNextAudioBlock`, additive to MIDI dispatch; voice silenced on `releaseResources`.
- A synth enable/disable toggle so the internal synth can be muted independently of the external MIDI-out path (Phase 5), avoiding a doubled-up sequence when both are active.
- `juce_dsp` added to `Berlin.jucer`; `juce_dsp` + `juce_audio_basics` added to `Tests/BerlinTests.jucer`.
- A fixed default patch, hardcoded.

### Out of Scope
- Any synth parameter UI or preset system — Phase 8 is headless, one fixed patch.
- Roadmap "eventually" items: multi-osc, detune, sync, noise, HP/BP/multimode filter, chorus, phaser, flanger, tape delay, saturation.
- More than one LFO; per-voice modulation matrix; MPE; velocity (see above).
- Any change to `Source/core/`, `generation/`, `playback/`, `midi/`, `export/`.
- Audio-device or channel-count changes; stereo output config stays `setAudioChannels(2, 2)`.

## Capabilities

### New
- `internal-synth-voice`: waveform selection, LP filter + resonance, ADSR gating, LFO destinations, fixed-capacity voice allocation and stealing, per-voice RT-safety.
- `internal-synth-output`: `StepEventBuffer` consumption inside `getNextAudioBlock`, sample-offset-accurate note dispatch, mixing/headroom, delay + reverb chain, silence on shutdown/device restart, coexistence with MIDI dispatch.

### Modified
- `realtime-audio-wiring`: its "Output Stays Silent" requirement ("no oscillator, filter, envelope, or voice is introduced") is directly contradicted and MUST be replaced. Allocation-free / lock-free / log-free callback requirements stay intact and now cover the synth path.
- `unit-test-harness`: its "module list remains `juce_core`-only" and "tier splits into unit-tested and manual-gate-only halves" requirements must accommodate `juce_dsp` + `juce_audio_basics`.

## Approach

1. **Tap point**: `SynthEngine::render(blockEvents, buffer, startSample, numSamples)` called in `getNextAudioBlock` after `player.process()`, alongside — not instead of — `midiTranslator`/`midiSink`. Reuses the note-off-before-note-on-at-shared-offset ordering `MidiEventTranslator` already relies on.
2. **Sample-offset accuracy**: render in segments between event offsets, the same shape `SequencePlayer`/`MidiEventTranslator` use. `StepEvent::sampleOffset` is relative to block start, **not** to `bufferToFill.startSample` — the caller must add it.
3. **RT-safety**: `std::array<Voice, N>` pool, `prepare(sampleRate)` allocates everything up front (including `DelayLine`/`Reverb` internals), `render` allocates nothing, locks nothing, logs nothing. Mirrors `StepEventBuffer`'s fixed-capacity pattern.
4. **Testability**: with `juce_dsp` in the harness, voice/engine DSP is exercised by offline block rendering with a synthetic `StepEventBuffer` — asserting envelope shape, silence when idle, and non-silence on note-on — rather than falling back to `MidiOutputSink`'s manual-gate-only tradeoff.
5. **Shutdown**: `releaseResources` already flushes a pending note-off for MIDI; the engine must also hard-reset voices and effect tails so a device restart cannot leave a drone.

## Affected Areas

| Area | Impact | Description |
|------|--------|-------------|
| `Source/synth/` | New | Oscillator, filter, ADSR, LFO, Voice, SynthEngine, effects chain |
| `Source/MainComponent.h/.cpp` | Modified | `SynthEngine` member; `prepareToPlay`, `getNextAudioBlock`, `releaseResources` wiring |
| `Berlin.jucer` + `Builds/`, `JuceLibraryCode/` | Modified/regenerated | `<MODULE id="juce_dsp">` + new `<FILE>` entries; Projucer resave |
| `Tests/BerlinTests.jucer` + `Tests/Builds/` | Modified/regenerated | Module list expanded; new synth test files registered |
| `Tests/Source/` | New | Voice/engine/DSP suites |
| `openspec/specs/internal-synth-voice/`, `internal-synth-output/` | New | New capability specs |
| `openspec/specs/realtime-audio-wiring/`, `unit-test-harness/` | Modified | Delta specs |
| `Source/core/`, `generation/`, `playback/`, `midi/`, `export/` | Untouched | Consumed unchanged; must be byte-for-byte identical in the final diff |

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| Audio-thread allocation via `juce::dsp` (`DelayLine`/`Reverb` resize, `AudioBlock` misuse) → dropouts | High | All sizing in `prepare(sampleRate)`; `render` is allocation-free by spec requirement and code review, same gate `realtime-audio-wiring` already applies |
| Two `.jucer` regens (app + tests) produce large mechanical diffs that swamp review | High | Isolate both regens in chained PR #1; reviewers see generated churn separately from DSP logic |
| Expanding the test harness destabilises a build that has been `juce_core`-only for 8 phases | Med | PR #1 lands the module change alone with a trivial link-smoke test before any synth code depends on it |
| Aliasing / harsh output from naive saw/square — `juce::dsp::Oscillator` is not band-limited by default | Med | Resolved: accepted as-is for an auditioning instrument; revisit only if a later phase finds it objectionable |
| Stuck note on loop wrap or device restart | Med | Hard voice reset on `releaseResources`; "no drone after stop" is a manual gate. Monophonic scope means no voice-stealing policy needed |
| Gain staging: oscillator + resonance + reverb clip the output | Med | Fixed headroom/gain in the default patch; verify no clipping on the single voice at sustained full level |
| Scope creep into synth parameter UI or "eventually" DSP features | High | Enumerated non-goals; a synth parameter/patch-editing UI is a separate later change. The one exception in this phase's scope is a single enable/disable toggle for the synth itself (needed for MIDI-out coexistence) |
| Precedent risk: "use `juce::dsp`" is read as licence to abandon JUCE-free cores everywhere | Med | `design.md` must state the exception is scoped to `Source/synth/` and why |

## Delivery forecast (review workload guard)

- **400-line budget risk: High.** New tier + tests + two `.jucer`/build regens far exceeds 400 authored lines.
- **Chained PRs recommended: Yes.** Proposed slices: (1) module/build plumbing + link smoke test; (2) oscillator + ADSR + filter with unit tests, unwired; (3) single-voice `SynthEngine` + `MainComponent` wiring (incl. enable/disable toggle) — first audible sound; (4) LFO + delay + reverb (bypassed by default).
- **Decision needed before apply: Yes** — confirm the four-slice split at `sdd-tasks`.

## Rollback Plan

`git revert` the slice commits in reverse order. Slices 2–4 are source-only: deleting `Source/synth/` and reverting `MainComponent.h/.cpp` restores Phase 7 behaviour (silent buffer, MIDI-out only). Slice 1 requires restoring `Berlin.jucer` and `Tests/BerlinTests.jucer` and running `Projucer.exe --resave` on both. No data migration, no user-visible file format change; exported `.mid` files are unaffected because `Source/export/` is untouched. Reverting to the `juce_core`-only harness restores the previous green suite exactly.

## Dependencies

- Exploration: Engram `sdd/internal-synth/explore`.
- Pinned JUCE 9.0.1 at `H:/Proyectos/Juce/JUCE` — `juce_dsp` confirmed present, not yet referenced by either `.jucer`.
- Projucer, for both regens.
- Archived Phase 4 `playback-transport-clock` (`StepEventBuffer` contract) and Phase 5 `midi-output-routing` (parallel consumer precedent).
- A human for the audio manual gate — "it sounds right" is not automatable.

## Success Criteria

- [ ] Launching the standalone app produces audible sound from the seeded sequence with no MIDI device connected.
- [ ] Notes start and stop on the correct steps; no stuck/hanging notes across loop wraps, or after stop and device restart.
- [ ] All four waveforms, the LP filter + resonance sweep, ADSR stages, and each of the four LFO destinations are audibly demonstrable from the fixed patch.
- [ ] Delay and reverb are audible and can be bypassed to silence-tails without artefacts.
- [ ] No dropouts, XRuns, or asserts over a sustained run of the monophonic voice.
- [ ] External MIDI output and MIDI export behave exactly as in Phase 7 — `Source/midi/*` and `Source/export/*` are byte-for-byte unchanged in the final diff.
- [ ] `BerlinTests.exe --category=Berlin` exits 0, including new synth suites, with the expanded module list.
- [ ] Code review confirms no allocation, lock, or logging call on the synth render path.

## Proposal question round — resolved 2026-09-05

1. **Polyphony**: strictly monophonic, matching the sequencer's existing single-`pendingNote` behavior. No voice pool, no voice-stealing policy needed for this phase.
2. **Aliasing tolerance**: naive (non-band-limited) `juce::dsp::Oscillator` waveforms are accepted as-is, consistent with the "auditioning instrument" scope. Band-limiting deferred to a later phase if it proves audibly objectionable.
3. **Effects default state**: delay and reverb ship **bypassed by default** (dry signal), not always-on. User enables them explicitly.
4. **MIDI-out coexistence**: a simple enable/disable toggle for the internal synth is in scope, so the user isn't forced to hear the sequence doubled when an external MIDI-out synth (Phase 5) is also active. This pulls a small UI control into this phase's scope — confirm with `sdd-design`/`sdd-tasks` whether it belongs in `MainComponent` alongside the Phase 7 export button, or elsewhere.
5. **Slice 3 stopping point**: unchanged from the original assumption — "first audible sound" (voice pool + oscillator/filter/ADSR wired, no LFO/effects yet) is an acceptable intermediate merge point for the chained-pr delivery strategy.
