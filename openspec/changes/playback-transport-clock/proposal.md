# Proposal: Playback Transport & Step Clock

## Intent

`core-sequencing-model` landed a tested domain model and seeded generators, but nothing in Berlin ever *runs*: `MainComponent::getNextAudioBlock` still only clears the buffer, and a generated `Sequence` is a value nobody plays. Roadmap Phase 4 (proposal doc line 1457) closes that gap — "Transport, BPM, Timing, Internal MIDI event scheduler", deliverable "Generated sequence plays correctly".

Phase 5 (MIDI device I/O) and Phase 8 (internal synth) do not exist yet, so "plays correctly" **cannot** mean audible sound or external MIDI. This change defines it as a falsifiable timing contract: the transport emits note-on/note-off events at sample-accurate positions derived from BPM, verified by asserted timestamps in the headless test harness. That gives Phases 5, 7 and 8 a proven clock to plug into instead of building output on unmeasured timing.

## Scope

### In Scope

- `Source/playback/StepEvent.h` — project-local value type `{ sampleOffset, stepIndex, note, isNoteOn }`. No `juce::MidiMessage`.
- `Source/playback/StepEventBuffer.h` — fixed-capacity, heap-free event sink (no allocation on the audio thread).
- `Source/playback/Transport.h/.cpp` — JUCE-free step clock: `{ bpm, stepsPerBeat }` + `prepare(sampleRate)` → samples-per-step; `advance(numSamples)` reports step boundaries with in-block sample offsets; running/stopped state; drift-free fractional accumulation; loop wrap.
- `Source/playback/SequencePlayer.h/.cpp` — drives a `Sequence` from `Transport` boundaries, emitting ordered note-off/note-on `StepEvent`s into a caller-supplied `StepEventBuffer`.
- `Source/MainComponent.h/.cpp` — **first change to touch these**: generate the sequence on the message thread before `setAudioChannels`, derive timing in `prepareToPlay`, advance the player in `getNextAudioBlock`, still output silence. One `std::atomic<int>` playhead as the seam later phases read.
- `Tests/Source/{Transport,SequencePlayer,PlaybackTiming}Tests.cpp` — timestamp-asserting suites, category `"Berlin"`.
- `Berlin.jucer` + `Tests/BerlinTests.jucer` — `<FILE>` registration for the shared `Source/playback/*` sources.

### Out of Scope (explicit non-goals)

- Audible output. No oscillator, filter, envelope or voice — Phase 8. The audio buffer stays cleared.
- Real MIDI I/O and `juce::MidiMessage`/`MidiBuffer`/`juce_audio_basics` — Phase 5. The test project stays `juce_core`-only.
- UI: transport buttons, BPM slider/field, playhead rendering — Phase 7. `paint`/`resized` stay stock.
- `MusicalTime` / PPQN / bar-beat-tick abstraction. Still deferred, per `core-sequencing-model`'s non-goal.
- External MIDI clock and DAW host-transport sync (proposal §23) — needs Phase 11.
- Mutation/evolution engine, per-step gate/duration/velocity fields and their generators.
- Runtime re-generation of the sequence while audio is running, and any BPM/transport control surface.
- CMake. Build system stays Projucer.

## Capabilities

### New Capabilities

- `playback-transport`: BPM + step resolution → samples-per-step, drift-free advance, running/stopped state, looping playhead.
- `step-event-scheduling`: `StepEvent`, note-off/note-on pairing and ordering, in-block sample offsets, fixed-capacity non-allocating sink, inactive-step and empty-sequence behaviour.
- `realtime-audio-wiring`: `MainComponent` lifecycle contract — sequence built before audio starts, timing derived in `prepareToPlay`, allocation-free/lock-free/log-free `getNextAudioBlock`, silent output.

### Modified Capabilities

None. `sequencing-core` and `deterministic-generation` are consumed unchanged; `unit-test-harness` gains suites but no requirement changes, because the `juce_core`-only module set is deliberately preserved.

## Resolved Open Questions

Settled for spec/design — not to be re-litigated:

- **Clock source**: the audio callback, using `prepareToPlay`'s `sampleRate`. Rejected: a message-thread `juce::Timer` (not sample-accurate; throwaway once Phases 5/8 need audio-thread timing).
- **Event type**: project-local `StepEvent`, not `juce::MidiMessage`. Rationale: `juce_audio_basics` in `Tests/BerlinTests.jucer` would break the settled `juce_core`-only boundary for a type nothing yet consumes. Translation to real MIDI is Phase 5's job.
- **Loop behaviour**: the sequence loops indefinitely (standard step-sequencer behaviour), not play-once-and-stop.
- **BPM**: fixed at construction/config time (constant + member). No runtime control surface until Phase 7.
- **Step resolution**: one step = a 16th note (`stepsPerBeat = 4`), fixed. `samplesPerStep = sampleRate * 60 / (bpm * stepsPerBeat)`.
- **Fractional steps**: `samplesPerStep` is usually non-integer (44.1 kHz @ 120 BPM → 5512.5). Step boundaries derive from an absolute running sample position, never from repeatedly adding a rounded integer, so error cannot accumulate.
- **Note-off timing**: `Step` has no gate/duration field (deferred by the prior change), so a note plays for its full step — note-off is emitted at the next step boundary, ordered **before** that step's note-on at the same sample offset. A real gate parameter is later, additive work.
- **Verification of "plays correctly"**: a test drives `SequencePlayer::process` over N simulated blocks (including irregular block sizes and blocks shorter than one step) and asserts each emitted event's *absolute* sample timestamp equals `round(k * samplesPerStep)` within ±1 sample across the loop wrap, and that the note values match the seeded sequence. No audio device, no listening, no log-inspection.

## Approach

1. **Domain first, wiring last.** `Transport` → `SequencePlayer` land TDD-red/green in the `juce_core`-only harness, driven by explicit `process(numSamples)` calls with no audio device. Only once green does `MainComponent` get wired.
2. **JUCE-free playback tier.** `Source/playback/*` uses the standard library only, mirroring the `Source/core/` boundary. This is what keeps the test project's module set unchanged and makes headless timing tests trivial.
3. **Real-time safety by construction.** The event sink is a fixed-capacity value type sized for the worst realistic block-size/BPM ratio, with a documented drop-and-flag policy on overflow. `getNextAudioBlock` performs no allocation, no locking, no `juce::String`/`Logger` call — observability is one `std::atomic<int>` playhead, written by the audio thread and read by later phases.
4. **Timing asserted, not observed.** A dedicated drift suite runs thousands of simulated steps and asserts cumulative error stays bounded — the falsifiability guarantee for the Phase 4 deliverable.
5. **Minimal `MainComponent` diff.** Sequence generation happens in the constructor *before* `setAudioChannels(2, 2)` (which starts audio immediately), so the audio thread only ever reads an immutable, fully-built `Sequence`. `paint`/`resized`/`releaseResources` structure stays stock.
6. **Mechanical regeneration.** Every `.jucer` file-list edit runs the established 6-step registration checklist (both files, `../` prefix for tests, `Projucer.exe --resave` on both, sync check, build both).

## Affected Areas

| Area | Impact | Description |
|------|--------|-------------|
| `Source/playback/` | New | `StepEvent.h`, `StepEventBuffer.h`, `Transport.h/.cpp`, `SequencePlayer.h/.cpp` |
| `Source/MainComponent.h/.cpp` | Modified | **First touch.** Sequence + player members, `prepareToPlay` timing, `getNextAudioBlock` advance, atomic playhead |
| `Source/Main.cpp` | Untouched | Stock `START_JUCE_APPLICATION` scaffold needs no change |
| `Tests/Source/` | New | `TransportTests.cpp`, `SequencePlayerTests.cpp`, `PlaybackTimingTests.cpp` |
| `Berlin.jucer`, `Tests/BerlinTests.jucer` | Modified | New `<GROUP name="playback">` `<FILE>` entries only; module lists unchanged |
| `Builds/`, `JuceLibraryCode/`, `Tests/Builds/`, `Tests/JuceLibraryCode/` | Regenerated | `Projucer.exe --resave` output |
| `Source/core/`, `Source/generation/` | Untouched | Consumed as-is; no signature changes |

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| First-ever edit to `MainComponent.*` — every prior change asserted these files unchanged; risk of breaking app startup/audio-device init | High | Keep the diff minimal and additive; do not restructure stock methods; explicit gate: the standalone app still launches, opens the device and runs silently |
| A silent, inaudible deliverable becomes unfalsifiable ("it plays, trust me") | High | "Plays correctly" is defined *only* as asserted absolute sample timestamps + note values; a reviewer rejects any success claim resting on listening or logs |
| Real-time rule violation: allocation, lock, or logging inside `getNextAudioBlock` (constitution, doc line 1970-1977) | Med | Fixed-capacity `StepEventBuffer`, immutable `Sequence`, atomic-only shared state; explicit review check on the callback body |
| Cumulative timing drift from rounded `samplesPerStep` | Med | Absolute-position derivation, not incremental integer addition; long-run drift test is a required suite |
| Pressure to use `juce::MidiMessage` "since it says MIDI scheduler" — would pull `juce_audio_basics` into the test project | Med | Settled above; the module lists in both `.jucer` files must be unchanged in the final diff |
| Event-sink overflow at extreme BPM/tiny block sizes | Low | Capacity sized from worst realistic ratio, with a specified, tested drop-and-flag policy rather than undefined behaviour |
| Scope creep into synth voices, MIDI output or transport UI | Med | Non-goals listed explicitly; each maps to a named later phase |
| Degenerate inputs (empty `Sequence`, `bpm <= 0`, `prepareToPlay` never called) | Low | Specified as required behaviour (emit nothing, never divide by zero) and covered by tests |

## Rollback Plan

Two-part revert. (1) `git revert` the change commits: `Source/playback/` and the new `Tests/Source/*Tests.cpp` disappear wholesale, and `Source/MainComponent.h/.cpp` return to the stock scaffold — this is the only non-additive part, and its pre-change content is the untouched Projucer template, so the restored state is exactly verifiable. (2) Restore the `<FILE>` lists in `Berlin.jucer` and `Tests/BerlinTests.jucer`, then run `Projucer.exe --resave` on both to regenerate `Builds/` and `JuceLibraryCode/`. Post-rollback the app returns to clearing the buffer and the existing 34 tests stay green; no persisted state or file format is involved.

## Dependencies

- `sequencing-core` (`Sequence`, `Step`) and `deterministic-generation` (`RhythmGenerator`, `PitchGenerator`, `DeterministicRandom`) as shipped by `core-sequencing-model`.
- Projucer binary available headlessly (`Projucer.exe --resave`) and a resolvable global JUCE modules path.
- An audio device for the standalone app's manual launch gate only. **No** audio device, `juce_audio_basics` or `juce_audio_devices` is required by the test harness.

## Success Criteria

- [ ] `Transport` converts `{bpm, stepsPerBeat, sampleRate}` to samples-per-step and reports step boundaries with correct in-block sample offsets, tested across irregular block sizes and blocks shorter than one step.
- [ ] A timing test asserts absolute note-on timestamps equal `round(k * samplesPerStep)` within ±1 sample for k = 0..N-1, spanning at least one full loop wrap.
- [ ] A drift test over thousands of steps proves cumulative error stays bounded (no growth from rounded step lengths).
- [ ] `SequencePlayer` emits note-off before note-on at a shared boundary offset, emits nothing for inactive steps, and emits the note values of the seeded generated sequence in order.
- [ ] The sequence loops indefinitely: step `N-1` is followed by step `0` with no timing gap or double event.
- [ ] Empty sequence, `bpm <= 0`, and stopped transport emit no events and never divide by zero.
- [ ] `getNextAudioBlock` contains no allocation, lock, `juce::String` or logging call — verified by code review against the constitution rule.
- [ ] `Tests/BerlinTests.jucer` still links `juce_core` only; both `.jucer` module lists are unchanged in the final diff.
- [ ] `BerlinTests.exe --category=Berlin` exits 0 with the previous 34 cases plus the new playback suites.
- [ ] The standalone app builds, launches, opens its audio device, and runs the transport silently without dropouts or asserts.
