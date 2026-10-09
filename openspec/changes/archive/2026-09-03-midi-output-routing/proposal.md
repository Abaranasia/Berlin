# Proposal: MIDI Output Routing

## Intent

`playback-transport-clock` proved the clock but wired it to nothing: `MainComponent::getNextAudioBlock` fills `blockEvents` every block and **nobody reads it**. The app runs a correct, drift-free sequence into a void. Roadmap Phase 5 (proposal doc lines 1474-1489) closes that: "MIDI device enumeration, MIDI output, note on/off, channel selection", deliverable "Application controls external synthesizer".

This is the first phase whose deliverable is externally observable: a hardware or virtual synth receives the notes. It also forces payment of a debt Phase 4 explicitly deferred — `SequencePlayer::stop()` never flushes `pendingNote`, and a stuck note only becomes audible once real MIDI leaves the process.

## Scope

### In Scope

- `Source/midi/MidiEventTranslator.h` (or equivalent) — JUCE-thin conversion of `StepEvent` → MIDI note-on/off on a selected 1-16 channel, into a **pre-reserved, reused** `juce::MidiBuffer`. Any pure byte/channel packing logic is extracted JUCE-free so it is unit-testable.
- `Source/midi/MidiOutputSink.h/.cpp` — owns the `std::unique_ptr<juce::MidiOutput>`, `startBackgroundThread()`/`stopBackgroundThread()` lifecycle, and per-block `sendBlockOfMessages(buffer, millisecondCounterToStartAt, sampleRate)` dispatch.
- Elapsed-sample timestamping: `millisecondCounterToStartAt` derived from cumulative samples processed since `prepare()`, plus one start reference captured on the message thread.
- `Source/playback/SequencePlayer.h/.cpp` — **`flushPendingNoteOff(StepEventBuffer&)`**, emitted on `stop()`/`reset()`/shutdown at offset 0. JUCE-free, unit-tested. Hard requirement, not a stretch goal.
- `Source/MainComponent.h/.cpp` — device enumeration (`MidiOutput::getAvailableDevices()`), auto-open the first available device on startup, fixed output channel constant (`kMidiChannel`, no UI), and dispatch call in `getNextAudioBlock`. No `ComboBox`/selection UI — mirrors Phase 4's "fixed at construction, no control surface" precedent; user-facing device/channel selection is Phase 7 work.
- Device swap/close discipline: **stop → flush note-off → all-notes-off → swap → restart**, message thread only (relevant to future device-loss/reopen handling even without a UI to trigger it manually this phase).
- Graceful no-device state: zero devices, open failure, or device removal degrade to silent-but-running, never crash or hang a note.
- `Tests/Source/` — suites for `flushPendingNoteOff` and the JUCE-free packing helper.
- `Berlin.jucer` `<FILE>` registration + `Projucer.exe --resave` (6-step checklist).

### Out of Scope (explicit non-goals)

- **Audible output from Berlin itself.** No oscillator/voice; the audio buffer stays cleared — Phase 8.
- **MIDI input**, MIDI-learn, external clock sync, DAW host transport — Phase 11.
- **`.mid` file export**, and per-step velocity/duration fields — Phase 6.
- **Designed UI**: piano roll, transport buttons, BPM control, presets, persisted device preference, and any user-facing device/channel picker — Phase 7. This phase auto-opens the first device and fixes the channel by constant; device/channel choice is not saved between launches.
- **Lock-free hot-swap** of the output device while audio runs. Stop/restart discipline instead; a second concurrency problem is not this phase's scope.
- **Hand-rolled SPSC queue / custom MIDI scheduler thread.** JUCE's `sendBlockOfMessages` background thread is the mechanism (see Resolved Questions).
- **`juce_audio_basics`/`juce_audio_devices` in `Tests/BerlinTests.jucer`.** The harness stays `juce_core`-only; real MIDI I/O is a manual gate by design.
- CMake. Build stays Projucer.

## Capabilities

### New Capabilities

- `midi-output-dispatch`: `StepEvent` → `juce::MidiBuffer` translation, note-on/note-off pairing on the selected channel, fixed velocity, pre-reserved non-allocating buffer, sample-accurate `millisecondCounterToStartAt` from elapsed samples, `sendBlockOfMessages` + background-thread lifecycle, all-notes-quiet on stop/close.
- `midi-device-selection`: enumeration, auto-open first available device by identifier, fixed output channel constant (no user-facing selection this phase), stop→flush→all-notes-off→swap→restart discipline, and behaviour for zero devices / failed open / device disappearing mid-playback.

### Modified Capabilities

- `step-event-scheduling`: **new requirement** — `SequencePlayer` MUST flush a pending note-off on `stop()`/`reset()` so no note can hang. Today `stop()` silently abandons `pendingNote`. `MidiOutputSink` additionally sends CC 123 (All Notes Off) on device close/swap/shutdown as a belt-and-braces guard beyond the app's own tracked note.
- `realtime-audio-wiring`: the "Allocation-Free, Lock-Free, Log-Free Audio Callback" requirement gains one **named, scoped exception** for `sendBlockOfMessages`'s short internal `CriticalSection`; "Output Stays Silent" is narrowed to *audio* output, now that the callback legitimately emits MIDI.

## Resolved Questions

Settled by exploration (`sdd/midi-output-phase5/explore`) — not to be re-litigated:

- **Dispatch mechanism**: `juce::MidiOutput::sendBlockOfMessages` + `startBackgroundThread()`. Rejected: per-event `sendMessageNow` from the audio thread (blocking OS-driver syscall with undocumented worst-case latency; also discards `StepEvent::sampleOffset` entirely, collapsing timing to block granularity). Rejected: a hand-rolled lock-free queue + timer (reimplements shipped JUCE behaviour for no precision gain, disproportionate to this phase's scope).
- **Constitution deviation**: `sendBlockOfMessages` takes a short, bounded, uncontended internal lock on the audio thread. This is a **deliberate, documented, named exception** — recorded in `design.md` exactly as Phase 4 recorded its `realtime-audio-wiring` coverage gap. Neither silently accepted nor sidestepped by overbuilding.
- **Timestamp source**: cumulative elapsed samples since `prepare()`, never a per-block `Time::getMillisecondCounter()` read — the latter reintroduces precisely the drift `Transport` was built to eliminate structurally.
- **Placement**: enumeration, `openDevice`, and channel choice are message-thread/GUI concerns and stay in `MainComponent`-adjacent code. `Source/playback/` stays JUCE-free.
- **Selection surface**: no `ComboBox`/UI this phase — auto-open the first available device, fixed channel constant. Consistent with Phase 4's "BPM fixed at construction, no control surface" precedent; user-facing device/channel selection is Phase 7 work. Not saved between launches (nothing to save yet).
- **Velocity**: fixed constant, `kNoteVelocity = 100`. `Step` has no velocity field (deferred by `core-sequencing-model`); adding one is Phase 6 work.
- **Panic guard**: in addition to `flushPendingNoteOff`'s tracked note-off, send CC 123 (All Notes Off) on device close/swap/shutdown — cheap insurance against a note this app didn't track (e.g. a prior crash leaving hardware in a stuck state).
- **Testability**: only `flushPendingNoteOff` and a JUCE-free packing helper are unit-testable. Device open, dispatch, and the background thread are manual-gate-only — same accepted tradeoff, and same "Known Coverage Gap" spec section, as Phase 4.

## Approach

1. **Debt first, TDD.** `SequencePlayer::flushPendingNoteOff` lands red/green in the `juce_core`-only harness before any JUCE MIDI type is touched. It is the one correctness fix with real test leverage.
2. **Thin, seam-shaped JUCE tier.** A new `Source/midi/` layer holds every `juce_audio_devices`/`juce_audio_basics` type, keeping `Source/playback/` and `Source/core/` untouched and still headless-testable.
3. **RT-safety by construction, exception by declaration.** The `MidiBuffer` is reserved in `prepare`, cleared (not reallocated) per block; no `String`, no `Logger`, no allocation in the callback. The single `sendBlockOfMessages` lock is the only deviation and is written down, not hidden.
4. **Timing inherited, not re-derived.** The dispatch path consumes `StepEvent::sampleOffset` and the transport's elapsed-sample count; it introduces no second clock.
5. **Message-thread device lifecycle.** Open/close/swap happen only on the message thread, always as stop → flush → swap → restart, mirroring Phase 4's fixed-at-construction BPM discipline.
6. **Falsifiable manual gate.** The deliverable is verified against a virtual MIDI port (e.g. loopMIDI) with a monitor capturing note-on/off, channel, and inter-note intervals — an artifact a reviewer can inspect, not "I heard it".

## Affected Areas

| Area | Impact | Description |
|------|--------|-------------|
| `Source/midi/` | New | `MidiEventTranslator`, `MidiOutputSink` — all JUCE MIDI types confined here |
| `Source/playback/SequencePlayer.h/.cpp` | Modified | `flushPendingNoteOff` + `stop()`/`reset()` calling it; stays JUCE-free |
| `Source/MainComponent.h/.cpp` | Modified | Auto-open first device, fixed channel constant, open/close lifecycle, dispatch in `getNextAudioBlock` — no new UI, `resized()` unchanged |
| `Tests/Source/` | New | `SequencePlayerStopTests.cpp`, packing-helper suite |
| `Berlin.jucer` | Modified | New `<GROUP name="midi">` `<FILE>` entries; module list already includes `juce_audio_devices` — **unchanged** |
| `Tests/BerlinTests.jucer` | Modified | `<FILE>` entries only; module list stays `juce_core`-only |
| `Builds/`, `JuceLibraryCode/`, `Tests/…` | Regenerated | `Projucer.exe --resave` |
| `Source/core/`, `Source/generation/`, `Transport`, `StepEventBuffer` | Untouched | Consumed as-is |

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| Stuck/hanging note on stop, quit, or device swap — the most user-visible possible failure | High | `flushPendingNoteOff` is a tested hard requirement; every close/swap path routes through stop → flush → swap → restart; all-notes-quiet asserted at each exit |
| Audio-thread lock via `sendBlockOfMessages` violates the constitution rule | Med | Accepted as a named, bounded, documented exception in `design.md` with rationale and the rejected alternatives; reviewed explicitly, not waived silently |
| Silent reintroduction of audio-thread allocation via a growing `juce::MidiBuffer` | Med | `ensureSize`/reserve in `prepare`, `clear()` per block; explicit review check on the callback body |
| Timing drift from an ad-hoc `Time::getMillisecondCounter()` per block | Med | Elapsed-sample derivation is a spec requirement; any wall-clock sampling in the dispatch path is a review rejection |
| Deliverable rests on "I heard it" and is unfalsifiable | High | Manual gate defined as a captured virtual-port MIDI monitor log with channel + interval assertions |
| Device disappears mid-playback (USB unplug) → crash or hang | Med | Specified degrade-to-silent behaviour with pending note-off flushed; required scenario in the spec |
| Scope creep into per-step velocity/duration, presets, persisted device preference, or Phase 7 UI | Med | Non-goals enumerated and each mapped to a named later phase |
| Pressure to add `juce_audio_devices` to the test harness "so MIDI is testable" | Med | Settled; the test `.jucer` module list must be unchanged in the final diff |

## Rollback Plan

`git revert` the change commits: `Source/midi/` and the new test files disappear wholesale; `Source/playback/SequencePlayer.h/.cpp` lose `flushPendingNoteOff` (purely additive, so the revert restores the exact archived Phase 4 state); `Source/MainComponent.h/.cpp` return to the Phase 4 version (auto-open/dispatch code removed, unchanged). Then restore the `<FILE>` lists in `Berlin.jucer` and `Tests/BerlinTests.jucer` and run `Projucer.exe --resave` on both. Post-rollback the app again runs the transport silently with no MIDI output, and the existing test suite stays green. No persisted state, file format, or external device configuration is written by this change, so nothing survives the revert.

## Dependencies

- Exploration artifact: Engram `sdd/midi-output-phase5/explore` (change folder is `midi-output-routing`; the explore key predates the naming decision).
- `playback-transport` / `step-event-scheduling` / `realtime-audio-wiring` as merged by `2026-09-02-playback-transport-clock`.
- `juce_audio_devices` — already in `Berlin.jucer`; no module addition.
- A virtual MIDI port (loopMIDI or equivalent) plus a MIDI monitor for the manual gate; a real external synth is optional, not required to verify.
- Projucer available headlessly with a resolvable global JUCE modules path.

## Success Criteria

- [ ] `SequencePlayer::stop()` and `reset()` emit a note-off for any sounding note; a test asserts no note-on is left unmatched across start/stop cycles, including stop mid-step and stop with nothing sounding.
- [ ] `MidiOutputSink` sends CC 123 (All Notes Off) on device close/swap/shutdown, independent of and in addition to `flushPendingNoteOff`'s tracked note-off.
- [ ] The app auto-opens the first available MIDI device and dispatches on a fixed channel constant, with no device/channel selection UI present this phase.
- [ ] The JUCE-free packing helper produces correct status byte, channel (1-16), note, and velocity for note-on and note-off, covered by unit tests.
- [ ] `getNextAudioBlock` performs no heap allocation, no `juce::String`/`Logger` call, and no lock other than the single documented `sendBlockOfMessages` acquisition — verified by review against the constitution rule.
- [ ] `millisecondCounterToStartAt` is computed from cumulative elapsed samples; no `Time::getMillisecondCounter()` call exists in the per-block path.
- [ ] Manual gate: with a virtual MIDI port selected, a monitor capture shows note-on/note-off pairs on the chosen channel at intervals matching 120 BPM / 16th notes, over at least one full loop wrap.
- [ ] Changing device or channel while playing produces no stuck note and no crash; playback resumes on the new device/channel.
- [ ] With zero MIDI devices available, or when `openDevice` fails, the app launches and runs silently without crash, assert, or hang.
- [ ] `Tests/BerlinTests.jucer` still links `juce_core` only; its module list is unchanged in the final diff.
- [ ] `BerlinTests.exe --category=Berlin` exits 0 with the previous suites plus the new ones.
- [ ] The audio output buffer is still cleared — Berlin itself remains silent.
