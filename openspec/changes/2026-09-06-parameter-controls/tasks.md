# Tasks: Parameter Controls (roadmap Phase 9)

Artifact store: hybrid. Delivery: **single-pr**, per design's Migration/Rollout (table-free generator lands first, ahead of UI wiring).

## Review Workload Forecast

| Field | Value |
|---|---|
| Estimated changed lines | ~450-550 authored (`SynthPatch.h` ~50, `SynthVoice.h/.cpp` ~280, `SynthEngine.h/.cpp` ~50, `MainComponent.h/.cpp` ~200, `SynthVoiceTests.cpp`/`SynthEngineTests.cpp` ~150) |
| 400-line budget risk | Medium vs. the skill's default 400-line guard; Low vs. this session's orchestrator-authorized 800-line budget |
| Chained PRs recommended | No |
| Suggested split | Single PR — see Work Unit below |
| Delivery strategy | single-pr |
| Chain strategy | pending |

Decision needed before apply: No
Chained PRs recommended: No
Chain strategy: pending
400-line budget risk: Medium

### Suggested Work Units

| Unit | Goal | Likely PR | Focused test command | Runtime harness | Rollback boundary |
|---|---|---|---|---|---|
| 1 | All 7 phases below, table-free generator first | PR 1 (base: `main`) | `BerlinTests.exe --category=Berlin` | Launch `Berlin.exe`, Phase 6 manual gate | `git revert` the single commit; no `.jucer`/generated files touched |

## Phase 1: Ranges + Clamp Helper (`SynthPatch.h`)

- [ ] 1.1 Add 9 `inline constexpr` min/max pairs + `constexpr clampParameter(float,float,float)` (`<algorithm>`, JUCE-free) to `Source/synth/SynthPatch.h`, per design's Interfaces block.
- [ ] 1.2 Update the file's header comment: drop "fixed / no parameter UI" wording.
- [ ] 1.3 RED then GREEN: add a `clampParameter` unit test (extremes, NaN-adjacent) to `Tests/Source/SynthVoiceTests.cpp` before/alongside the constants exist.

## Phase 2: Table-Free Generator + Equivalence Test (gates everything after it)

- [ ] 2.1 RED: add the DFT-harmonics equivalence test to `SynthVoiceTests.cpp` — reference 128-point table oscillator vs. `SynthVoice`, saw/square/triangle, harmonics 1-8 within 3%, RMS within 3% (fails to compile/pass until 2.2-2.3 land).
- [ ] 2.2 GREEN: delete `applyWaveform()`; `prepare()` installs the single 4-way `[this]` generator lambda (`lookupTableNumPoints = 0`), Decision 1, in `SynthVoice.cpp`.
- [ ] 2.3 GREEN: add waveform-switch-mid-note test (no dropout, all-finite) and confirm equivalence test passes. *(internal-synth-voice: Four Selectable Oscillator Waveforms)*
- [ ] 2.4 Confirm no allocation on waveform switch (RT-safety spot check per `juce-app-dev`).

## Phase 3: SynthVoice Atomics + applyParameters() + LFO Re-Park

- [ ] 3.1 RED: add live cutoff/resonance-mid-note, live-ADSR-mid-note, LFO-destination-re-park, and clamp/assert-freedom tests to `SynthVoiceTests.cpp` (fail against current `SynthVoice.h`).
- [ ] 3.2 GREEN: add nested `Parameters` struct of atomics + `static_assert`s, ten setters, `applyParameters()`, `lastAppliedCutoffHz`, cached `ADSR::Parameters`, live `waveform` member to `SynthVoice.h`. *(internal-synth-voice: all 4 modified requirements)*
- [ ] 3.3 GREEN: implement `applyParameters()` (relaxed loads, once per control block) in `SynthVoice.cpp`.
- [ ] 3.4 GREEN: gate `adsr.setParameters()` on an actual change (Decision 4).
- [ ] 3.5 GREEN: rewrite `updateLfoModulation()` base-then-delta — always re-apply all 4 base values, add delta to the active destination only (Decision 3). *(closes Phase 8 bug)*
- [ ] 3.6 Confirm all Phase 3 tests green; no allocation/lock/log in `applyParameters()`/`updateLfoModulation()`.

## Phase 4: SynthEngine Forwarders

- [ ] 4.1 Add ten one-line `voice.setX(v)` forwarders to `SynthEngine.h/.cpp`, mirroring `SynthVoice`'s setter signatures.
- [ ] 4.2 Extend `SynthEngineTests.cpp`: each forwarder produces the same rendered output as calling the voice setter directly.

## Phase 5: MainComponent UI Wiring

- [ ] 5.1 `MainComponent.h`: add 2 `ComboBox` + 8 `Slider` + 10 name `Label`s + 4 section `Label`s; declare `pushAllParametersToSynth()`.
- [ ] 5.2 `MainComponent.cpp` ctor: add `configureSlider(slider, label, name, min, max, initial, midPoint)` lambda; wire 10 callbacks; every initial value read from `kDefaultPatch`; `setValue(v, dontSendNotification)`.
- [ ] 5.3 `MainComponent.cpp::resized()`: two-column layout via `placeLabelled` lambda (Decision 7); merge `synthToggle`/`fxToggle` onto one row.
- [ ] 5.4 `MainComponent.cpp::prepareToPlay`: call `pushAllParametersToSynth()` immediately after `synth.prepare(spec)` (Decision 6).
- [ ] 5.5 Build `Berlin.sln`; confirm clean compile; no `.jucer` regen needed (no new files).

## Phase 6: Manual Audibility Gate (human-only)

- [ ] 6.1 **(human-verified)** Drag every control while sustaining (immediate, no zipper/click); switch waveform mid-note (no dropout); switch LFO destination mid-note (nothing parked); saw/square/triangle sound like Phase 8; restart audio device (sliders/sound stay in sync).

## Phase 7: Final Cleanup & Verification

- [ ] 7.1 Run `BerlinTests.exe --category=Berlin`; confirm exit 0, no regression.
- [ ] 7.2 RT-safety review: confirm no allocation/lock/log in `applyParameters`, `updateLfoModulation`, the generator; `initialise` called exactly once (in `prepare`).
- [ ] 7.3 Diff `Source/core/`, `generation/`, `playback/`, `midi/`, `export/`, `Lfo.*`, `SynthEffects.*` against `main`; confirm byte-for-byte unchanged.
- [ ] 7.4 Merge the spec delta into `openspec/specs/internal-synth-voice/spec.md`.
