# Proposal: Step Velocity (Per-Step Accents End to End)

## Intent

Every Berlin note plays at the fixed velocity 100, so a pattern has no dynamics. Accents are a core part of the Berlin School sound (Franke, E&MM 1983: a few accent levels are enough; ucapps velocity grooves). This change adds per-step velocity from the model, through MIDI out, the internal synth and export, to an editable control in the UI.

It is the first half of the `step-dynamics` exploration, split by feature. Per-step gate length comes next in `step-gate`.

## Scope

### In Scope
- Append `int velocity = 100` (1..127) to `Step`, after `skipped`. The struct stays an aggregate, and `step-gate` will append `gate` after it.
- Append `velocity` to `StepEvent`. `SequencePlayer` fills it from the step on note-on.
- `MidiEventTranslator` sends the event velocity, defensively clamped to 1..127. `kNoteVelocity` survives only as the default.
- `SynthVoice::noteOn(freq, velocity)` latches a gain of `velocity / 100` (see D3).
- Export:
  - append `velocity` to `MidiExportEvent`;
  - `MidiFileWriter` writes per-event velocity;
  - the redundant fixed-velocity constructor argument is removed.
- `StepEdit` rejects velocity outside 1..127 (no clamping). `editStep` takes an optional velocity; when it is absent, the current value is kept.
- `UiBridge`: `setStep` takes an optional `velocity`, and `encodeSteps` emits it. This adds the missing `encodeSteps` tests. The command count stays 16.
- React:
  - protocol and mock carry velocity;
  - `sendStep` takes a step object;
  - new `ui/src/lib/dynamics.ts` holds the level table (ghost 40 / soft 70 / normal 100 / accent 127) and nearest-level snapping;
  - a per-cell velocity cycle button sits beside the cell as a sibling (a11y) and joins the roving tabindex;
  - a `data-velocity` attribute drives the visual.

### Out of Scope
- Gate below 100%, export gate ticks and the gate UI: all in `step-gate`.
- Ties above 100%, an accent generator and grooves: deferred.
- Persisting velocity: deferred, the same as note edits (no schema change).

## Capabilities

### New Capabilities
- None.

### Modified Capabilities
- `sequencing-core`: `Step.velocity`.
- `step-event-scheduling`: note-on events carry the step velocity.
- `midi-output-dispatch`: per-event velocity with a clamp, replacing the fixed velocity.
- `internal-synth-voice`: velocity-scaled note gain.
- `internal-synth-output`: no-clip requirement covers accent 127 at the default level.
- `midi-export-timeline`: per-step velocity replaces "Fixed Velocity From the Shared Constant".
- `midi-file-output`: per-event velocity; no fixed-velocity constructor argument.
- `sequence-mutation`: mutated steps keep their velocity.
- `step-editing`: velocity edits, rejection when out of range, absent means unchanged.
- `ui-bridge`: `setStep` and `encodeSteps` carry `velocity`.
- `web-ui-controls`: velocity cycle button and visual.

## Approach

| # | Decision |
|---|---|
| D1 | Velocity is stored as raw MIDI 1..127. The UI snaps to 4 levels, and 8 levels later need no engine change. |
| D2 | No RNG draws, so goldens and ReproducibilityTests stay unchanged. Generated steps get 100. |
| D3 | Linear `gain = v/100`. 100 gives exactly 1.0f (bit-identical audio, so the existing tests are untouched). Ghost is about -8 dB, and accent is +2.1 dB (1.27x). The headroom is a deliberate choice: accents should get louder, not normal hits quieter. A `v/127` curve would re-baseline every level test. A new test must prove accent 127 at the default output 0.8 with effects does not clip. If it fails, design caps the gain at 127 lower while staying neutral at 100. |
| D4 | Fields are appended last, and absent arguments mean unchanged, so `step-gate` adds `gate` with the same pattern and no churn. |

## Affected Areas

| Area | Impact |
|------|--------|
| `Source/core/Step.h`, `Source/playback/{StepEvent.h,SequencePlayer.cpp}` | Modified |
| `Source/midi/MidiEventTranslator.*`, `Source/synth/{SynthVoice.*,SynthEngine.cpp}` | Modified (audio thread) |
| `Source/export/{MidiExportTimeline.*,MidiFileWriter.*}` and its callers | Modified |
| `Source/generation/StepEdit.*`, `Source/plugin/BerlinAudioProcessor.cpp`, `Source/bridge/UiBridge.cpp` | Modified |
| `ui/src/{bridge/protocol.ts,bridge/mock*,store/actions.ts,components/StepView.tsx,lib/dynamics.ts}` | Modified/New |
| `Tests/Source/*`, `ui` Vitest | New and modified tests |

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| Accent clips the output | Med | No-clip test (D3); fallback ceiling |
| Default not neutral | Low | Existing suites run unchanged |
| `sendStep` refactor churns tests | Med | One mechanical pass |
| UI density | Low | One button now; gate layout is revisited in `step-gate` |
| Velocity lost on reload | High | Explicit non-goal |
| Budget overrun | Med | See forecast |

## Rollback Plan

Single PR: revert the commit. There is no schema change, so presets and state are unaffected.

## Dependencies

- `step-model` (archived, 4e94506).

## Success Criteria

- [ ] At default velocity, MIDI, synth audio, export bytes and goldens are identical to before the change.
- [ ] Edited velocities reach MIDI out, the synth gain and the exported file.
- [ ] Out-of-range edits are rejected.
- [ ] Accent does not clip at the default level.
- [ ] No allocation or locking is added on the audio thread.

## Review-size forecast

About 760-900 changed lines: production about 260 (C++ 120, React 140) and tests about 550 (C++ 320, Vitest 230). The test share is taken from step-model, which forecast 685 and landed about 1,330. This puts the change at or slightly over 800. If apply trends over, the first trim is the `encodeSteps` backfill beyond velocity.

## Proposal question round (auto mode — assumptions for user review)

1. Is +2.1 dB (1.27x) the right accent loudness, or should accent be capped nearer +1 dB?
2. Are 4 UI levels (40/70/100/127) enough for the first slice?
3. Should the velocity button cycle upward and wrap (ghost → soft → normal → accent → ghost)?
4. Is it acceptable that velocity edits are lost on preset reload?
