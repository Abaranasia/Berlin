# Design: Step Model (Variable Length + True Skip)

## Technical Approach

`Step` gains `bool skipped`. One pure, JUCE-free helper (`Source/core/StepCursor.h`) defines "the next non-skipped step after X", and both `SequencePlayer` and `MidiExportTimeline` use it, so they cannot drift apart. The player stops deriving the step index from `stepCounter % size`. Instead it keeps an audio-thread `cursor` (the last played index), so Transport only supplies timing. `GenerationParams.length` feeds all three generators. At length 16 the constructor arguments are identical to today's, so RNG draws and goldens do not change. The bridge and React carry `skipped` and `length`. Nothing is persisted.

## Architecture Decisions

| # | Choice | Rejected | Rationale |
|---|---|---|---|
| D1 | `kMaxSteps = 64` lives in `core/Sequence.h`. `MutationEngine`'s literal `64` uses it. `kNumSteps = 16` stays as the default length. | A new constant in `GenerationParams.h` or `StepEdit.h` | Core is included by every producer and consumer: generation, mutation, player, bridge. Keeping `kNumSteps` leaves `ReproducibilityTests` untouched. |
| D2 | `StepCursor.h` (header-only, inline, noexcept): `StepAdvance nextPlayableStep (const Sequence&, int from)` scans `from+1 ..` modulo size, at most `size` probes. It returns `{index, wrapped}`. `index = -1` means empty or all-skipped. `wrapped = from >= 0 && index <= from`. `int countPlayableSteps (const Sequence&)`. Precondition: `-1 <= from < size`. | Duplicating the rule in each consumer; a member on `Sequence` | It is one testable definition of rule 1 (wrap = crossing from the last non-skipped step to the first). A skipped step 0 is irrelevant, and a single playable step wraps on every boundary. Header-only means no `.jucer` link registration. |
| D3 | `SequencePlayer`: a new audio-thread member `int cursor = -1`. Per boundary: note-off first (unchanged), then `adv = nextPlayableStep (sequence, cursor)`. If `adv.index < 0` there is no event, no loop and no playhead store. Otherwise `if (adv.wrapped) ++loopCount`, emit note-on if active, `cursor = adv.index`, `playhead = cursor`. The `previousStep`/playhead wrap test is deleted. | Keeping `stepCounter % size` and skipping indices (time would still pass); a cursor stored as "next index" | Skipped steps take no boundary and inactive steps still take one (rule 3). The old post-adopt false-wrap exclusion becomes `cursor = -1` on restart-adopt, `prepare()` and `reset()`: the first boundary after `-1` can never be a wrap. The cost is O(size <= 64) per boundary, with no allocation or lock. |
| D4 | keepPosition adopt, after the swap: `if (cursor >= size \|\| (cursor >= 0 && sequence[cursor].skipped && nextPlayableStep (sequence, cursor).wrapped)) cursor = -1;`. The note-off rule gains `\|\| sequence[pendingStep].skipped`. The playhead and `loopCount` are untouched. | Snapping the cursor to 0; a "suppress next wrap" flag | This implements rule 4 exactly. The cursor continues; out-of-range or a skipped cursor whose continuation would wrap goes to the first non-skipped step with no loop counted, and nothing changes otherwise. A skipped sounding step is cut, the same as a muted one. |
| D5 | `MidiExportTimeline`: `totalSteps = repeats * countPlayableSteps(seq)`. Each k advances `cursor = nextPlayableStep(seq, cursor).index` from `-1`. Ticks stay `k * ticksPerStep`. All-skipped returns `ok`, zero events and `endTick 0` (the existing empty-Sequence path). | An independent `for r / for i / if skipped continue` | It uses the same function as the player (rule 9). The cross-check test proves equivalence. |
| D6 | `SequenceBuilder`: `length = clamp (params.length, 1, kMaxSteps)`. Random mode: `SkipMaskGenerator (length, randomActiveCount (length))` with `randomActiveCount (n) = max (1, (n*11 + 8) / 16)`, exported from `SequenceBuilder.h`. Euclidean: `(length, pulses, rotation)`; the generator already clamps and wraps. Probability: `length` draws. | Scaling inside `SkipMaskGenerator` | At length 16 the args are `(16, 11)`, which gives 5 draws, exactly today's call. The generator class stays untouched, and `active` keeps meaning a rest; generators never set `skipped`. |
| D7 | Bridge: add `{"length", 1, kMaxSteps}` to `kGenerationIntFields` and widen pulses to `0..kMaxSteps` and rotation to `0..kMaxSteps-1`. After the int loop, clamp `pulses` to `[0, length]` and `rotation` to `[0, length-1]`. `setStep` gets an optional `skipped`: if present, `readRequiredBool`, else `std::nullopt`. `encodeSteps` adds `skipped`. `kCommands` is unchanged. | Rejecting out-of-range length; a new `setSkip` command | Clamp matches every other generation field. With one command, the static_assert and the protocol name list stay as they are. |
| D8 | Processor: `editStep (int, int, bool, std::optional<bool> skipped = {})` keeps the step's current `skipped` when absent. `length` is a STAGED generation param like every other field: `setGenerationParams` commits it (clamped) without rebuilding; it applies on the next Generate / Randomize / New Seed / preset load, which regenerate from the current seed at the live length. No new processor entry point and no new busy path. `validateStepEdit` drops `kEditableSteps` and checks `0 <= index < size`. | Regenerating immediately on a length change (rejected at spec reconciliation: it would be the only param that rebuilds on set, needs a new busy path, and silently clears hand edits) | Rule 5 as reconciled with spec generation-live-control "Sequence Length Control" (orchestrator decision 2026-10-17). |
| D9 | Persistence: no change. `loadPreset` and `setStateInformation` assign the 4 persisted fields individually, so `length` (like `mode` and `pulses`) survives and `regenerate (false)` rebuilds at the current length. This was verified in `BerlinAudioProcessor.cpp`. | Schema v4 | Rule 10; a test pins it. |
| D10 | `MutationEngine`: `addNote`/`removeNote` exclude skipped indices from both candidate lists. Copy transforms already copy whole `Step`s. | Leaving them as they are | Rule 8. With no skips, the lists and draws are unchanged, so the goldens are safe. |
| D11 | React: `GenerationParams.length` and `steps[].skipped` in the protocol. `limits.ts` gets length `1..64`, pulses `0..64`, rotation `0..63`. `steps.ts` replaces `STEP_COUNT` with `ROW_SIZE = 16` and `stepRow (playhead, playing, count)`. `StepView` renders `steps.length` cells, or 16 placeholders before the first snapshot. CSS `repeat(16, …)` already wraps into rows. Each slot is a `.step` plus a sibling `<button class="step-skip" aria-pressed aria-label="Skip step N">`; the cell gets `data-skipped`. The focus query uses `.steps`. `sendStep (store, i, note, active, skipped)` always sends absolute `skipped`. `RhythmPanel` adds a Length slider (`sendGen 'length'`) and caps pulses/rotation at `length`/`length-1`. The mock resizes steps on `length` and accepts `skipped`. | A modifier-click for skip (hidden); a nested button inside `role=button` (invalid a11y) | It reuses the existing lane, overlay and busy-retry machinery. |

## Data Flow

    Length slider ─▶ setGenerationParams{length} (staged) ; Generate ─▶ regenerate(false) at live length ─▶ publish(restart)
    Skip button  ─▶ setStep{index,note,active,skipped} ─▶ editStep ─▶ publish(keepPosition)
    audio: adopt (D4 cursor fix) ─▶ boundary ─▶ nextPlayableStep(cursor) ─▶ event / loop++ / playhead
    export: countPlayableSteps × repeats ─▶ nextPlayableStep ─▶ ticks        (same helper)

## File Changes (authored line estimates)

| File | Action | ~Lines |
|---|---|---|
| `Source/core/{Step.h,Sequence.h}`, `Source/core/StepCursor.h` | Modify / Create | 45 |
| `Source/generation/{GenerationParams.h,SequenceBuilder.{h,cpp},MutationEngine.cpp,StepEdit.{h,cpp}}` | Modify | 35 |
| `Source/playback/SequencePlayer.{h,cpp}`, `Source/export/MidiExportTimeline.cpp` | Modify | 40 |
| `Source/plugin/BerlinAudioProcessor.{h,cpp}`, `Source/bridge/UiBridge.cpp` | Modify | 45 |
| `Berlin.jucer`, `Tests/BerlinTests.jucer` (header entry) | Modify | 4 |
| `ui/src/{bridge/protocol,bridge/limits,bridge/mock,state/steps,store/actions,store/store}.ts` | Modify | 35 |
| `ui/src/components/{StepView,RhythmPanel}.tsx`, `styles/components.css` | Modify | 60 |
| C++ tests (Step, Sequence, SequenceBuilder, SequencePlayer, Handoff, MidiExportTimeline, MutationEngine, StepEdit, Processor, UiBridge) | Modify | 300 |
| Vitest (steps, StepView, RhythmPanel, mock, protocol) | Modify | 120 |

Total is about 685 lines, under 800. If it grows, cut the RhythmPanel dynamic caps first (the bridge clamps anyway), then the mock resize.

## Interfaces / Contracts

```cpp
struct Step { int note = 0; bool active = false; bool skipped = false; };   // aggregate; == compares all 3
inline constexpr int kMaxSteps = 64;                                        // core/Sequence.h
struct StepAdvance { int index; bool wrapped; };
StepAdvance nextPlayableStep (const Sequence&, int from) noexcept;          // core/StepCursor.h
int countPlayableSteps (const Sequence&) noexcept;
int randomActiveCount (int length) noexcept;                                // SequenceBuilder.h
StepEditResult editStep (int index, int note, bool active, std::optional<bool> skipped = {});
```
```ts
GenerationParams += { length: number }; Snapshot.steps: { note; active; skipped: boolean }[]
| { name: 'setStep'; args: { index: number; note: number; active: boolean; skipped?: boolean } }
```

## Testing Strategy (strict TDD, RED first in this order)

| # | Suite | Failing cases first |
|---|---|---|
| 1 | StepTests, SequenceTests | `skipped` defaults false, `==` sees it. `nextPlayableStep`: from -1 gives the first non-skipped; skipped step 0 wraps to the next playable with `wrapped`; single playable step wraps every call; all-skipped and empty give -1. `countPlayableSteps`. |
| 2 | SequenceBuilderTests | `randomActiveCount` 16→11, 1→1, 64→44; length 1/8/64 sizes in each mode; no generated skips; length 16 equals the default output (ReproducibilityTests stay unchanged and green). |
| 3 | SequencePlayerTests / Handoff | Skipped steps take no boundary; inactive steps take one; loop counted at the wrap with skipped step 0; restart-adopt does not count; all-skipped is silent with loopCount 0 and no crash; length 1 counts each boundary. keepPosition: cursor continues; shrink below cursor goes to the first playable without a loop; skipped cursor with a forward playable continues; skipped cursor needing a wrap gives no count; never resets to 0; a skipped sounding step gives note-off at offset 0. |
| 4 | MidiExportTimelineTests | Zero ticks for skipped; all-skipped gives ok, empty, endTick 0; the equivalence test is extended with a skip/rest mix that compares player offsets to export ticks. |
| 5 | MutationEngine, StepEdit | `addNote`/`removeNote` never pick skipped; stretch/palindrome carry the flag; index 16..63 is valid within size; index == size is rejected. |
| 6 | Processor, UiBridge | `skipped` absent keeps the flag; setGenerationParams{length} does not change the playing sequence; the next Generate rebuilds at that length from the same seed; Randomize and loadPreset keep the live length. `length` 0→1 and 99→64; pulses clamped to length; `setStep skipped:"x"` gives `invalid type: skipped`; snapshot has `skipped`/`length`; 16 commands. |
| 7 | Vitest | `stepRow` with count; StepView renders 24 cells for 24 steps; the skip button sends `setStep{…skipped:true}`; `data-skipped`; Length slider sends `gen.length`; mock `setStep` skipped. |

Manual: lengths 1/7/64 in standalone and plugin; skip while playing; export matches what is heard.

## Threat Matrix

N/A: no routing, shell, subprocess, VCS/PR automation, executable-file classification, or process-integration boundary.

## Migration / Rollout

No migration and no schema change. Skip and length edits reset on reload; that is the documented non-goal.

## Open Questions

- [x] Length-1 sequences count a loop on every boundary (today they never count). This follows from rule 1 and is accepted; tasks must add a test for it.
- [x] Busy length change: moot. Length is staged (D8), so there is no new busy path.
