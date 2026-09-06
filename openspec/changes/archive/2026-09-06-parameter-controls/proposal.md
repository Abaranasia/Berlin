# Proposal: Parameter Controls (roadmap Phase 9)

Artifact store: hybrid. Engram: `sdd/parameter-controls/proposal`. Delivery: **single-pr**, 800-line review budget.

## Intent

Phase 8 shipped a working monophonic synth whose sound is frozen: `kDefaultPatch` is read exactly once, in `SynthVoice::prepare()` (`Source/synth/SynthVoice.cpp:71`), and never again. Users can hear the sequencer but cannot shape it — every audition sounds identical, so the generative engine cannot be evaluated against different timbres. Phase 9 makes the patch live: turn a knob, hear it now, while a note sounds.

## Scope

### In Scope
- Live user control over waveform, filter cutoff + resonance, ADSR (A/D/S/R), and LFO destination/rate/depth, in `MainComponent` per Berlin UI Pattern v1.
- A **live-apply path**: message-thread setters on `SynthEngine` → atomics → applied inside `SynthVoice`'s existing 32-sample control block (`kControlBlockSize`), mirroring the `enabled`/`effectsEnabled` idiom.
- **Waveform refactor**: saw/square/triangle move from allocating `oscillator.initialise(fn, 128)` lookup tables to table-free live-eval (`n = 0`), the technique `pulse` already uses (Decision 3) — the only way live switching stays RT-safe.
- **Fix**: LFO-destination switch mid-note currently parks the previous destination (osc frequency or filter cutoff) at its last modulated value. Re-apply base value every control block, then add the delta only for the active destination.
- Documented, clamped ranges for every exposed parameter; live/mid-note parameter-change tests.
- `pulseWidth` control — decide in `sdd-design` (see open decisions).

### Out of Scope
- Preset save/load or any persistence — later phase.
- Polyphony, piano roll, generation/randomize.
- Delay/reverb/`outputLevel` parameter UI (the existing FX on/off toggle stays as-is).
- Band-limited oscillators — the table-free refactor keeps naive waveforms, aliasing still accepted.
- `Source/core/`, `generation/`, `playback/`, `midi/`, `export/` — untouched.

## Capabilities

### New Capabilities
- None.

### Modified Capabilities
- `internal-synth-voice`: **Purpose** ("the patch is fixed for this phase. No parameter UI") is directly contradicted and MUST be replaced. Four Selectable Oscillator Waveforms (fixed-patch → live user selection + no-allocation-on-switch scenario); Low-Pass Filter With Resonance (live cutoff/resonance change while sounding); ADSR-Gated Amplitude (live change mid-envelope-stage — the "snap to new target" behaviour is expected, not a defect); Single LFO With Selectable Destination ("exactly one destination per fixed patch" → live-selectable, plus explicit expected behaviour when switching destination mid-note); Allocation-Free Voice Rendering (add scenario: live parameter application, incl. waveform switch, allocates nothing).
- `internal-synth-output`: **no requirement change.** Verification-only pass on "Allocation-Free Synth Contribution To The Callback" to confirm the new setter path in `getNextAudioBlock`'s call graph stays clean.

## Approach

Approach 1 from exploration: **atomic mirror + control-rate apply**. Chosen because it reuses two idioms this codebase already owns — message-thread atomic writes (`setEnabled`) and control-rate re-application (`updateLfoModulation`) — instead of inventing machinery.

Rejected: re-calling `SynthVoice::prepare()` on a dirty flag (allocates on the audio thread — violates the hard RT rule). Rejected: message-thread shadow voice + pointer swap (mid-note swap resets oscillator phase/ADSR/filter → audible click exactly when the user drags a slider, which is the worst possible moment).

## Settled decisions (given, not open)

1. Waveform generation becomes table-free for all four waveforms. Small per-sample CPU increase accepted; monophonic single voice makes it negligible.
2. Setters take the `SynthEngine`-level shape of `setEnabled`; continuous parameters need **no** edge latch (only booleans with reset semantics do).
3. New controls follow Berlin UI Pattern v1: in-class member initializers, lambda wired in the constructor, sequential `resized()` chops.
4. No new source files if the refactor lands in place → no `Berlin.jucer` regen expected; only `Tests/BerlinTests.jucer` if new test files are added.

## Open decisions for `sdd-design`

| Decision | Why it is open |
|---|---|
| Numeric range + taper for resonance | No documented safe max exists in code; high values approach self-oscillation. Must be measured, not guessed. |
| LFO rate bounds | Negative rate currently reverses phase; no clamp exists. |
| ADSR min/max times | No documented bounds; only cutoff has any (`kMinCutoffHz = 20`, max `0.49 × sampleRate`). |
| `pulseWidth` slider: in or out? | Not explicitly requested, but it is the only field that makes `pulse` meaningful, and it is already both a patch field and an LFO destination. |
| Gate `adsr.setParameters()` on actual change? | Called every 32 samples otherwise. Performance nicety, not correctness. |
| Slider taper (log for cutoff/rate?) | Linear sliders on frequency ranges feel wrong; a UX decision, not a DSP one. |

## Affected Areas

| Area | Impact | Description |
|---|---|---|
| `Source/synth/SynthVoice.h/.cpp` | Modified | Table-free waveform generator; atomic-mirrored params; base-then-delta LFO re-apply |
| `Source/synth/SynthEngine.h/.cpp` | Modified | New message-thread parameter setters + atomics |
| `Source/synth/SynthPatch.h` | Modified | Documented range constants (min/max per field) |
| `Source/MainComponent.h/.cpp` | Modified | ~9-10 new sliders/comboboxes + labels + layout |
| `Tests/Source/SynthVoiceTests.cpp`, `SynthEngineTests.cpp` | Modified | First live/mid-note parameter-change coverage |
| `Tests/BerlinTests.jucer` | Modified (conditional) | Only if new test files are added |
| `openspec/specs/internal-synth-voice/spec.md` | Modified | Delta: Purpose + 5 requirements |
| `Source/core/`, `generation/`, `playback/`, `midi/`, `export/` | Untouched | Must be byte-for-byte identical in the final diff |

## Risks

| Risk | Likelihood | Mitigation |
|---|---|---|
| Waveform refactor is not additive — it rewrites working Phase 8 DSP | High | Land the table-free generator with a before/after equivalence test (same waveform shape at same pitch) before wiring any UI |
| Table-free eval changes the audible character of saw/square/triangle vs. the 128-point table | Med | Accepted if audibly equivalent; verify via manual gate. If not, this becomes a real design decision, not a silent regression |
| Zipper noise / clicks when dragging a slider (32-sample cadence, no smoothing) | Med | Control-rate cadence already proven for LFO cutoff sweeps; add `SmoothedValue` only if the manual gate hears artefacts |
| Guessed parameter ranges produce unusable or self-oscillating settings | Med | Explicitly deferred to `sdd-design` as measured decisions, not proposal assumptions |
| Live `adsr.setParameters()` mid-envelope produces an audible jump | Low | Spec it as expected behaviour in the ADSR delta rather than treating it as a bug |
| 10 new controls overwhelm the flat `resized()` layout (window is 800×600 with export/status/toggles already present) | Med | `sdd-design` decides grouping/rows; no new layout framework |
| Scope creep into presets | Med | Enumerated non-goal; ranges are documented in code, not persisted |

## Delivery forecast (review workload guard)

- **800-line budget risk: Low-Medium.** Estimate ~450-550 authored lines (voice/engine ~180, UI ~120, tests ~150, spec delta ~80). No Projucer app regen expected.
- **Chained PRs recommended: No.** Single PR, per the cached `single-pr` strategy.
- **Decision needed before apply: No** — unless `sdd-tasks` re-forecasts above 800.

## Rollback Plan

`git revert` the single commit. All changes are source-only within `Source/synth/` and `Source/MainComponent.*`; reverting restores Phase 8's fixed-patch behaviour exactly. No file format, no persisted state, no data migration — nothing was written to disk by this phase. If `Tests/BerlinTests.jucer` was regenerated, restore it and re-run `Projucer.exe --resave`. Exported `.mid` output is unaffected (`Source/export/` untouched).

## Dependencies

- Exploration: Engram `sdd/parameter-controls/explore` (id 177, corrected revision).
- Archived Phase 8 `internal-synth` — `SynthPatch`/`SynthVoice`/`SynthEngine` contract and design Decisions 2, 3, 6.
- Baseline specs at `openspec/specs/internal-synth-voice/spec.md` and `internal-synth-output/spec.md`.
- A human for the audio manual gate — "no clicks while dragging" is not automatable.

## Success Criteria

- [ ] Every exposed parameter (waveform, cutoff, resonance, A/D/S/R, LFO destination/rate/depth) audibly changes the sound **while a note is sounding**, not only on the next note.
- [ ] Switching waveform mid-note introduces no allocation and no audible dropout.
- [ ] Switching LFO destination mid-note leaves no parameter parked at a stale modulated value (the Phase 8 latent bug is closed).
- [ ] Every control is clamped to a documented range; no setting produces silence, runaway self-oscillation, or an assert.
- [ ] Code review confirms no allocation, lock, or logging call was added to the audio path.
- [ ] `BerlinTests.exe --category=Berlin` exits 0, including new live/mid-note parameter tests.
- [ ] MIDI output and MIDI export are byte-for-byte unchanged in behaviour and diff.

## Proposal question round — needs user review

Asked as a sub-agent without direct user access; the assumptions below were taken so the phase is not blocked. Correct any of them before `sdd-design`.

1. **Who is this for, and at what moment?** Assumed: the developer/musician auditioning generated sequences, tweaking while playback loops. Not assumed: a performance instrument where controls are played expressively.
2. **What "feels right" for a parameter change?** Assumed: instant is better than smooth — no ramp/smoothing unless the manual gate hears artefacts. If you would rather every change glide, that flips a design decision.
3. **Is `pulseWidth` a user control or an internal LFO target only?** Assumed: to be decided in `sdd-design`, leaning "include it" since `pulse` is otherwise a fixed 50% square.
4. **Do parameter values need to survive an app restart?** Assumed: **no** — that is the preset phase. Every launch starts from `kDefaultPatch`.
5. **Is changing the sound of the Phase 8 default patch acceptable?** The table-free refactor may alter saw/square/triangle character slightly. Assumed: acceptable if audibly equivalent.
