# Exploration: step-gate (per-step gate length)

Follows `step-velocity`. Mirror of Engram `sdd/step-gate/explore` (obs 620).

## Current State

- **SequencePlayer** keeps a single `pendingNote` / `pendingStep`. The note-off is emitted only at the next playable boundary, before the next note-on, at the same offset.
  - Other note-offs are emitted at offset 0: keepPosition adopt, full swap, adopted stop and `flushPendingNoteOff`.
  - The keepPosition predicate checks only `skipped`, `active` and `note`, so a gate-only edit sends nothing. This comes for free.
  - Every path sets `pendingNote = -1`, so a note is never turned off twice.
- **Transport:** `getBoundary(i).sampleOffset` is block-relative. The absolute boundary is `getSamplePosition() + offset`, and `getSamplesPerStep()` is available. `transport.reset()` zeroes the position on a full swap.
- **MidiEventTranslator** uses `sampleOffset` unmodified, so a mid-block note-off works.
- **SynthEngine** drops events whose offset is below its cursor, so events MUST be non-decreasing in offset. This is the key constraint.
  - A note-off is matched against `currentNote`.
  - `StepEventBuffer` holds 64 events. The gate off replaces the boundary off, so the worst-case count is unchanged.
- **Synth:** the default ADSR is A 10 ms, D 150 ms, S 0.7, R 200 ms.
  - An early `noteOff` just starts the release, so no synth change is needed.
  - Gate 100 means off and on at the same sample, which is the existing legato glide path.
- **Export:** `MidiExportTimeline` works in ticks at 960 PPQ, so `ticksPerStep` is 240 at 4 steps per beat.
  - The header already says a gate change must update both the player and the timeline.
  - `MidiFileWriter` is untouched.
- **Velocity template to follow:**
  - The field is appended last on `Step`, `StepEvent` and `MidiExportEvent`.
  - `validateStepEdit` and `editStep` take a trailing `optional<int>`, and `UiBridge` reads it with `readOptionalInt`.
  - `sendStep` takes the full step object.
  - The `step-controls` row holds `step-vel` and `step-skip`, and the levels live in `lib/dynamics.ts`.
- **Collision:** `data-gate` and the StepView `gate` prop already mean the ACTIVE flag.

## Recommendation

Ship it as a single PR.

- **Model:**
  - `Step::gatePercent` is an int from 1 to 100, default 100, with `kDefaultGatePercent`, `kMin`, `kMax` and `isValidGatePercent`.
  - The wire and TypeScript key is also `gatePercent`.
  - `StepEvent` gets NO new field.
- **Player:** add `long long pendingOffAbs`, with `LLONG_MAX` as the sentinel for gate 100. That adds zero new events, so the output stays bit-identical.
  - At note-on: `gateSamples = max(1, llround(sps * gate / 100))`, then `pendingOffAbs = position + b.sampleOffset + gateSamples`.
  - Before each boundary: if `pendingNote >= 0 && pendingOffAbs < boundaryAbs`, emit the off at `pendingOffAbs - position` and clear `pendingNote`.
  - After the loop: if `pendingNote >= 0 && pendingOffAbs - position < numSamples`, emit the off.
  - `pendingOffAbs` is read only while `pendingNote >= 0` and is rewritten at every note-on, so no new reset sites are needed.
  - If the tempo changes mid-gate, the off keeps its absolute sample, and the boundary-first rule still applies.
  - A keepPosition gate edit takes effect on the next pass.
- **Ties and legato:** the model has no ties.
  - Gate above 100 is DEFERRED, because it needs overlapping notes, a pending-note queue and more than the single-voice synth.
  - Rest and skipped steps ignore their gate. The gate is a fraction of the step that owns the note, and `nextPlayableStep` is unaffected.
- **Export:** for gate below 100, push the off right after the note-on at `tick + clamp((gate*tps+50)/100, 1, tps)`, set `pendingNote = -1`, and emit no terminal off.
  - Gate 100 keeps the legacy path, so files stay byte-identical.
  - Test parity with an exact ratio: 24000 samples per step against 240 ticks.
- **Validation:** the order is index, note, velocity, gate, busy. The error token is `gate-out-of-range`, with `StepEditError` / `StepEditResult::gateOutOfRange`. `kCommands` stays at 16.
- **UI:** a cycle button with levels 25/50/75/100 (staccato, short, long, full).
  - It goes in `step-controls` as a third button, `.step-gate`, with `data-gate-length` holding the level name.
  - Generalize `nearest`/`next` in `dynamics.ts` to cover both level tables.
  - Check the cell width at 16 steps.
  - Rejected alternatives: a drag bar (high cost), modifier-click (poor UX), a row-level pane (medium cost).
- **Out of scope:** gate above 100, ties, preset persistence, an accent/gate generator.

## Size

About 150 production lines plus about 330 test lines, roughly 480 (range 450-650). That is under the 800 budget, so no split is needed.

## Risks

- A note-off in the wrong order or a stuck note. Tests must assert the order of the full event stream.
- Bit-identity at gate 100: the goldens, Reproducibility and export suites must stay unchanged.
- Rounding differs between samples (player) and ticks (export).
- Tempo change mid-gate and gates that round to 1 sample need explicit tests.
- A keepPosition gate edit keeps the old note-off time for the note that is already sounding.
- UI density, and the `data-gate` naming collision.
- A stale `pendingOffAbs` after `prepare()` is harmless; it falls back to the legacy off.
- Apply strictly on top of step-velocity, which touches the same files.

## Specs Needing Deltas

step-event-scheduling (the "a note MUST sound for its full step" rule), sequencing-core, step-editing, ui-bridge, midi-export-timeline, web-ui-layout, web-ui-controls.
