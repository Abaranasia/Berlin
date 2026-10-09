# Exploration: step-editing

Mirror of Engram `sdd/step-editing/explore` (id 421).

## Current State

- **Model**: `Step { int note /*MIDI*/; bool active /*gate*/; }` (`Source/core/Step.h`). No velocity or length. `Sequence` wraps `std::vector<Step>`; generation is fixed at 16 steps, but stretch/compress/palindrome mutations can resize it (4 to 64).
- **Handoff**: `SequencePlayer::publishSequence` is a single-slot lock-free swap that returns busy while a publish is pending. On adoption `process()` sends a note-off, swaps, and calls `transport.reset()` (`SequencePlayer.cpp:89`), restarting the loop at step 0. Reusing it per edit would restart the loop on every click.
- **Processor**: owns `currentSequence`, `sequenceSeed`, `mutationCount`. `regenerate`, `mutate` and auto-evolve all publish whole sequences; mutations operate on `currentSequence`, so edits become the base for the next mutation.
- **Bridge/UI**: 15 dispatch commands; the snapshot already carries per-step `{note, active}`, `generationParams` (scale, root, range) and seed. The playhead is a separate 30 Hz event. `StepView`/`StepCell` are read-only, with no handlers, tabIndex or tests.
- **Persistence**: presets (schema v3) store seed, scale, root, range, patch and bpm, not the steps. MIDI export uses `currentSequence`, so edits export as shown.
- **Undo**: none exists anywhere.

## Approaches

| | Scope | Lines |
|---|---|---|
| A. UI-only | Not viable: no command can set a step | n/a |
| **B. Engine command + position-preserving handoff + interactive StepView (recommended)** | `setStep` bridge command, processor `editStep` with busy rejection and clamping, keep-position publish, keyboard/wheel/drag/click in StepView | ~470-600 |
| B-lite | B with mute/unmute only | ~300 |
| C. B + per-step evolve locks + undo stack + steps in presets (schema v4) | Everything | ~1,100+ (split needed) |

## Decisions for the user

1. Loop keeps playing in position during edits (assumed yes).
2. Auto-evolve on an edited pattern: leave it, pause briefly after edits, or stop on first edit.
3. Regenerate/randomize/load preset discard edits: OK as-is, confirm, or offer "revert edits".
4. Presets remembering edits (schema v4, separate slice).
5. Note stepping: scale-aware vs chromatic; clamp to the pitch range or not.
6. Plain click: mute/unmute vs select only.
7. Muted steps show their note dimmed.
8. Undo in v1; editing limited to the visible 16 steps.

## Risks

- Audio-thread change in `process()`: must stay noexcept, allocation-free and lock-free, with a correct note-off when the sounding step is edited.
- Busy rejections during fast drags/wheel: per-step store keys with latest-wins; the last edit must land.
- Auto-evolve can reshape or resize the pattern between an edit and the UI refresh.
- Seed/preset reproducibility silently broken by edits unless an `edited` indicator exists.
- Wheel `preventDefault` and keyboard focus need checking in WebView2, not only jsdom.
