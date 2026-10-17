# Exploration: step-model

Source of truth for this phase is the Engram observation `sdd/step-model/explore`. This is the openspec mirror (hybrid store).

## Goal

The project's original intent is a generator of Berlin School sequences and arpeggios. This change adapts the sourced research (Moog 960 per-step play/skip switches; Franke's accent levels and gate lengths) to the existing engine:
- variable sequence length
- true skip steps (removed from the cycle, not just muted)
- per-step velocity and gate length

## Current state (verified)

- **Step model:** `Step{int note; bool active}` is an aggregate. `Sequence::operator==` compares `Step`s.
- **Generators:** all three rhythm modes use `kNumSteps = 16`. Random mode mutes 5 of the 16 steps through `SkipMaskGenerator` (`kActiveSteps = 11`). `buildSeededSequence` draws pitches from the same RNG, so any new draw changes existing seeds.
- **Player:** `stepIndex = stepCounter % sequence.size()`. There is one `pendingNote`, and its note-off is sent at the next boundary, so every gate is a full step. The loop count increments on `stepIndex == 0 && previousStep != 0`.
- **Output:**
  - `StepEvent` has no velocity.
  - `MidiEventTranslator::kNoteVelocity = 100` is fixed.
  - `SynthEngine` drops events whose offset is lower than its cursor.
  - `SynthVoice::noteOn(freq)` has no velocity input.
  - `StepEventBuffer` holds 64 events.
- **Export:** `MidiExportTimeline` duplicates the player's logic in ticks; its header says a gate change must update both. `MidiFileWriter` takes one fixed velocity.
- **Persistence:** `kSchemaVersion = 3`. It stores seed, scale, range, bpm and patch. Rhythm mode, pulses, rotation, probability and step edits are NOT persisted.
- **Edit path:** `editStep` publishes `Step{note, active}` with `keepPosition`. `kEditableSteps = 16`.
- **Bridge and UI:**
  - `encodeSteps` sends `{note, active}`.
  - `setStep` takes `{index, note, active}`.
  - `kCommands` has a `static_assert` of 16.
  - React hard-codes `STEP_COUNT = 16` in `ui/src/state/steps.ts`.
  - `StepView` renders 16 cells.
- **Mutations:** the copy-based transforms copy whole `Step`s, and lengths are clamped to 4..64. `addNote` and `removeNote` act on `active`.

## Touchpoints

- **Core:** `core/Step.h`, `Sequence.cpp`.
- **Generation:** `GenerationParams.h`, `SequenceBuilder.cpp`, `StepEdit.h/.cpp`, `MutationEngine.cpp`.
- **Playback:** `StepEvent.h`, `SequencePlayer.cpp`.
- **Output:** `MidiEventTranslator`, `SynthEngine`, `SynthVoice`.
- **Export:** `MidiExportTimeline`, `MidiFileWriter`.
- **Processor:** `BerlinAudioProcessor.cpp`.
- **Presets:** `Preset.h`, `PresetManager` (only if persisted).
- **Bridge:** `UiBridge.cpp`.
- **UI:** `ui/src/bridge/protocol.ts`, `state/steps.ts`, `components/StepView.tsx` and its CSS, `store/actions.ts`, `store/status.ts`.
- **Tests:** the matching C++ and Vitest suites.

## Real-time safety

The publish mechanism stays an O(1) vector swap behind an atomic flag; POD fields on `Step` don't change that.

**Skip cursor:**
- An audio-thread-only `nextIndex` scans forward, bounded by the sequence size, with no allocation.
- Crossing the end of the sequence counts as the wrap, which drives `loopCount`.
- An all-skipped sequence plays silently and is safe.

**Gate below 100%:** a pending note-off is scheduled at an absolute sample position and emitted in offset order, before the next boundary's events. The off must land at least 1 sample after the on.

**Determinism:** velocity and gate add no new RNG draws.

## Options

1. **Skip:** A, a `bool skipped` flag (recommended); B, a `StepState` enum; C, a separate mask.
2. **Length:** A, `GenerationParams.length` (default 16, range 1..32), with random mode's active count scaled so length 16 stays byte-identical (recommended); B, length derived from mutations only.
3. **Gate:** A, an integer percent of the step where 100 means legacy (recommended); B, ticks.
4. **Velocity:** store 1..127, with the UI snapping to 4-8 accent levels.

## Size

About 1100-1300 lines including tests, which is over the 800 budget. The natural cut is: (1) length + skip, (2) velocity + gate. Persistence is deferred.

## Risks

- Edits are lost on reload because they aren't persisted.
- The `loopCount` wrap rule feeds auto-evolve.
- The player and export timing logic are duplicated and must stay equivalent.
- Gate event ordering: a wrong order silently drops events in `SynthEngine`.
- 16 is hard-coded in several places.
- `addNote`/`removeNote` need a rule for skipped steps.
