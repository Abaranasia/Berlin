# Design: Step Editing (mute and note per step)

## Technical Approach

The message thread validates an edit against `currentSequence` and the current `scaleNotes`, copies the sequence, applies `{note, active}`, and publishes the copy through the existing single slot with a new `PublishMode::keepPosition`. On adoption, `SequencePlayer::process()` skips `transport.reset()` and the playhead reset. It sends a note-off only when the sounding step's content changed. Validation and `scaleNotes` live in a pure unit (`Source/generation/StepEdit`) covered by BerlinTests. React steps through the snapshot's `scaleNotes` with pure helpers (`ui/src/lib/stepEdit.ts`), sends absolute `setStep` values under per-step store keys, and retries on `busy` inside the store.

## Architecture Decisions

| # | Choice | Rejected | Rationale |
|---|---|---|---|
| D1 | `enum class PublishMode { restart, keepPosition }`. `publishSequence (Sequence&, PublishMode = restart) noexcept`. A plain `PublishMode pendingMode` member is written before the `sequencePending` release store and read after the acquire load, under the same ownership rule as `pendingSequence`. | A second slot or atomic flag; a separate `publishEdit()` | One slot keeps the single `busy` semantics and the current ordering. The default argument leaves all 4 existing callers (regenerate, mutate, loadPreset via regenerate, evolve via mutate) byte-identical: they still reset. |
| D2 | keepPosition adopt: `sequence.swap (pendingSequence)`, no `transport.reset()`, no playhead store, `loopCount` untouched. Note-off rule, evaluated on the audio thread after the swap: if `pendingNote >= 0` and (`pendingStep >= sequence.size()` or `! sequence[pendingStep].active` or `sequence[pendingStep].note != pendingNote`), push `{0, pendingStep, pendingNote, false}` first and set `pendingNote = -1`. | Sending the edited index with the slot and comparing it to `pendingStep` | `pendingNote`/`pendingStep` are audio-thread-exclusive, and the check runs on the adopted content at the adoption instant. No message-thread snapshot of the playhead can go stale. It is equivalent to "the sounding step is the edited one and its note or gate changed", and it is also safe if sizes differ. It is a compare plus at most one `push`: no allocation, no lock. A step that becomes active mid-step is not retriggered; it sounds on its next boundary. |
| D3 | Pure unit `Source/generation/StepEdit.{h,cpp}`: `kEditableSteps = 16`; `std::vector<int> scaleNotesFor (const GenerationParams&)` = `normalizePitchRange` copy of the range, then ascending notes with `Scale::fromPitchClass(...).contains(n)`; `enum class StepEditError { none, stepOutOfRange, noteOutOfScale }`; `validateStepEdit (const Sequence&, const std::vector<int>& scaleNotes, int index, int note) noexcept`. | Adding to `Scale` (core) or `PitchGenerator` | It uses the same normalization as `buildSeededSequence`, so the editable set matches what generation can produce. `Scale` stays untouched (its private ctor and "core" scope). The span is >= 12, so the list is never empty. |
| D4 | `BerlinAudioProcessor::editStep (int index, int note, bool active) -> StepEditResult {ok, busy, stepOutOfRange, noteOutOfScale}`. Order: validate (index, then note), then `isPublishPending()` → busy, then copy, assign, publish keepPosition, commit `currentSequence`, set `edited = true`. Every edit publishes, including a no-op. `regenerate()` success clears `edited` (covers regenerate, randomize, loadPreset and `setStateInformation`). `mutate()` does not touch it. | Busy first (the `regenerate` D14 order) | Neither order touches state before the publish. Validating first means the UI's busy retry only ever re-sends edits that can succeed. Spec must match this order. |
| D5 | Bridge: `handleSetStep` uses a new `readRequiredInt` (number, finite, integral, else `invalid type: <field>`; `missing arg: <field>` / `non-finite: <field>` as today), plus `readRequiredBool ("active")`. Out-of-int range values are clamped to a sentinel before casting, so they fail as range errors. Tokens: `busy`, `step-out-of-range`, `note-out-of-scale`. `kCommands` becomes 16. The snapshot adds `edited` and `scaleNotes` (from `scaleNotesFor(getGenerationParams())`). | Clamping the index or rounding the note | "Bad args" is the existing parameterised family (`missing arg:`, `invalid type:`, `non-finite:`), not one token. Absolute values must not be silently altered. |
| D6 | Store: keys `` `step.${number}` `` with optimistic `{note, active}`. Busy retry lives in `run()`: when a `setStep` result is `{ok:false, error:'busy'}`, the run ends silently (no status, overlay kept, `inFlight` kept). Then, if a newer `pending` exists, run it (latest wins). Otherwise re-run the same command after `busyRetryDelayMs` (default 25). After `busyRetryLimit` (default 40, about 1 s) it settles normally with "Busy, try again". Retryable commands are a module constant `RETRY_ON_BUSY = {'setStep'}`. | Retrying in `onSettled` in the component | `onSettled` runs after the overlay is cleared, which flickers the cell and races with new sends. Keeping the key in flight routes fast wheel/drag sends into the existing latest-wins `pending` slot. |
| D7 | UI computes from the shown value: `shown(state, 'step.i', engineStep)`. Pure helpers in `lib/stepEdit.ts`: `nextNote(scaleNotes, current, dir)` (snap rule, stops at the ends), `noteAfter(scaleNotes, start, steps)`, `classifyGesture(dy, slopPx=4)`, `dragSteps(startY, y, pxPerNote=12)`, `accumulateWheel(acc, deltaY, deltaMode) -> {acc, steps}` (100 px or 3 lines per note, carrying the remainder; up = higher note), `focusTarget(index, key, count)`. | Duplicating the scale tables in TS | `scaleNotes` is authoritative from C++ (the contract). Chained presses read the overlay, so fast input accumulates correctly. |
| D8 | `StepCell` is a `div role="button"` with roving `tabIndex` (0 on the focused cell, -1 elsewhere), `aria-pressed={active}`, and `aria-label="Step 3, D#3, on"` / `"..., muted"`. Pointer: a click without drag toggles mute, and a vertical drag sends a note per step change (mute unchanged). One non-passive `wheel` listener on the `.steps` container (`useEffect`, `{passive:false}`, `preventDefault`) resolves the cell from `data-step` and resets the accumulator when the cell changes. A muted cell shows its note dimmed (`data-gate=false`). An "Edited" badge with a title explaining the seed shows when `edited`. | A native `<button>` | A native button fires click on Space keyup and after a drag, which needs suppression logic. Explicit handlers are deterministic in jsdom. React's `onWheel` is passive, so it cannot `preventDefault`. |

## Data Flow

    StepCell gesture ─▶ lib/stepEdit (next note from scaleNotes) ─▶ store.send('step.i', setStep{abs})
       ─▶ bridge ─▶ UiBridge.handleSetStep ─▶ processor.editStep: validate ─▶ busy? ─▶ publish(keepPosition)
       ◀─ {ok, snapshot{steps, edited, scaleNotes}} ◀─┘          busy ─▶ store retry / run pending
    audio: process() adopt ─▶ keepPosition: swap, note-off iff sounding step changed, no reset

## File Changes (authored line estimates)

| File | Action | ~Lines |
|---|---|---|
| `Source/playback/SequencePlayer.{h,cpp}` | Modify: mode, keepPosition adopt | 35 |
| `Source/generation/StepEdit.{h,cpp}` | Create (pure) | 45 |
| `Source/plugin/BerlinAudioProcessor.{h,cpp}` | Modify: `editStep`, `edited`, getter | 40 |
| `Source/bridge/UiBridge.cpp` | Modify: `readRequiredInt`, handler, table, snapshot | 45 |
| `Berlin.jucer`, `Tests/BerlinTests.jucer` | Modify | 6 |
| `Tests/Source/StepEditTests.cpp` | Create | 70 |
| `Tests/Source/{SequencePlayerHandoff,BerlinAudioProcessor,UiBridge}Tests.cpp` | Modify | 140 |
| `ui/src/bridge/{protocol,mock}.ts`, `ui/src/store/{store,actions,status}.ts` | Modify | 75 |
| `ui/src/lib/stepEdit.ts` | Create | 50 |
| `ui/src/components/StepView.tsx`, `ui/src/styles/components.css` | Modify | 120 |
| `ui/src/lib/stepEdit.test.ts`, `components/StepView.test.tsx` | Create | 150 |
| `ui/src/store/store.test.ts`, `bridge/{protocol,mock}.test.ts` | Modify | 60 |

## Interfaces / Contracts

```cpp
enum class PublishMode { restart, keepPosition };
bool publishSequence (Sequence& incoming, PublishMode mode = PublishMode::restart) noexcept;
std::vector<int> scaleNotesFor (const GenerationParams&);
StepEditError validateStepEdit (const Sequence&, const std::vector<int>& scaleNotes, int index, int note) noexcept;
```
```ts
| { name: 'setStep'; args: { index: number; note: number; active: boolean } }
// Snapshot += edited: boolean; scaleNotes: number[]   ErrorToken += 'step-out-of-range' | 'note-out-of-scale'
```

## Testing Strategy (RED first, strict TDD)

| Layer | Cases |
|---|---|
| BerlinTests | Handoff: keepPosition keeps the playhead and step counter (the next boundary is step k+1, not 0); a sounding step whose note changed → note-off at offset 0 first; whose gate went off → note-off; same content → no event; another step edited → no event; nothing sounding → no event; `loopCount` unchanged; restart mode is unchanged; `static_assert noexcept`. StepEdit: C minor 36..72 list; root/scale change; inverted/narrow range normalized; index -1/16/size-limited; note out of scale/range. Processor: ok edits `currentSequence` and sets `edited`; busy/invalid → unchanged; regenerate/randomize/loadPreset clear; mutate keeps; mutate after an edit uses the edited base. Bridge: 16 commands; tokens; `invalid type: index` for 2.5/"3"; `missing arg: active`; snapshot `edited`/`scaleNotes`. |
| Vitest | `nextNote`: in-scale up/down, snap from out-of-scale both ways, ends stop. `accumulateWheel`: small deltas accumulate, remainder carried, line mode, sign. `classifyGesture`/`dragSteps`. Store: busy then ok → final value lands and no status; a newer send during retry replaces the old one; the limit is exhausted → "Busy, try again"; other commands do not retry (fake timers). StepView: click toggles, drag sends notes, wheel `defaultPrevented`, Space/Up/Down/Home/End, roving tabindex, aria attributes, dimmed muted note, edited badge. Protocol: 16 names. Mock: setStep validation. |
| Manual (WebView2) | Wheel over steps does not scroll the page; focus ring and arrow navigation; editing while playing keeps loop position (standalone and plugin); muting the sounding step cuts it with no hung note on the internal synth or external MIDI; fast wheel ends on the last note; auto-evolve on; export reflects edits. |

## Threat Matrix

N/A: no routing, shell, subprocess, VCS/PR automation, executable-file classification, or process-integration boundary.

## Work Units and Size Forecast

| # | Commit (each builds and passes) | ~Lines |
|---|---|---|
| 1 | `PublishMode` + keepPosition adopt + handoff tests | 95 |
| 2 | `StepEdit` pure unit + tests + jucer | 120 |
| 3 | `editStep`, `edited`, `setStep`, snapshot + processor/bridge tests | 175 |
| 4 | Protocol, mock, store busy retry, `sendStep` + tests | 135 |
| 5 | `lib/stepEdit`, interactive StepView, CSS + tests | 310 |

The total is about 835 authored lines, which is at or slightly over the 800 budget (the proposal's 470–600 left out most test lines). The plan stays a single PR, and test fixtures are kept tight. If the apply phase goes past 800, split after commit 3: PR A (engine, about 390) works on its own and PR B is the UI (about 445).

## Migration / Rollout

No migration. Edits are not persisted (presets and plugin state still regenerate from the seed, which clears `edited`).

## Open Questions

- [ ] D4 order (validation before busy) must match the spec's scenarios.
- [ ] The mock keeps a fixed `scaleNotes` fixture and does not recompute it on `setGenerationParams`. This is acceptable for tests and dev only.
