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

- [x] 1.1 Add 9 `inline constexpr` min/max pairs + `constexpr clampParameter(float,float,float)` (`<algorithm>`, JUCE-free) to `Source/synth/SynthPatch.h`, per design's Interfaces block.
- [x] 1.2 Update the file's header comment: drop "fixed / no parameter UI" wording.
- [x] 1.3 RED then GREEN: add a `clampParameter` unit test (extremes, NaN-adjacent) to `Tests/Source/SynthVoiceTests.cpp` before/alongside the constants exist.

## Phase 2: Table-Free Generator + Equivalence Test (gates everything after it)

- [x] 2.1 RED: add the DFT-harmonics equivalence test to `SynthVoiceTests.cpp` — reference 128-point table oscillator vs. `SynthVoice`, saw/square/triangle, harmonics 1-8 within 3%, RMS within 3% (approval test: passed as a baseline against the still-table-based pre-2.2 `SynthVoice`; the mid-note-switch test in 2.3 is what genuinely fails to compile pre-`setWaveform`).
- [x] 2.2 GREEN: delete `applyWaveform()`; `prepare()` installs the single 4-way `[this]` generator lambda (`lookupTableNumPoints = 0`), Decision 1, in `SynthVoice.cpp`. Added a minimal `setWaveform()` (plain member write) so mid-note switching is testable now; Phase 3 upgrades it to atomic-backed without changing its public signature.
- [x] 2.3 GREEN: add waveform-switch-mid-note test (no dropout, all-finite) and confirm equivalence test passes. *(internal-synth-voice: Four Selectable Oscillator Waveforms)*
- [x] 2.4 Confirm no allocation on waveform switch (RT-safety spot check per `juce-app-dev`): `setWaveform()` is a single non-atomic member assignment, no heap/lock/log.

## Phase 3: SynthVoice Atomics + applyParameters() + LFO Re-Park

- [x] 3.1 RED: add live cutoff/resonance-mid-note, live-ADSR-mid-note, LFO-destination-re-park, and clamp/assert-freedom tests to `SynthVoiceTests.cpp` (failed to compile against pre-3.2 `SynthVoice.h`, confirmed).
- [x] 3.2 GREEN: add nested `Parameters` struct of atomics + `static_assert`s, ten setters, `applyParameters()`, `lastAppliedCutoffHz`, cached `ADSR::Parameters`, live `waveform` member to `SynthVoice.h`. *(internal-synth-voice: all 4 modified requirements)*
- [x] 3.3 GREEN: implement `applyParameters()` (relaxed loads, once per control block) in `SynthVoice.cpp`.
- [x] 3.4 GREEN: gate `adsr.setParameters()` on an actual change (Decision 4).
- [x] 3.5 GREEN: rewrite `updateLfoModulation()` base-then-delta — always re-apply all 4 base values, add delta to the active destination only (Decision 3). *(closes Phase 8 bug)*
- [x] 3.6 Confirm all Phase 3 tests green (125/125 scenarios, exit 0); no allocation/lock/log in `applyParameters()`/`updateLfoModulation()` (code-reviewed: atomic loads/stores, `filter.setResonance`/`setCutoffFrequency`/`adsr.setParameters`/`lfo.setRate` are all pre-allocated scalar-coefficient updates, no heap/lock/log).

## Phase 4: SynthEngine Forwarders

- [x] 4.1 Add ten one-line `voice.setX(v)` forwarders to `SynthEngine.h/.cpp`, mirroring `SynthVoice`'s setter signatures. (Implemented **eleven** — design.md's Interfaces section explicitly says "mirrors all eleven setter signatures verbatim"; `setWaveform` was the 11th, already introduced in Phase 2. Noted as a task-count/design wording discrepancy, resolved in design's favor.)
- [x] 4.2 Extend `SynthEngineTests.cpp`: each forwarder produces the same rendered output as calling the voice setter directly.

## Phase 5: MainComponent UI Wiring

- [x] 5.1 `MainComponent.h`: add 2 `ComboBox` + 9 `Slider` + 11 name `Label`s + 4 section `Label`s; declare `pushAllParametersToSynth()`. (9 sliders / 11 labels, not 8/10 — design.md's Interfaces section lists 11 total setters (9 float + waveform + lfoDestination) and the row-count math in Decision 7 only works out to "6 rows"/"9 rows" with 9 sliders; the task text's "8 Slider"/"10 name Labels" undercounts by one, same pattern as Phase 4's "ten" vs. design's "eleven" forwarders. Resolved in design's favor.)
- [x] 5.2 `MainComponent.cpp` ctor: add `configureSlider(slider, label, name, min, max, initial, midPoint)` lambda; wire 11 callbacks (9 slider + 2 combo); every initial value read from `kDefaultPatch`; `setValue(v, dontSendNotification)`.
- [x] 5.3 `MainComponent.cpp::resized()`: two-column layout via `placeLabelled` lambda (Decision 7); merge `synthToggle`/`fxToggle` onto one row.
- [x] 5.4 `MainComponent.cpp::prepareToPlay`: call `pushAllParametersToSynth()` immediately after `synth.prepare(spec)` (Decision 6).
- [x] 5.5 Build `Berlin.sln`; confirm clean compile (confirmed: `Berlin_App.vcxproj -> ...Berlin.exe`, only 2 pre-existing unrelated warnings in `Main.cpp`); no `.jucer` regen needed (no new files).

## Phase 6: Manual Audibility Gate (human-only)

- [x] 6.1 **(human-verified)** Drag every control while sustaining (immediate, no zipper/click); switch waveform mid-note (no dropout); switch LFO destination mid-note (nothing parked); saw/square/triangle sound like Phase 8; restart audio device (sliders/sound stay in sync). Confirmed by user 2026-09-06: "everything works great and as expected."

## Phase 7: Final Cleanup & Verification

- [x] 7.1 Run `BerlinTests.exe --category=Berlin`; confirm exit 0, no regression. Confirmed: exit 0, 126/126 scenarios (up from the 116/116 baseline), zero failures.
- [x] 7.2 RT-safety review: confirm no allocation/lock/log in `applyParameters`, `updateLfoModulation`, the generator; `initialise` called exactly once (in `prepare`). Confirmed by line-by-line read of the final `SynthVoice.cpp`: `applyParameters()` is relaxed atomic loads + `filter.setResonance`/change-gated `adsr.setParameters`/`lfo.setRate` (all pre-allocated scalar-coefficient updates, no heap/lock/log); `updateLfoModulation()` is pure arithmetic + `oscillator.setFrequency`/change-guarded `filter.setCutoffFrequency`; `oscillator.initialise(...)` appears exactly once, in `prepare()`.
- [x] 7.3 Diff `Source/core/`, `generation/`, `playback/`, `midi/`, `export/`, `Lfo.*`, `SynthEffects.*` against `main`; confirm byte-for-byte unchanged. **Clarification**: diffed against `1b7e8d2` (the commit this branch was cut from — literal `main` predates all of roadmap Phase 8 and would show unrelated history, not this change's untouched tier). `git diff --stat 1b7e8d2 HEAD -- <those paths>` produced zero output — confirmed byte-for-byte unchanged.
- [x] 7.4 Merge the spec delta into `openspec/specs/internal-synth-voice/spec.md`. Done: Purpose and the 5 modified requirements (waveforms, filter, ADSR, LFO, allocation-free rendering) replaced with the delta's versions (numeric ranges spelled out inline instead of the delta's `[range pinned by design]` placeholders); the 2 unmodified requirements (monophonic, silence-when-idle) carried over verbatim.
