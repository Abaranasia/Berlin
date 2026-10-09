# Design: Internal Synth (roadmap Phase 8)

Artifact store: hybrid. File: `openspec/changes/internal-synth/design.md`. Every JUCE symbol below was read from the pinned 9.0.1 checkout at `H:/Proyectos/Juce/JUCE`, not recalled.

## Verified corrections to the proposal

| Proposal / task brief said | Verified reality | Source |
|---|---|---|
| `juce::dsp::ADSR` | **Does not exist.** ADSR is `juce::ADSR`, in `juce_audio_basics`, not `juce_dsp` | `juce_audio_basics/utilities/juce_ADSR.h:52`; `juce_audio_basics.h:86` |
| `juce::dsp::LFO` | **Does not exist in 9.0.1.** `juce_dsp.h`'s widget includes are Reverb, Bias, Gain, WaveShaper, Oscillator, LadderFilter, Compressor, NoiseGate, Limiter, Phaser, Chorus — no LFO. Must be hand-rolled | `juce_dsp.h:295-305` |
| Tests need `juce_dsp` + `juce_audio_basics` (2 modules) | `juce_dsp` declares `dependencies: juce_audio_formats`. Chain is `juce_dsp` → `juce_audio_formats` → `juce_audio_basics` → `juce_core`. Tests need **three** new modules | `juce_dsp.h:54`, `juce_audio_formats.h:54`, `juce_audio_basics.h:54` |
| — | `Oscillator::processSample` returns `input + generator(...)` — it is **additive**. Pass `0.0f` for the raw waveform | `juce_Oscillator.h:126-131` |
| — | `setFrequency(f)` targets a `SmoothedValue` ramped over **0.05 s** (`frequency.reset(sampleRate, 0.05)`). Pitch modulation must use `setFrequency(f, true)` or the LFO fights the ramp | `juce_Oscillator.h:91-100, 121` |
| — | `initialise(fn, n)` with `n != 0` bakes `fn` into a `LookupTableTransform` — a stateful pulse-width lambda would be frozen. PWM requires `n == 0` | `juce_Oscillator.h:70-87` |
| — | `dsp::Reverb::process` `jassertfalse`s on anything but 1-in/1-out or 2-in/2-out, and its `enabled` member defaults to **true** | `juce_Reverb.h:96-118` |
| — | `setCutoffFrequency` calls `update()` (a `std::tan`). Per-sample cutoff modulation is a transcendental per sample | `juce_StateVariableTPTFilter.h:84, 160` |
| — | `Oscillator::prepare` does `rampBuffer.resize(...)`; `DelayLine::setMaximumDelayInSamples` is documented "never call it from the audio thread"; `StateVariableTPTFilter` holds `std::vector` state. All allocate | `juce_Oscillator.h:110`, `juce_DelayLine.h:122-128`, `juce_StateVariableTPTFilter.h:164` |

## Scoped convention exception (explicit, bounded)

`Source/core/`, `generation/`, `playback/` and the tested half of `export/` are JUCE-free by design. **`Source/synth/` is the one tier permitted to include `<juce_dsp/...>` / `<juce_audio_basics/...>`.** The exception is justified only by "re-deriving oscillator/filter/envelope math is worse than using battle-tested primitives" and is **not** precedent for any other tier. Two guardrails keep it honest: (a) `Source/synth/SynthPatch.h` and `Source/synth/Lfo.h` stay JUCE-free anyway, because their logic is pure arithmetic with no JUCE equivalent; (b) no existing JUCE-free file gains a JUCE include in this change.

## Architecture Decisions

### Decision 1 — Strictly one voice, owned by value; no pool, no stealing

**Choice**: `SynthEngine` owns exactly one `SynthVoice` member by value.
**Alternatives**: `std::array<Voice, N>` pool (the proposal's original shape); `juce::Synthesiser` + `SynthesiserVoice`.
**Rationale**: `SequencePlayer` tracks a single `pendingNote` (`SequencePlayer.cpp:55-66`) and emits note-off **before** note-on at a shared offset, so the stream is already monophonic — a pool would have permanently-idle voices and a voice-stealing policy with no reachable branch. `juce::Synthesiser` is rejected because it wants a `MidiBuffer`, which would make the synth a consumer of `Source/midi/` output rather than a peer consumer of `StepEventBuffer`, coupling two paths the proposal requires to stay independent.

### Decision 2 — Hand-rolled JUCE-free `Lfo`, evaluated at control rate

**Choice**: `berlin::Lfo` — a `double` phase accumulator with `prepare(sampleRate)`, `setRate(hz)`, `advance(int numSamples)`, `getValue()` returning `[-1, 1]`. Modulation is recomputed once per **control block of 32 samples**, subdividing the event-boundary segments.
**Alternatives**: a second `juce::dsp::Oscillator` at sub-audio rate; per-sample modulation.
**Rationale**: `juce::dsp::LFO` does not exist (verified above), so something must be written either way; a JUCE-free 20-line class is directly unit-testable and deterministic. Reusing `dsp::Oscillator` would drag in its 0.05 s frequency smoothing and `rampBuffer` for no benefit. Control-rate is not a compromise: `setCutoffFrequency` costs a `std::tan` per call, so per-sample cutoff modulation would put a transcendental in the inner loop. 32 samples ≈ 1.5 kHz control rate at 48 kHz — far above any audible vibrato/wobble artefact.

### Decision 3 — Pulse width as a live-read generator, lookup table disabled

**Choice**: one `juce::dsp::Oscillator<float>`, re-`initialise`d only when the waveform changes (prepare-time), always with `lookupTableNumPoints = 0`. The pulse generator reads a live member:

```cpp
// phase x arrives in [-pi, pi]; pulseWidth is in (0, 1)
osc.initialise ([this] (float x)
{
    const float edge = juce::MathConstants<float>::pi * (2.0f * pulseWidth - 1.0f);
    return x < edge ? -1.0f : 1.0f;
}, 0);
```

**Alternatives**: four pre-initialised oscillators; a lookup table per pulse-width step.
**Rationale**: the lookup-table path (`juce_Oscillator.h:73-82`) evaluates the function *once* at `initialise` time, so LFO-driven PWM is impossible with `n != 0` — this is the non-obvious constraint that dictates the shape. Consequence to enforce in review: `SynthVoice` captures `this` in the generator, so it **must** be non-copyable and non-movable (`JUCE_DECLARE_NON_COPYABLE`), or a move would leave a dangling generator.

### Decision 4 — Stereo scratch buffer; the synth never sees `startSample`

**Choice**: `SynthEngine` owns `juce::AudioBuffer<float> scratch` sized `(2, samplesPerBlockExpected)`, allocated in `prepare`. The voice renders mono into scratch channels 0 and 1 starting at index **0**; the terminal FX pass runs over a `dsp::AudioBlock` view of the scratch; only the final `addFrom` applies `bufferToFill.startSample`.
**Alternatives**: render straight into `bufferToFill.buffer` with offset arithmetic throughout; a mono scratch.
**Rationale**: `StepEvent::sampleOffset` is relative to **block start**, while `bufferToFill` is indexed from `startSample` — `MidiEventTranslator.cpp:34-35` gets away with passing `sampleOffset` raw because `juce::MidiBuffer` is block-relative. A second consumer writing into a raw `AudioBuffer` does not get that; mixing the two origins in every inner loop is the most likely off-by-`startSample` bug in this change. One origin inside, one translation at the boundary. Stereo (not mono) scratch because `dsp::Reverb::process` asserts on non-1/non-2 channel counts and only `processStereo` yields any width. Because scratch is fixed-size, `render` **clamps** `numSamples` to `scratch.getNumSamples()` (with a `jassert`) rather than resizing — resizing on the audio thread is the failure this design exists to avoid.

### Decision 5 — `prepare` allocates everything; `render` is a pure state machine

**Choice**: `SynthEngine::prepare (const juce::dsp::ProcessSpec&)` is called from `MainComponent::prepareToPlay` and performs *every* allocating call: `scratch.setSize`, `Oscillator::initialise` + `prepare`, `StateVariableTPTFilter::prepare`, `ADSR::setSampleRate` + `setParameters`, `DelayLine::setMaximumDelayInSamples` + `prepare`, `dsp::Reverb::prepare`. `render` calls none of them.
**Rationale**: this is the `realtime-audio-wiring` allocation-free/lock-free/log-free callback constraint applied to the new path, and each of the four listed classes was verified to allocate in `prepare` (table above). `MainComponent::prepareToPlay` currently discards its block size via `juce::ignoreUnused (samplesPerBlockExpected)` (`MainComponent.cpp:103`); that line is deleted, because the size now feeds `ProcessSpec::maximumBlockSize` and the scratch allocation.

### Decision 6 — Two independent `std::atomic<bool>` seams, each with an audio-thread edge latch

**Choice**: `std::atomic<bool> enabled { true }` (synth on/off) and `std::atomic<bool> effectsEnabled { false }` (delay+reverb), written by the message thread from the toggles, read once per `render` with `memory_order_relaxed`. Each has a private audio-thread-only `wasEnabled` / `wasEffectsEnabled` latch: on a true→false edge the owning objects are reset exactly once, then the path is skipped.
**Alternatives**: `dsp::Reverb::setEnabled` / a bypassed `ProcessContext`; no latch.
**Rationale**: `std::atomic<bool>` mirrors `SequencePlayer`'s `std::atomic<int> playhead` — the project's only established cross-thread seam, and the reason no lock appears anywhere on this path. The edge latch is what prevents the two failure modes the proposal calls out: a disabled synth leaving a sustaining envelope (drone), and a re-enabled FX chain replaying a stale reverb/delay tail from minutes earlier. `dsp::Reverb::setEnabled` is rejected as the mechanism because it is a plain `bool` (`juce_Reverb.h:118`) written from another thread and, defaulting to `true`, would silently contradict "bypassed by default"; gating with our own atomic also skips the reverb call entirely rather than paying for a `copyFrom`.

### Decision 7 — Note handling honours the stream literally; note-off is note-matched

**Choice**: in the segment loop, `isNoteOn` sets the voice's pitch then calls `adsr.noteOn()`; `!isNoteOn` calls `adsr.noteOff()` **only if** `event.note == currentNote`. Events are consumed in buffer order, which preserves the off-before-on-at-shared-offset ordering `SequencePlayer` guarantees.
**Rationale**: `ADSR::noteOn` sets `state = attack` without zeroing `envelopeVal` (`juce_ADSR.h:130-146`), so a retrigger ramps up from wherever the release left off — no click, for free, with no crossfade logic. The note-match guard is one comparison and makes a stray/duplicated note-off structurally incapable of cutting a live note. `juce::ADSR`'s documented "call `reset()` if you change parameters mid-release" hazard never fires here because the patch is fixed and set once in `prepare`.

### Decision 8 — `releaseResources` hard-resets; it does not render the flush event

**Choice**: `MainComponent::releaseResources` keeps its existing `player.flushPendingNoteOff` → MIDI path unchanged, and additionally calls `synth.reset()` (envelope idle, oscillator phase zero, filter state zero, delay and reverb cleared, scratch zeroed).
**Rationale**: the flushed note-off exists so external hardware does not hang; the internal synth has no external state to unwind, and no audio callback will run to render a release tail anyway. A hard reset is strictly stronger than replaying the note-off and is the only thing that survives a device restart. `Source/midi/*` is untouched by this — the existing three lines stay byte-identical.

### Decision 9 — UI follows Berlin UI Pattern v1 verbatim

**Choice**: two `juce::ToggleButton` members on `MainComponent`, declared after the domain members, alongside the Phase 7 `exportButton` / `statusLabel`; in-class initialisers carry the labels; the ctor does `addAndMakeVisible` + `onClick = [this]{ ... }` and `setSize` stays the last statement of the UI section; bounds only in `resized()` via `removeFromTop`/`removeFromLeft` off `getLocalBounds().reduced(kMargin)`.
**Alternatives**: a menu item; a single tri-state control combining synth and FX.
**Rationale**: pattern v1 was established by `sdd/midi-export-ui/design` and is the project's UI convention; `ToggleButton` over `TextButton` because both controls are binary state, not actions (the export button is the action case). Two separate toggles because the settled scope makes the synth enable and the FX bypass independent — folding them into one control would make "synth on, FX off" (the default) unreachable in one click. The toggles are the *only* UI in this phase; no parameter controls.

## Data flow

```
                 message thread                    audio thread
                 ──────────────                    ────────────
  synthToggle ──> std::atomic<bool> enabled ─────────────┐
  fxToggle    ──> std::atomic<bool> effectsEnabled ──┐   │
                                                     │   v
  getNextAudioBlock:
    bufferToFill.clearActiveBufferRegion()
    player.process (numSamples, blockEvents) ──> StepEventBuffer (fixed cap 64)
                                                   │            │
                          ┌────────────────────────┘            │  (peer consumer,
                          v                                     v   read-only)
              midiTranslator.translate ──> midiBlock    synth.render (blockEvents,
                          │                                      *buffer, startSample,
                          v                                       numSamples)
                    midiSink.dispatch                              │
                    (external MIDI out)                            v
                                                    ┌──────────────────────────────┐
                                                    │ segment loop, origin 0       │
                                                    │  split at event offsets,     │
                                                    │  then per 32-sample control  │
                                                    │  block:                      │
                                                    │   Lfo.advance/getValue       │
                                                    │    -> pitch (setFrequency    │
                                                    │        force=true)           │
                                                    │    -> cutoff, amp, pulseWidth│
                                                    │   Oscillator.processSample(0)│
                                                    │    -> SVTPT lowpass          │
                                                    │    -> * ADSR.getNextSample() │
                                                    │    -> scratch[0] & scratch[1]│
                                                    └──────────────┬───────────────┘
                                                                   v
                                          effectsEnabled ? DelayLine -> dsp::Reverb : (dry)
                                                                   v
                                       bufferToFill.buffer->addFrom (ch, startSample,
                                                                     scratch, ch, 0, n)
```

`Source/midi/*` and `Source/export/*` appear here only as the untouched left branch; the synth reads `blockEvents` and mutates nothing the MIDI path depends on.

## File changes

| File | Action | Description |
|---|---|---|
| `Source/synth/SynthPatch.h` | Create | JUCE-free POD: waveform enum, cutoff/resonance, ADSR times, LFO rate/depth/destination enum, delay/reverb params, output level. One `constexpr` default patch |
| `Source/synth/Lfo.h` / `.cpp` | Create | JUCE-free phase accumulator; `prepare`/`setRate`/`advance`/`getValue` |
| `Source/synth/SynthVoice.h` / `.cpp` | Create | `dsp::Oscillator` + `dsp::StateVariableTPTFilter` + `juce::ADSR`. Non-copyable, non-movable (Decision 3) |
| `Source/synth/SynthEffects.h` / `.cpp` | Create | Terminal `dsp::DelayLine` + `dsp::Reverb` pass over a stereo `dsp::AudioBlock` |
| `Source/synth/SynthEngine.h` / `.cpp` | Create | Owns voice, LFO, effects, scratch, the two atomics and their edge latches; `prepare`/`render`/`reset` |
| `Source/MainComponent.h` | Modify | `berlin::SynthEngine synth;` + two `juce::ToggleButton` members; `#include "synth/SynthEngine.h"` |
| `Source/MainComponent.cpp` | Modify | ctor toggle wiring + `resized()` rows; `prepareToPlay` builds `ProcessSpec` (drop `ignoreUnused`); one `synth.render(...)` line in `getNextAudioBlock`; one `synth.reset()` in `releaseResources` |
| `Berlin.jucer` | Modify | `<MODULE id="juce_dsp" .../>` + `<MODULEPATH id="juce_dsp" .../>`; `<GROUP name="synth">` with `<FILE>` rows |
| `JuceLibraryCode/`, `Builds/VisualStudio2026/` | Regenerate | Projucer output (see below) |
| `Tests/BerlinTests.jucer` | Modify | Three new `<MODULE>`/`<MODULEPATH>` entries; synth `<GROUP>`; new test `<FILE>` rows |
| `Tests/JuceLibraryCode/`, `Tests/Builds/VisualStudio2026/` | Regenerate | Projucer output (see below) |
| `Tests/Source/DspLinkSmokeTests.cpp`, `LfoTests.cpp`, `SynthVoiceTests.cpp`, `SynthEngineTests.cpp` | Create | See Testing Strategy |
| `Source/core/`, `generation/`, `playback/`, `midi/`, `export/` | Untouched | Must be byte-for-byte identical in the final diff |

## Module / build mechanics, and why PR #1 is isolated

**`Berlin.jucer`** — one module. Projucer writes `<MODULES>` alphabetically, so `<MODULE id="juce_dsp" showAllCode="1" useLocalCopy="0" useGlobalPath="1"/>` lands between `juce_data_structures` and `juce_events`, with a sibling-identical `<MODULEPATH id="juce_dsp" path="../../Projucer/modules"/>`. `juce_audio_basics` and `juce_audio_formats` are already present (`Berlin.jucer:49,51`), so the dependency chain is already satisfied here. Resave emits `JuceLibraryCode/include_juce_dsp.cpp` + `.mm`, rewrites `JuceLibraryCode/JuceHeader.h`, and rewrites `Builds/VisualStudio2026/Berlin.vcxproj` + `.vcxproj.filters`.

**`Tests/BerlinTests.jucer`** — **three** modules, not one and not two: `juce_audio_basics`, `juce_audio_formats`, `juce_dsp`, each with `<MODULEPATH ... path="../../../Projucer/modules"/>` matching the existing `juce_core` row (`Tests/BerlinTests.jucer:67`). `juce_audio_formats` is non-optional because `juce_dsp` declares it as its dependency (Convolution loads impulse responses through it); omitting it is the most likely way this slice fails at link time. Resave grows `Tests/JuceLibraryCode/` from 6 files to roughly a dozen, including `include_juce_audio_formats_flac_1.c` and `_flac_2.c` — a real, if inert, compile-time cost on the test target that reviewers should see attributed to the module change and not to synth logic. `useGlobalPath="1"` and `useAppConfig="0"` are preserved on every new row so the harness keeps resolving modules through the same global path as today (`H:/Proyectos/Juce/Projucer/modules/juce_dsp/` confirmed present).

**Isolation**: both regens land together in **PR #1**, with nothing but a `DspLinkSmokeTests.cpp` depending on them. That PR is almost entirely generated churn and can be reviewed as "did the module list change correctly, does it still build green" without any DSP reasoning. PRs #2–#4 then add only `<FILE>` rows and the corresponding `ClCompile`/`ClInclude` lines — small, readable resaves. Sequencing matters in the other direction too: `Source/synth/` cannot compile before PR #1, and the eight-phase-old `juce_core`-only harness gets its own green-build checkpoint before any synth code relies on it.

## Interfaces

```cpp
namespace berlin
{
class SynthEngine
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec);          // message thread; allocates
    void render  (const StepEventBuffer& events,                // audio thread; allocation-,
                  juce::AudioBuffer<float>& destination,        //   lock- and log-free
                  int startSample, int numSamples) noexcept;
    void reset() noexcept;                                      // hard silence, no tail

    void setEnabled (bool) noexcept;                            // message thread -> atomic
    void setEffectsEnabled (bool) noexcept;                     // message thread -> atomic
};
}
```

`StepEvent` is consumed unchanged (`{ sampleOffset, stepIndex, note, isNoteOn }`); no velocity field is added, per the proposal's deferral. Pitch comes from `juce::MidiMessage::getMidiNoteInHertz (event.note)`, available via `juce_audio_basics`.

## Testing Strategy

| Layer | What to test | Approach |
|---|---|---|
| Build | `juce_dsp` actually links into the test target | `DspLinkSmokeTests.cpp` — construct + `prepare` a `dsp::Oscillator<float>`, assert `processSample(0.0f)` is finite. Lands with PR #1, before any Berlin symbol depends on the module |
| Unit | `Lfo` phase math | JUCE-free: exact period at a known rate, range stays in `[-1, 1]`, `advance(n)` equals `n` × `advance(1)` |
| Unit | `SynthVoice` | Offline block render: silent while idle; non-silent after `noteOn`; envelope non-decreasing through attack; returns to exact silence after release completes; output bounded (gain-staging guard) |
| Unit | `SynthEngine::render` | Synthetic `StepEventBuffer`: first non-zero sample lands at `startSample + sampleOffset`; `setEnabled(false)` leaves `destination` bit-identical; `reset()` silences a sounding voice; note-off with a mismatched note does not cut the voice; `numSamples` beyond the prepared block size is clamped, never resized |
| Regression | Existing 17 suites under the expanded module list | `BerlinTests.exe --category=Berlin` exits 0 |
| Review | RT-safety | Line-by-line confirmation that `render` contains no allocation, lock, logging, or `prepare`-family call — the same gate `realtime-audio-wiring` already applies, now covering `Source/synth/` |
| Manual gate (human) | Audibility | Sound with no MIDI device attached; each waveform; filter + resonance sweep; ADSR stages; each of the four LFO destinations; delay and reverb audible and cleanly bypassable; no drone after stop or device restart; synth toggle off removes doubling against an external MIDI synth |

`MainComponent` itself stays outside automated reach (it needs `juce_gui_basics`, which the harness still will not have) — that is the same accepted gap as `MidiOutputSink` and `midi-export-ui`, and is deliberately **not** widened here.

## Threat Matrix

N/A — no routing, shell command, subprocess, VCS/PR automation, executable-file classification, or process-integration boundary. The adjacent hazard is real-time safety on the audio thread, addressed by Decisions 4, 5 and 6 and gated by the review row above rather than by a threat-matrix row.

## Migration / Rollout

No data migration, no persisted state, no file-format change; exported `.mid` files are unaffected because `Source/export/` is untouched. Four chained PRs:

1. Both `.jucer` module changes + both regens + `DspLinkSmokeTests.cpp`. Green build on the expanded harness.
2. `SynthPatch`, `SynthVoice` (oscillator + filter + ADSR) + unit tests. Unwired — `MainComponent` unchanged, so the app still renders silence.
3. `SynthEngine` + `MainComponent` wiring + the synth enable toggle. **First audible sound**; a valid stopping point on its own.
4. `Lfo` + its four destinations, `SynthEffects` (delay + reverb), the FX toggle. Effects bypassed by default, so merging this changes nothing until the user toggles it.

Rollback is `git revert` in reverse order; slices 2–4 are source-only, and reverting slice 1 requires restoring both `.jucer` files and running `Projucer.exe --resave` on each.

## Open Questions

None blocking. Resolved during design: `juce::dsp::ADSR`/`LFO` do not exist (substitutes chosen); the tests need three modules rather than two; PWM forces `lookupTableNumPoints = 0`; scratch-buffer indexing removes the `startSample` hazard; FX bypass uses our own atomic rather than `dsp::Reverb::setEnabled`. Deferred by the proposal's non-goals and untouched here: velocity in `StepEvent`, band-limited oscillators, any synth parameter UI or preset system.
