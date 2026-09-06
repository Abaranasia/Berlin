# Design: Parameter Controls (roadmap Phase 9)

Artifact store: hybrid. Engram: `sdd/parameter-controls/design`. Every JUCE symbol and every numeric bound below was read from the pinned 9.0.1 checkout at `H:/Proyectos/Juce/JUCE`, not recalled.

## Verified findings that shaped this design

| Assumption | Verified reality | Source |
|---|---|---|
| High resonance can self-oscillate / blow up | It **cannot**. `h = 1/(1 + R2·g + g²)` with `R2 = 1/resonance > 0` is finite and positive for every finite `resonance` — the TPT SVF is unconditionally stable. The real hazard is peak gain ≈ `resonance` (it *is* Q), i.e. clipping, not divergence | `juce_StateVariableTPTFilter.cpp:132-137` |
| Resonance may be any float | `jassert (newResonance > 0)`; `setCutoffFrequency` asserts `isPositiveAndBelow (f, sampleRate * 0.5)`. Both are reachable from a slider, so clamping is a correctness requirement, not polish | `juce_StateVariableTPTFilter.cpp:54, 63` |
| `adsr.setParameters()` is a harmless idempotent write | **It is not.** It calls `recalculateRates()`, which overwrites `releaseRate` with `sustain / (release·sr)` — discarding the `envelopeVal`-derived rate `noteOff()` computed. Calling it unconditionally every control block would silently change the release contour of every note released before reaching sustain. It can also call `goToNextState()` → `reset()` mid-release | `juce_ADSR.h:92-99, 155, 262-279` |
| A `std::atomic<SynthPatch>` would be a tidy single seam | 19 floats ≈ 76 bytes → not lock-free → the implementation takes an internal lock. Forbidden on the audio thread | `<atomic>` lock-free rules |
| `Label::attachToComponent (c, true)` auto-places a name label | Its width is `jmin (textWidth, component.getX())` — a control near a column edge silently truncates its label to nothing | `juce_Label.cpp:175-182` |

## Architecture Decisions

### Decision 1 — One generator lambda for all four waveforms, `initialise` called exactly once

**Choice**: delete `applyWaveform()`. `prepare()` installs a single `[this]` lambda with `lookupTableNumPoints = 0` that switches on a live `waveform` member every sample — the technique Phase 8 already proved for `pulse` (Decision 3), now the norm.

```cpp
oscillator.initialise ([this] (float x) noexcept
{
    switch (waveform)
    {
        case Waveform::saw:      return x / juce::MathConstants<float>::pi;
        case Waveform::square:   return x < 0.0f ? -1.0f : 1.0f;
        case Waveform::triangle: return (2.0f / juce::MathConstants<float>::pi) * std::abs (x) - 1.0f;
        case Waveform::pulse:    return x < juce::MathConstants<float>::pi * (2.0f * pulseWidth - 1.0f) ? -1.0f : 1.0f;
    }
    return 0.0f;
}, 0);
```

**Alternatives**: re-`initialise` on change from a message-thread hop; four pre-initialised oscillators swapped by pointer.
**Rationale**: switching waveform becomes a plain member assignment, so allocation on switch is *structurally impossible* rather than merely avoided — the strongest possible answer to the "Allocation-Free Voice Rendering" delta. Four oscillators would need four `rampBuffer`s and a phase-continuity story across the swap. Cost: one predictable branch per sample on a single monophonic voice.
**Accepted consequence**: table-free is *sharper* than the 128-point table (which linearly interpolated the square/pulse discontinuity across one 2π/127 cell), so the top of the spectrum gets slightly brighter. Bounded by the equivalence test below.

### Decision 2 — Atomics live on `SynthVoice`, `SynthEngine` forwards

**Choice**: a nested `struct Parameters` of `std::atomic` members on `SynthVoice` (message thread writes, audio thread reads, `memory_order_relaxed`), copied into the existing plain audio-thread-only members once per 32-sample control block by a new `applyParameters()`. `SynthEngine` gains ten thin same-named forwarders so `MainComponent` still talks only to `synth`.

**Alternatives**: atomics on `SynthEngine` pushed down each block (a second full mirror of every field); one `std::atomic<SynthPatch>` (not lock-free — see table); a `SpinLock`-guarded patch.
**Rationale**: state lives where it is consumed, which is exactly why `enabled`/`effectsEnabled` live on the engine — they gate engine behaviour, these gate voice DSP. The nested struct (rather than ten flat members) keeps the cross-thread seam visually distinct from the audio-thread-only members it feeds. Continuous parameters get **no edge latch**: latches exist for booleans with reset semantics, and no reset is owed here.
**Guardrail**: `static_assert (std::atomic<float>::is_always_lock_free)` and the same for `std::atomic<Waveform>` / `std::atomic<LfoDestination>`, in `SynthVoice.h`.
**Accepted consequence**: fields are independently atomic, so one control block may observe a mix of old and new values across fields for ≤32 samples (≤0.73 ms). No cross-field invariant exists, so this is harmless.

### Decision 3 — Base-value-then-delta re-apply every control block (closes the Phase 8 latent bug)

**Choice**: `updateLfoModulation()` stops being a switch that touches only the active destination. It now always recomputes all four targets from their base values, adds the LFO delta only for the active destination, then commits:

```
frequency  = baseFrequencyHz;  cutoff = baseCutoffHz;
pulseWidth = basePulseWidth;   amplitudeLfoGain = 1.0f;
switch (lfoDestination) { ... add delta to exactly one ... }
oscillator.setFrequency (frequency, true);
if (cutoff != lastAppliedCutoffHz) { filter.setCutoffFrequency (jlimit (kMinCutoffHz, maxCutoffHz, cutoff)); lastAppliedCutoffHz = cutoff; }
```

**Alternatives**: restore the previous destination on the switch edge (needs a `previousDestination` latch and a per-destination undo branch); reset the voice on destination change (audible).
**Rationale**: `oscillator.setFrequency` and `filter.setCutoffFrequency` are stateful, so today switching away from pitch or cutoff mid-note parks them at the last modulated value forever. Unconditional re-application is stateless-by-construction — there is no edge to miss and no undo to get wrong. It costs nothing extra, because the user's cutoff and pitch values have to be written every control block anyway now that they are live. The `lastAppliedCutoffHz` guard is what keeps the `std::tan` inside `update()` off the path when nothing moved.

### Decision 4 — `adsr.setParameters()` is gated on an actual change

**Choice**: `applyParameters()` compares the four loaded floats against the cached `juce::ADSR::Parameters` and calls `setParameters` only on a real difference.

**Alternatives**: call it unconditionally every control block; call it once per `render()`.
**Rationale**: this is a **correctness** gate, not a performance nicety. `recalculateRates()` recomputes `releaseRate` from `sustain`, discarding the `envelopeVal`-derived rate `noteOff()` set — so a note released during its attack would have its release silently accelerated by a call the user never made. Gating means `recalculateRates()` runs only when the user actually moves a slider, which also confines the one genuine hazard (`sustain = 0` during release trips `releaseRate <= 0` → `goToNextState()` → `reset()`, an instant cut) to a deliberate user action rather than every block.

### Decision 5 — Ranges, tapers and clamps; `pulseWidth` gets a slider

All bounds are `inline constexpr` in `SynthPatch.h` (JUCE-free, `<algorithm>` only). Setters clamp on the message thread; the voice additionally clamps cutoff to the sample-rate-dependent `maxCutoffHz = 0.49 × sr`, which a fixed slider range cannot know.

| Parameter | Range | Default | Taper | Why this bound |
|---|---|---|---|---|
| `cutoffHz` | 20 – 20000 Hz | 4000 | `setSkewFactorFromMidPoint (1000.0)` | Existing `kMinCutoffHz`; 20 kHz is audible-band top and always ≤ `0.49·sr` at 44.1 kHz. Log feel: linear puts the whole musical range in the bottom 5 % of travel |
| `resonance` | 0.7071068 – 8.0 | 0.7071068 | `setSkewFactorFromMidPoint (2.0)` | Min is the documented 12 dB/oct Butterworth point and satisfies `jassert (> 0)`. Max 8 because `resonance` **is** Q: peak gain ≈ 8 (≈18 dB). The filter cannot diverge (verified above), so the cap is a loudness choice; 8 keeps worst case (cutoff parked on the fundamental) at ≈5× full scale — hot and clipping, like any resonant hardware filter, but finite and NaN-free |
| `pulseWidth` | 0.05 – 0.95 | 0.5 | linear | Existing `kMinPulseWidth`/`kMaxPulseWidth`; the extremes are already where the LFO destination is allowed to push it |
| `attack` | 0.001 – 4.0 s | 0.01 | `setSkewFactorFromMidPoint (0.2)` | 1 ms ≈ 44 samples — instant to the ear, but strictly > 0 so `recalculateRates()` cannot take the `attackRate <= 0` branch mid-attack |
| `decay` | 0.001 – 4.0 s | 0.15 | `setSkewFactorFromMidPoint (0.3)` | Same floor rationale; 4 s covers long pad decays |
| `sustain` | 0.0 – 1.0 | 0.7 | linear | Natural level. 0 is kept reachable because plucks need it; see Decision 4 for the release-stage edge |
| `release` | 0.005 – 8.0 s | 0.2 | `setSkewFactorFromMidPoint (0.5)` | Floor is 5 ms ≈ 220 samples: `release = 0` makes `noteOff()` call `reset()` — a hard cut and a guaranteed click |
| `lfoRateHz` | 0.05 – 20.0 Hz | 4.0 | `setSkewFactorFromMidPoint (2.0)` | 0.05 Hz = a 20 s sweep; 20 Hz is the audible-modulation ceiling and stays far below the control-rate Nyquist (44100 / 32 / 2 ≈ 689 Hz), so the modulator itself never aliases. The positive floor also kills the current unclamped negative-rate phase reversal |
| `lfoDepth` | 0.0 – 1.0 | 0.0 | linear | Already the assumed normalised depth; 0 keeps the shipped default LFO-off |

**`pulseWidth` gets a dedicated slider — yes.** `SynthVoiceTests.cpp:169-173` states it outright: *"A 50%-duty pulse is byte-identical to the square generator."* Shipping a waveform selector containing both `square` and a permanently-50 % `pulse` ships a duplicate entry. It also costs zero DSP (the field, its clamps and its live read already exist) and it is the base value the `pulseWidth` LFO destination modulates *around* — without it that destination can only ever wobble about 0.5.

**No smoothing / no `SmoothedValue`.** Instant, per the settled decision, and each parameter has its own reason rather than a blanket claim: the SVF is documented as *"designed for fast modulation"* and the Phase 8 LFO already sweeps its cutoff ±2 octaves at this exact 32-sample cadence, far faster than any drag; resonance and ADSR change coefficients/slopes, not signal values; LFO rate changes a phase increment, not a phase. The only two genuine output-value steps are a waveform switch and `pulseWidth`/`lfoDepth` jumps — both already occur at this cadence today via the LFO. If the manual gate hears artefacts, ramping becomes a follow-up, not a silent addition here.

### Decision 6 — `MainComponent` re-pushes every parameter after `prepare()`

**Choice**: a private `pushAllParametersToSynth()` called at the end of `prepareToPlay`, immediately after `synth.prepare (spec)`.
**Rationale**: `SynthEngine::prepare` seeds the voice from `kDefaultPatch`, and the `juce-app-dev` contract is explicit that a device/sample-rate/block-size change re-triggers `prepareToPlay`. Without this, changing the audio device would silently snap every parameter back to default while the sliders still showed the user's values. Alternatives rejected: a `parametersSeeded` flag hidden inside `SynthVoice` (invisible to the caller, and would break the existing tests that rely on `prepare(spec, patch)` taking effect), or a second `prepareKeepingParameters` entry point. The UI is the source of truth for live parameters, so the re-push belongs at the UI boundary — and it is the natural hook for a future preset load.
**Hard ordering constraint**: `setAudioChannels()` can invoke `prepareToPlay` synchronously, so every control must be constructed and given its value *before* that call — i.e. in the existing UI block, above `setSize`.

### Decision 7 — Two columns and section labels inside the existing `resized()` idiom

**Choice**: no new layout primitive. `resized()` keeps `getLocalBounds().reduced (kMargin)` and `removeFromTop`/`removeFromLeft`, gains one local lambda, and splits the parameter area into two columns:

```cpp
auto placeLabelled = [] (juce::Rectangle<int>& column, juce::Component& label, juce::Component& control)
{
    auto row = column.removeFromTop (kControlHeight);
    label  .setBounds (row.removeFromLeft (kLabelWidth));   // kLabelWidth = 96
    control.setBounds (row);
    column .removeFromTop (kMargin / 2);
};
```

Header rows keep their order (`exportButton`, `statusLabel`, then `synthToggle` + `fxToggle` **merged onto one shared row** — the only change to existing layout, freeing 34 px and reading better). Then `auto left = area.removeFromLeft (area.getWidth() / 2 - kMargin / 2);` with `right = area`.

- **Left column** — `OSCILLATOR`: waveform, pulse width. `FILTER`: cutoff, resonance. (6 rows)
- **Right column** — `ENVELOPE`: attack, decay, sustain, release. `LFO`: destination, rate, depth. (9 rows)

**Budget check**: header 120 px + 9 × (28 + 6) = 426 px of the 588 usable in an 800×600 window — ~160 px spare, and each column is 382 px wide (96 label + ~230 slider + 56 value box). The proposal's "10 controls overwhelm the flat layout" risk is closed by arithmetic, not by hope.
**Alternatives**: `Label::attachToComponent (s, true)` — rejected, its width clamps to the owner's `x` and truncates near a column edge (verified above); tabs or a `Viewport` — a new primitive for a layout that demonstrably fits.
**Berlin UI Pattern v1 is preserved verbatim**: in-class member initialisers, `addAndMakeVisible` + `[this]` lambda in the ctor, bounds only in `resized()`. A ctor-local `configureSlider (slider, label, name, min, max, initial, midPoint)` lambda absorbs the repetition so each control is two lines. **Every initial value is read from `berlin::kDefaultPatch`**, so UI defaults and atomic defaults cannot drift, and `setValue (v, juce::dontSendNotification)` keeps the ctor from firing setters. `ComboBox` item IDs are `static_cast<int> (e) + 1` (JUCE forbids id 0) and map back with `static_cast<E> (box.getSelectedId() - 1)`.

## Data flow

```
      message thread                                    audio thread
      ──────────────                                    ────────────
  waveformBox / 9 sliders / lfoDestinationBox
        │  onValueChange / onChange
        v
  synth.setCutoffHz(v) ... (10 forwarders)
        │  clamp to [kMin.., kMax..]
        v
  SynthVoice::Parameters { std::atomic<float> x9, atomic<Waveform>, atomic<LfoDestination> }
        │                                        relaxed load, once per control block
        └──────────────────────────────────────────────┐
                                                       v
   SynthVoice::render, per 32-sample control block:
     applyParameters()   waveform / basePulseWidth / baseCutoffHz / resonance / lfo rate+depth+dest
        │                 filter.setResonance  (guarded on change)
        │                 adsr.setParameters   (guarded on change - Decision 4)
        v
     updateLfoModulation()   base values re-applied ALWAYS, delta added for the
        │                     active destination only  (Decision 3)
        v
     per sample: oscillator.processSample(0)   <- single 4-way generator (Decision 1)
                   -> SVTPT lowpass -> * adsr.getNextSample() -> [* amplitudeLfoGain]
```

`SynthEngine::render`'s event loop, scratch buffer, `enabled`/`effectsEnabled` latches and the whole `Source/midi/` + `Source/export/` branch are untouched.

## File changes

| File | Action | Description |
|---|---|---|
| `Source/synth/SynthPatch.h` | Modify | Nine `inline constexpr` min/max pairs + a `constexpr` clamp helper (`<algorithm>`; stays JUCE-free). Header comment's "fixed / no parameter UI" wording replaced |
| `Source/synth/SynthVoice.h` | Modify | Nested `struct Parameters` of atomics + `static_assert`s; ten message-thread setters; `applyParameters()`; `lastAppliedCutoffHz`, cached `ADSR::Parameters`, live `Waveform waveform` member; `applyWaveform()` removed |
| `Source/synth/SynthVoice.cpp` | Modify | Single 4-way generator (Decision 1); `prepare()` seeds + clamps the atomics and installs the generator once; `applyParameters()` at the head of each control block; `updateLfoModulation()` rewritten base-then-delta; local `kMinCutoffHz`/`kMinPulseWidth`/`kMaxPulseWidth` deleted in favour of the `SynthPatch.h` constants |
| `Source/synth/SynthEngine.h/.cpp` | Modify | Ten thin `voice.setX(v)` forwarders alongside `setEnabled`/`setEffectsEnabled`. No new state |
| `Source/MainComponent.h` | Modify | 10 controls (2 `ComboBox`, 8 `Slider`) + 10 name `Label`s + 4 section `Label`s; `pushAllParametersToSynth()` declaration |
| `Source/MainComponent.cpp` | Modify | `configureSlider` ctor lambda + 10 callbacks; two-column `resized()` with `placeLabelled`; `pushAllParametersToSynth()` called from `prepareToPlay` |
| `Tests/Source/SynthVoiceTests.cpp` | Modify | Table-free equivalence, live/mid-note, LFO-repark and clamp suites (below) |
| `Tests/Source/SynthEngineTests.cpp` | Modify | Forwarder coverage through `render` |
| `Berlin.jucer`, `Tests/BerlinTests.jucer`, both `JuceLibraryCode/`, both `Builds/` | **Untouched** | No new source or test files → **no Projucer regen anywhere in this change**. Deliberate: the diff stays 100 % authored, zero generated churn |
| `Source/core/`, `generation/`, `playback/`, `midi/`, `export/`, `Source/synth/Lfo.*`, `SynthEffects.*` | Untouched | Must be byte-for-byte identical in the final diff |

## Interfaces

```cpp
namespace berlin
{
inline constexpr float kMinCutoffHz = 20.0f,        kMaxCutoffHz = 20000.0f;
inline constexpr float kMinResonance = 0.7071068f,  kMaxResonance = 8.0f;
inline constexpr float kMinPulseWidth = 0.05f,      kMaxPulseWidth = 0.95f;
inline constexpr float kMinAttackSeconds = 0.001f,  kMaxAttackSeconds = 4.0f;
inline constexpr float kMinDecaySeconds = 0.001f,   kMaxDecaySeconds = 4.0f;
inline constexpr float kMinSustain = 0.0f,          kMaxSustain = 1.0f;
inline constexpr float kMinReleaseSeconds = 0.005f, kMaxReleaseSeconds = 8.0f;
inline constexpr float kMinLfoRateHz = 0.05f,       kMaxLfoRateHz = 20.0f;
inline constexpr float kMinLfoDepth = 0.0f,         kMaxLfoDepth = 1.0f;

constexpr float clampParameter (float v, float lo, float hi) noexcept { return std::clamp (v, lo, hi); }

class SynthVoice
{
public:
    // ---- MESSAGE THREAD -> atomic; clamped here. Safe to call at any time. ----
    void setWaveform       (Waveform) noexcept;   void setCutoffHz  (float) noexcept;
    void setResonance      (float) noexcept;      void setPulseWidth (float) noexcept;
    void setAttackSeconds  (float) noexcept;      void setDecaySeconds (float) noexcept;
    void setSustain        (float) noexcept;      void setReleaseSeconds (float) noexcept;
    void setLfoRateHz      (float) noexcept;      void setLfoDepth  (float) noexcept;
    void setLfoDestination (LfoDestination) noexcept;
private:
    struct Parameters      // message thread writes, audio thread reads (relaxed)
    {
        std::atomic<Waveform> waveform { kDefaultPatch.waveform };
        std::atomic<float>    cutoffHz { kDefaultPatch.cutoffHz };
        std::atomic<float>    resonance { kDefaultPatch.resonance };
        std::atomic<float>    pulseWidth { kDefaultPatch.pulseWidth };
        std::atomic<float>    attack { kDefaultPatch.attack };
        std::atomic<float>    decay { kDefaultPatch.decay };
        std::atomic<float>    sustain { kDefaultPatch.sustain };
        std::atomic<float>    release { kDefaultPatch.release };
        std::atomic<float>    lfoRateHz { kDefaultPatch.lfoRateHz };
        std::atomic<float>    lfoDepth { kDefaultPatch.lfoDepth };
        std::atomic<LfoDestination> lfoDestination { kDefaultPatch.lfoDestination };
    };
    Parameters target;
    void applyParameters() noexcept;   // audio thread; head of every control block
};
}
```

`SynthEngine` mirrors all eleven setter signatures verbatim as one-line forwarders. `SynthVoice::prepare (spec, patch)` keeps its exact signature and semantics (it seeds `target` from `patch`, clamped) so every existing test compiles and passes unchanged.

## Testing Strategy

| Layer | What to test | Approach |
|---|---|---|
| Unit | **Table-free equivalence** (the highest-risk item) | In `SynthVoiceTests.cpp`, build a reference `juce::dsp::Oscillator<float>` with the *old* 128-point table lambda; render one steady-state period of each of saw/square/triangle from both it and a `SynthVoice` (cutoff at max, attack/decay 0, sustain 1); naive DFT of harmonics 1–8 must match within 3 % relative, and RMS within 3 %. Deliberately not sample-exact: the table linearly interpolated discontinuities, so only the very top of the spectrum may differ |
| Unit | Waveform switch mid-note | Render on `saw`, `setWaveform(square)`, render again; second half matches a reference square voice's steady state within the filter-transient tolerance; every sample finite |
| Unit | Live cutoff / resonance mid-note | Reuse the existing `brightnessProxy` / `peakAmplitude` metrics, applied to the second half of one sustained note instead of two separately-prepared voices |
| Unit | Live ADSR mid-note | Change `release` while sustaining, then `noteOff`; measured tail length matches the **new** release time (±1 control block). Also asserts the Decision 4 gate does not suppress a real change |
| Unit | **LFO destination re-park** (closes the Phase 8 bug) | `cutoff` destination at depth 1.0, render until the LFO is off-centre, switch to `amplitude`; brightness must return to a never-modulated reference voice's baseline. Repeat for `pitch` via zero-crossing period count returning to `baseFrequencyHz` |
| Unit | Clamping / assert-freedom | Drive every setter to ±extremes (`1e9`, `-100`, `0`, `NaN`-adjacent); render; all samples finite and non-NaN. In a Debug test build an unclamped value would trip JUCE's own `jassert` in `setResonance`/`setCutoffFrequency`, so this suite *is* the clamp test |
| Unit | `SynthEngine` forwarders | Each `synth.setX()` produces the same rendered output as calling the voice setter directly |
| Regression | All existing suites | `BerlinTests.exe --category=Berlin` exits 0. Verified compatible: the tests that pass `attack = 0.0f` / `decay = 0.0f` now clamp to 1 ms, but every such test discards ≥441 settle samples before measuring, so outcomes are unchanged |
| Review | RT-safety | Line-by-line confirmation that `applyParameters`, `updateLfoModulation` and the generator contain no allocation, lock, logging or `prepare`-family call. `initialise` appears exactly once, in `prepare` |
| Manual gate (human) | Audibility | Drag every control while a note sustains: audible immediately, no zipper, no click. Switch waveform mid-note: no dropout. Switch LFO destination mid-note: nothing parked. Saw/square/triangle still sound like Phase 8. Restart the audio device: sliders and sound stay in sync (Decision 6) |

`MainComponent` stays outside automated reach (it needs `juce_gui_basics`, which the harness still lacks) — the same accepted gap as Phase 7 and 8, deliberately not widened.

## Threat Matrix

N/A — no routing, shell command, subprocess, VCS/PR automation, executable-file classification, or process-integration boundary. The adjacent hazard is real-time safety on the audio thread, addressed by Decisions 1, 2 and 4 and gated by the review row above.

## Migration / Rollout

No migration. No persisted state (presets are a later phase — every launch starts from `kDefaultPatch`). No file-format change; `.mid` export is byte-for-byte unaffected. Single PR per the cached `single-pr` strategy, with the **table-free generator landing as the first commit inside it**, ahead of any UI wiring, so the equivalence test gates the DSP rewrite before ten controls are attached to it. Rollback is `git revert` of the single commit; nothing generated needs restoring because no `.jucer` is touched.

## Open Questions

None blocking. Resolved here, with evidence: the SVF cannot self-oscillate (so the resonance cap is a loudness choice, set at 8.0); `adsr.setParameters()` must be change-gated for correctness, not performance; `pulseWidth` gets a slider because `pulse` at 50 % is byte-identical to `square`; all nine ranges and tapers are pinned above; `attachToComponent` is unusable for left-side labels here. Deferred by the proposal's non-goals: preset persistence, delay/reverb/`outputLevel` UI, band-limited oscillators, polyphony.

*Deviation note: this artifact exceeds the sdd-design 800-word budget. It follows the established Phase 8 `design.md` structure the orchestrator specified, and carries six numeric/architecture decisions this phase was explicitly told to own and evidence.*
