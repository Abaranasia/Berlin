# Design: Step Gate (Per-Step Gate Length End to End)

## Technical Approach

Built strictly on the staged `step-velocity` tree. `Step` gains `int gatePercent = kDefaultGatePercent` (100) as its last field. Only two places read it: `SequencePlayer` (absolute-sample note-off) and `MidiExportTimeline` (tick note-off). `StepEvent`, `MidiExportEvent`, the translator, the synth and `MidiFileWriter` are untouched. Gate 100 takes the existing code path exactly (sentinel), so every output and golden is bit-identical. Edit, bridge and React follow the velocity template one field later.

## Architecture Decisions

| # | Choice | Rejected | Rationale |
|---|---|---|---|
| D1 | `core/Step.h`: `kDefaultGatePercent = 100`, `kMinGatePercent = 1`, `kMaxGatePercent = 100` (also the percent denominator), `constexpr bool isValidGatePercent (int)`. Field appended last; `operator==` compares it. | A float fraction; a separate gate type | Same shape as velocity; integer keeps `==` exact and the wire format trivial. |
| D2 | Player member `long long pendingOffAbs { kNoGateOff }`, `kNoGateOff = std::numeric_limits<long long>::max()` (file-local constant). Gate >= 100 stores the sentinel and emits nothing new. | A per-event gate on `StepEvent`; a note queue | One scalar, no allocation, no new event for gate 100, `StepEventBuffer` worst case unchanged (the gate off replaces the boundary off). |
| D3 | Gate off uses strict `pendingOffAbs < boundaryAbs`; equality or overshoot falls through to the legacy boundary off. | Clamping the gate length to `sps - 1` | Exactly one off per note by construction, even under rounding or a faster tempo. |
| D4 | Gate offs are evaluated only while the transport is running. | Firing them on a direct `stop()` | Time is frozen while stopped; a direct `stop()` keeps today's "flush still emits" contract. |
| D5 | `prepare()` and `reset()` also set `pendingOffAbs = kNoGateOff`. | Relying on "rewritten at every note-on" | Explicit and one line each; a note surviving `prepare()` falls back to the legacy off at the first boundary (position restarts at 0). |
| D6 | Export: gate < 100 pushes the off immediately after the on at `tick + clamp ((gate * tps + kMaxGatePercent / 2) / kMaxGatePercent, 1, tps)` and clears `pendingNote`; gate 100 keeps the pending/terminal path. | Post-sorting events | Off tick <= next step tick, so events stay non-decreasing without a sort. |
| D7 | Edit: `StepEditError::gateOutOfRange`, `StepEditResult::gateOutOfRange`, token `gate-out-of-range`; trailing `std::optional<int> gatePercent = {}` on `validateStepEdit` and `editStep`. Order: index, note, velocity, gate, busy. Bridge reuses `readOptionalInt` (so `invalid type: gatePercent` / `non-finite: gatePercent` precede range). `kCommands` stays 16. | An options struct | Velocity D7/D8 reserved this slot; existing call sites untouched. |
| D8 | React: generalize `dynamics.ts` to `nearestLevel (levels, value)` / `nextLevel (levels, value)` over `Level<N> { name: N; value }`; add `GATE_LEVELS` staccato 25, short 50, long 75, full 100 (full wraps to staccato). `DEFAULT_VELOCITY`, `DEFAULT_GATE`, `GATE = {min:1,max:100}` in `limits.ts`; no literal 100 in StepView or the mock seed. | A second pair of gate-only helpers | One tested algorithm; integer gates can never tie between levels 25 apart. |
| D9 | `data-gate-length={level name}` on `.step` (mirrors `data-velocity`); `.step-gate` button between `.step-vel` and `.step-skip`, content `<span class="gate-bar" aria-hidden>` whose width comes from CSS per level; `aria-label="Gate step N: <name>"`, roving `tabIndex` like its siblings. | `data-gate` (already the active flag); letter glyphs (S/S collide) | Avoids the collision; the bar reads as a length at a glance. |
| D10 | Density: `.step-controls` becomes `grid-template-columns: 1fr 1fr`, `.step-skip { grid-column: 1 / -1 }`. | Three buttons in one row | At the 16-column layout a cell is about 45 px; three in a row give about 12 px per button (the "Skip" label already clips at two). Two-up gives about 20 px glyph buttons and a full-width Skip. |
| D11 | keepPosition gate-only edit: no code change (predicate checks skipped/active/note); the sounding note keeps its old `pendingOffAbs`, the next pass uses the new gate. | Re-timing the live off | No synth change; pinned by a handoff test. |

## Player Algorithm (`SequencePlayer::process`)

```
adopt block (unchanged)            // offs at 0 clear pendingNote; full swap resets transport
setBpm; play/stop transition       // adopted stop: off at 0, pendingNote = -1
blockStart = transport.getSamplePosition()
for each boundary b:
    boundaryAbs = blockStart + b.sampleOffset
    if pendingNote >= 0 && pendingOffAbs < boundaryAbs:          // gate off BEFORE the boundary
        push { pendingOffAbs - blockStart, pendingStep, pendingNote, off }; pendingNote = -1
    legacy boundary off (unchanged); pendingNote = -1
    on step.active: push on; pendingNote/pendingStep as today;
        pendingOffAbs = gateOffAbs (boundaryAbs, transport.getSamplesPerStep(), step.gatePercent)
if transport.isRunning() && pendingNote >= 0 && pendingOffAbs < blockStart + numSamples:
    push { pendingOffAbs - blockStart, ... off }; pendingNote = -1       // gate off at block end
transport.advance (numSamples)
```

`gateOffAbs (onAbs, sps, g)`: `g >= kMaxGatePercent` returns `kNoGateOff`; else `onAbs + max (1LL, llround (sps * g / kMaxGatePercent))`. Invariant: an unfired `pendingOffAbs >= blockStart`, so offsets are in `[0, numSamples)` and non-decreasing (SynthEngine constraint). Tempo change: the absolute sample is kept; if the faster grid brings the boundary first, D3 emits the legacy off there. Full swap, adopted stop, keepPosition note change and `flushPendingNoteOff` clear `pendingNote`, so a stale `pendingOffAbs` is never read.

## Export Tick Math

`tps = 960 / stepsPerBeat`. Gate 50 at tps 240 gives 120; gate 1 gives 2; at tps 1 the clamp gives 1. Parity at the exact ratio (96 kHz, 60 BPM, 4 spb: sps 24000, tps 240): `samples = 240 g`, `ticks = round (2.4 g)` with no ties (24g is even), so `llround (samples * tps / sps) == ticks` for every g in 1..99.

## Data Flow

    Gate button ─▶ sendStep{…,gatePercent} ─▶ setStep ─▶ readOptionalInt ─▶ editStep ─▶ validate(index,note,velocity,gate) ─▶ busy ─▶ publish(keepPosition)
    audio: boundary note-on ─▶ pendingOffAbs ─▶ gate off (mid-block or later block) ─▶ translator / SynthEngine (unchanged)
    export: timeline off at tick + gateTicks ─▶ MidiFileWriter (unchanged)

## File Changes (authored added+deleted)

| File | ~Lines |
|---|---|
| `Source/core/Step.h` | 10 |
| `Source/playback/SequencePlayer.{h,cpp}` | 30 |
| `Source/export/MidiExportTimeline.{h,cpp}` (header comment, gate branch) | 12 |
| `Source/generation/StepEdit.{h,cpp}`, `Source/plugin/BerlinAudioProcessor.{h,cpp}`, `Source/bridge/UiBridge.cpp` | 25 |
| `ui/src/lib/dynamics.ts`, `bridge/{protocol,limits,mock}.ts`, `store/store.ts` (lane type) | 35 |
| `ui/src/components/StepView.tsx`, `styles/components.css` | 40 |
| C++ tests: Step, SequencePlayer, Handoff, Stop, MidiExportTimeline, StepEdit, MutationEngine, Processor, UiBridge | 290 |
| Vitest: dynamics, StepView, mock, protocol/store/App fixtures | 90 |

About 150 production + 380 tests = 530 (range 500-700). `actions.ts` needs no change (`StepState` is derived).

## Testing Strategy (strict TDD, RED first, in order)

1. **StepTests**: default 100, `==` sees it, aggregate holds, `isValidGatePercent` 0/1/100/101.
2. **SequencePlayerTests** (96 kHz, 60 BPM, 4 spb so sps = 24000), full `StepEvent` vector equality: two active steps gate 50, one 48000 block gives `[on@0, off@12000, on@24000, off@36000]`; the same at block sizes 1/64/511/4096 gives identical absolute times (block-size independence) with non-decreasing offsets per block; off landing exactly on a block end appears at offset 0 of the next block; 1-sample gate (sps 60, gate 1) gives the off at on+1; gate 99 rounding onto the boundary gives exactly one off; rest and skipped steps ignore gate; gate 100 stream equals the legacy expectation.
3. **Tempo**: slower mid-gate keeps the absolute off sample; faster so the boundary comes first gives one off at the boundary.
4. **Handoff/Stop**: keepPosition gate-only edit emits nothing at adopt, the old off time holds, the next pass uses the new gate; keepPosition note change after the gate off emits no extra off; full swap and adopted stop before the gate off emit one off at 0 and none later; direct `stop()` then `process` emits no gate off and `flushPendingNoteOff` still emits; after `prepare`/`reset` the off falls back to the first boundary.
5. **MidiExportTimelineTests**: gate 50/1, tps 1 clamp, short last note has no terminal off, gate 100 events and `endTick` unchanged, samples-vs-ticks parity for g = 1..99.
6. **StepEdit/MutationEngine**: 0/101 reject, 1/100 ok, index/note/velocity each precede gate, nullopt ok; `addNote` keeps the target gate.
7. **Processor**: absent keeps, present sets, out of range leaves sequence and `edited` unchanged, validation before busy; a gate-50 note-off reaches `processBlock` MIDI at the expected sample and `exportMidiTo` at the expected tick.
8. **UiBridge**: set/absent/0/101 → `gate-out-of-range`, `"x"`/1.5 → `invalid type: gatePercent`, order against velocity-out-of-range and busy, `encodeSteps` emits 5 keys, 16 commands.
9. **Vitest**: `nearestLevel`/`nextLevel` over both tables (gate 60→50, 63→75, full→staccato), velocity cases still green; StepView renders `.step-gate` with label and `data-gate-length`, click sends `setStep{…,gatePercent:25}` from full; mock order and keep-when-absent.

Existing goldens, Reproducibility, export and synth bit-identity suites run unchanged. Manual: 16-step row at minimum window width, staccato by ear and on a MIDI monitor, exported file in a DAW.

## Cut Order (if over 800)

1. Merge the block-size variants into one parameterized loop. 2. Drop the protocol/App fixture-only edits to the minimum type fix. 3. Merge mock cases into StepView tests. 4. Reduce the Processor export assertion to the first off. Never cut stream-order, tempo, keepPosition, parity or gate-100 identity tests.

## Threat Matrix

N/A: no routing, shell, subprocess, VCS/PR automation, executable-file classification, or process-integration boundary.

## Migration / Rollout

No migration. Gate is not persisted (same as velocity).

## Open Questions

- [ ] Spec agent: `data-gate-length` sits on `.step` (like `data-velocity`); reconcile if the spec places it on the button.
- [ ] Spec agent: gate offs do not fire while stopped by a direct `stop()` (D4).
