# Proposal: Step Editing (mute and note per step)

## Intent

The step grid is read-only. Users cannot fix one wrong note or silence one step without regenerating the whole pattern. This change lets users mute or unmute a step and move its note within the scale, directly in the StepView, while the loop keeps playing in position.

## Scope

### In Scope
- A `setStep` bridge command. The processor's `editStep` validates the index (`< min(size, 16)`), clamps the note, rejects with `busy` while a publish is pending, and is authoritative for the resulting note.
- Scale-aware note stepping: next or previous in-scale note (current scale and root), clamped to [rangeLow, rangeHigh]. An out-of-scale note snaps to the nearest in-scale note in the chosen direction.
- A keep-position handoff: no `transport.reset()`. A note-off is sent only if the sounding step is the edited one.
- An `edited` snapshot flag. Step edits set it. Regenerate, randomize and loadPreset clear it. Mutation and auto-evolve keep it.
- Interactive StepView. A click toggles mute and a drag or the wheel changes the note. Space toggles mute, Up/Down change the note, and Left/Right/Home/End move focus (roving tabindex). Muted steps show a dimmed note. A small indicator shows that the seed no longer reproduces the pattern.
- Per-step latest-wins coalescing in the store, retried on `busy`.

### Out of Scope
- Undo, confirmation dialogs, and "revert edits".
- Storing edits in presets (later slice, preset schema v4).
- Per-step evolve locks, velocity, length, and editing steps 17 to 64.
- Chromatic stepping.

## Capabilities

### New Capabilities
- `step-editing`: the edit semantics (index limit, scale stepping and snapping, range clamp, mute toggle), the `edited` flag lifecycle, and edits discarded by regenerate, randomize and loadPreset.

### Modified Capabilities
- `generation-live-control`: "Audio-Thread-Safe Regeneration Handoff" gains a keep-position publish variant with a conditional note-off.
- `sequence-mutation`: "Publish Always Resets Playback Phase" is narrowed to mutation and generation publishes. Edits become the base for the next mutation.
- `auto-evolution`: evolve runs on edited patterns. Busy collisions with edit publishes follow the existing retry rule.
- `ui-bridge`: the `setStep` command, its error tokens, and `edited` in the snapshot contents.
- `web-ui-controls`: per-step latest-wins coalescing and busy retry for `setStep`.
- `web-ui-layout`: "Step View" becomes interactive, with the dimmed muted note, the edited indicator, and keyboard focus.

`sequencing-core` is unchanged because `Step {note, active}` already carries both fields.

## Approach

This is exploration approach B. The message thread copies `currentSequence`, applies the edit, and publishes it through a flagged keep-position slot. `process()` adopts it without a reset and stays noexcept, allocation-free and lock-free. React computes the requested direction. C++ returns the final note, and the snapshot reconciles it.

## Affected Areas

| Area | Impact |
|---|---|
| `Source/playback/SequencePlayer.*` | Keep-position adopt, conditional note-off |
| `Source/` processor, bridge dispatch, snapshot | `editStep`, `setStep`, `edited` |
| `ui/src/components/StepView*`, store, `bridge/` + tests | Interaction, coalescing, indicator |

## Risks

| Risk | Likelihood | Mitigation |
|---|---|---|
| RT handoff: a stuck note or a wrong note-off when the sounding step changes | Med | Unit test the adopt path. Note-off only when the sounding index equals the edited index. |
| Busy rejections drop the last edit during a fast drag or wheel | Med | Per-step latest-wins keys with retry on `busy`. A test asserts that the final value lands. |
| Evolve reshapes or resizes the pattern between an edit and a UI refresh | Med | Index validated against the live size. The snapshot is authoritative. |
| Wheel `preventDefault` and focus behave differently in WebView2 than in jsdom | Med | Manual check in WebView2 |

## Rollback Plan

Revert the PR. Nothing is persisted (no preset or schema change), so older builds are unaffected.

## Delivery Forecast

About 470-600 lines (C++ ~200, React ~180, tests ~150, specs ~70). This fits one PR under the 800-line budget.

## Success Criteria

- [ ] Muting or changing a note during playback does not restart the loop, and no note hangs.
- [ ] Notes stay in scale and within the Pitch range. An out-of-scale note snaps in the chosen direction.
- [ ] `edited` is set by edits and cleared by regenerate, randomize and loadPreset. The indicator follows it.
- [ ] A fast wheel or drag ends on the last requested note.
- [ ] MIDI export reflects the edits.
- [ ] `BerlinTests` and Vitest pass.
- [ ] Manual check in WebView2: wheel, drag, click and keyboard on standalone and plugin, with auto-evolve on.

## Proposal question round

Auto mode. The product decisions come from the user (Engram #422). Assumptions for review:
- Mutation and evolve keep `edited` set.
- A drag maps vertical distance to scale steps.
