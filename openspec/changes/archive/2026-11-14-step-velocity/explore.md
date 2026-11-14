# Exploration: step-dynamics

The full text is in Engram `sdd/step-dynamics/explore` (observation 478). This file is the openspec mirror for the hybrid store.

## Goal

Add per-step velocity/accent and per-step gate length, as the follow-up to step-model.

Sources (single-source, see the research memory):
- Franke (E&MM 1983): about 8 accent levels are enough, and gate is a multiple of the step.
- ucapps: the velocity groove 0, -32, -50, -32, 0, -32.

## Current state (verified on feat/step-dynamics)

**Step and editing**
- `Step{note, active, skipped}` is an aggregate. Fields appended at the end with defaults keep every existing initializer compiling.
- The builder and mutations copy whole `Step`s, so new fields ride along. Goldens stay valid as long as no RNG draws are added.
- `BerlinAudioProcessor::editStep` is the only place that builds a `Step{}` explicitly. It must keep velocity and gate when they are absent.

**Bridge**
- `UiBridge`: `encodeSteps` has no test.
- `setStep` gains optional args. The command count stays at 16.

**Playback**
- `StepEvent{sampleOffset, stepIndex, note, isNoteOn}`, in a 64-event buffer.
- `SequencePlayer` keeps one `pendingNote`, and its note-off goes at the next playable boundary.
- Other note-off paths: keepPosition adopt, full swap, stop, flush.

**Output**
- `MidiEventTranslator` uses `kNoteVelocity = 100`.
- `SynthEngine` drops events with offset < cursor, so ORDER MATTERS.
- `SynthVoice::noteOn(freq)` has no velocity.

**Export**
- `MidiExportEvent{tick, note, noteOn}` with `ticksPerStep = 240`.
- `MidiFileWriter(channel, fixedVelocity)` never sorts.

**Presets and UI**
- Preset schema v3 does not store steps, so dynamics are as ephemeral as note edits.
- React: StepView cell plus a sibling Skip button with roving tabindex, and `sendStep` with 5 positional args.

## Recommended decisions

- **Velocity:** stored as 1..127, default 100. The UI snaps to 4 levels (ghost 40, soft 70, normal 100, accent 127). 8 levels would be possible later without engine changes.
- **Gate:** integer percent 1..100, default 100, where 100 is legacy. Out-of-range values are rejected, not clamped.
- **Gate above 100% (ties/legato):** DEFERRED. It needs a pending-note queue and an overlap policy.
- **Accent generator:** DEFERRED. It needs its own sub-seed and persistence.
- **Player gate:**
  - An audio-thread `pendingOffAbs` holds an absolute sample position; `LLONG_MAX` means the legacy gate of 100.
  - Before each boundary, emit a due off. After the loop, emit an off that falls mid-block.
  - Every existing off path resets the pending state, so a note never gets a double off.
  - Tempo change mid-gate: the off keeps its absolute sample, and a boundary that now comes first still emits it.
  - A keepPosition edit of velocity or gate applies on the next pass, with no retrigger.
- **Export:** for gate < 100, push the off at `tick + max(1, (gate*ticksPerStep + 50)/100)`. Gate 100 keeps the legacy path, which stays byte-identical. The writer uses per-event velocity, and the fixed-velocity constructor argument is removed.
- **Synth:** a velocity gain is set at note-on. The curve is a design decision: v/100 with a ceiling, or v/127, which re-baselines the default level.
- **UI:**
  - Per-cell velocity and gate cycle buttons, as siblings of the cell (a11y).
  - `data-velocity` / `data-gate` attributes drive bar visuals.
  - `sendStep` takes a step object.
  - A new `ui/src/lib/dynamics.ts` holds the level tables.

## Size

About 345 production lines plus about 710 test lines, roughly 1,050 total (range 850-1,100). That's over the 800 budget. A split was recommended: either engine then UI, or velocity then gate.

## Risks

1. Gate-off ordering and double offs. A wrong order means stuck notes in `SynthEngine`.
2. Rounding drift between player and export: samples versus ticks.
3. The velocity gain curve: clipping versus re-baselining existing audio tests.
4. Default neutrality must be proven by running the existing suites unchanged.
5. Size.
6. UI density with two extra controls per cell.
7. The `sendStep` refactor churns existing tests.
8. Dynamics aren't persisted.
9. Gate length is fixed in samples at note-on, which interacts with tempo changes.
