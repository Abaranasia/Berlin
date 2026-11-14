# Proposal: Step Gate (Per-Step Gate Length End to End)

## Intent

Every Berlin note sounds for its full step, so patterns cannot be staccato. Short and mixed gates are the second half of Berlin School dynamics (the first half is `step-velocity`). This change adds per-step gate length from the model, through playback and export, to an editable UI control.

## Scope

### In Scope
- Append `int gatePercent = 100` (1..100) to `Step`, after `velocity`, with `kDefaultGatePercent`, `kMinGatePercent`, `kMaxGatePercent` and `isValidGatePercent`. `StepEvent` gets no new field.
- `SequencePlayer` emits the note-off at an absolute sample (`pendingOffAbs`) inside the step for gate below 100.
- `MidiExportTimeline` places the note-off at a gate-scaled tick for gate below 100.
- `StepEdit` rejects gate outside 1..100 with `gate-out-of-range` (order: index, note, velocity, gate, busy). An absent gate keeps the current value.
- `UiBridge`: `setStep` takes an optional `gatePercent`, and `encodeSteps` emits it. The command count stays 16.
- React: protocol and mock carry `gatePercent`; a third `.step-gate` cycle button (25/50/75/100: staccato, short, long, full) in `step-controls`, exposed as `data-gate-length`; `dynamics.ts` `nearest`/`next` generalized to both level tables.

### Out of Scope
- Gate above 100 and ties (need overlapping notes and a note queue).
- Persisting gate in presets/state (same as velocity).
- Accent or gate generators.

## Capabilities

### New Capabilities
- None.

### Modified Capabilities
- `sequencing-core`: `Step.gatePercent`.
- `step-event-scheduling`: replaces "a note MUST sound for its full step" with gate-scaled note-off; non-decreasing offsets.
- `midi-export-timeline`: gate-scaled note-off ticks.
- `step-editing`: gate edits, `gate-out-of-range`, absent means unchanged.
- `ui-bridge`: `setStep` and `encodeSteps` carry `gatePercent`.
- `web-ui-controls`: gate cycle button and `data-gate-length` visual.
- `web-ui-layout`: three buttons in `step-controls` at 16 steps.

## Approach

| # | Decision |
|---|---|
| D1 | Gate 100 uses the sentinel `LLONG_MAX`: zero new events, so MIDI, audio, export bytes and goldens are bit-identical. |
| D2 | Note-off at `max(1, llround(sps * gate / 100))` samples after note-on, emitted before any later boundary so offsets stay non-decreasing (SynthEngine constraint). A tempo change keeps the absolute sample. |
| D3 | Export: `tick + clamp((gate*tps+50)/100, 1, tps)`; no terminal off for short notes. |
| D4 | `data-gate-length`, not `data-gate` (already the active flag). |
| D5 | A keepPosition gate-only edit takes effect on the next pass; no synth change. |

## Affected Areas

| Area | Impact |
|------|--------|
| `Source/core/Step.h`, `Source/playback/SequencePlayer.*` | Modified (audio thread) |
| `Source/export/MidiExportTimeline.*` | Modified |
| `Source/generation/StepEdit.*`, `Source/plugin/BerlinAudioProcessor.*`, `Source/bridge/UiBridge.cpp` | Modified |
| `ui/src/{bridge/*,store/actions.ts,components/StepView.tsx,lib/dynamics.ts,styles/components.css}` | Modified |
| `Tests/Source/*`, `ui` Vitest | New and modified tests |

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| Out-of-order or stuck note-off | Med | Assert full event-stream order; tempo-change and 1-sample gate tests |
| Gate 100 not bit-identical | Low | Goldens, Reproducibility, export suites unchanged |
| Sample vs tick rounding drift | Low | Parity test at exact ratio (24000 samples / 240 ticks) |
| UI density at 16 steps | Med | Verify cell width |
| Conflicts with uncommitted velocity work | Med | Apply strictly on top of `step-velocity` |

## Rollback Plan

Single PR: revert the commit. No schema change; presets and state are unaffected.

## Dependencies

- `step-velocity` (in working tree, reviewed and approved).

## Success Criteria

- [ ] At gate 100, all outputs and goldens are identical to before.
- [ ] Short gates end notes inside the step in MIDI out, synth and export.
- [ ] Out-of-range gates are rejected.
- [ ] No allocation or locking added on the audio thread.

## Review-size forecast

About 480 lines (production ~150, tests ~330; range 450-650), under the 800 budget. No split.

## Proposal question round (auto mode — assumptions for user review)

1. Are 25/50/75/100 the right first gate levels?
2. Is a next-pass delay acceptable for gate edits on the sounding note?
3. Is losing gate on preset reload acceptable for this slice?
