# Design: Step Velocity (Per-Step Accents End to End)

## Technical Approach

`Step` gains `int velocity = kDefaultVelocity` (100) as its last field, and `StepEvent` gains the same field last. The value is copied through every existing path: the builder and mutations copy whole `Step`s, `SequencePlayer` copies it into the note-on, `MidiExportTimeline` copies it into the export event, and the bridge encodes it. Only four places read it. `MidiEventTranslator` clamps it to 1..127 for MIDI out. `SynthVoice` turns it into a gain of `v/100` at note-on. `MidiFileWriter` writes it per event. React shows it as one of 4 levels. At velocity 100 every output is bit-identical to today, because `x * 1.0f == x` in IEEE 754 and the MIDI and SMF bytes already carry 100. Nothing is persisted. There are no RNG draws, so the goldens do not change.

## Architecture Decisions

| # | Choice | Rejected | Rationale |
|---|---|---|---|
| D1 | `core/Step.h`: `kDefaultVelocity = 100`, `kMinVelocity = 1`, `kMaxVelocity = 127`, `constexpr bool isValidVelocity (int)`. `Step::velocity` is appended last and `operator==` compares it. `MidiEventTranslator::kNoteVelocity` is deleted, so core holds the only definition. | Keeping `kNoteVelocity` as an alias | One definition project-wide, as midi-export D8 already required. Core is JUCE-free and every tier already includes it. |
| D2 | `StepEvent::velocity = kDefaultVelocity` is appended last and `operator==` compares it. Note-on: `{ off, idx, step.note, true, step.velocity }`. Every note-off push stays as it is and keeps the default 100, which nothing reads. | Setting note-off velocity to 0 | Existing aggregate initializers and expectations keep compiling and keep comparing equal, so there is no churn in the player tests. |
| D3 | keepPosition adopt: **no code change**. The invalidation predicate already looks only at `skipped`, `active` and `note` (`SequencePlayer.cpp:101-105`). A velocity-only change therefore sends no note-off and no retrigger, and the next boundary on that step reads `step.velocity` from the adopted sequence. A new handoff test pins this. | A "changed fields" diff mask | The rule we want is already what the code does. A test is enough to lock it in. |
| D4 | `MidiMessageBytes.h`: `constexpr int clampNoteOnVelocity (int v)` returns a value in 1..127. The translator calls `makeNoteOn (channel, note, clampNoteOnVelocity (event.velocity))`. | Clamping to 0..127 | A note-on with velocity 0 is a note-off on the wire. |
| D5 | Synth: `SynthPatch.h` gets `kMaxVelocityGain = 1.27f` and `constexpr float velocityToGain (int v)`, which returns `min (clamp(v,1,127) / 100.0f, kMaxVelocityGain)`. `SynthVoice::noteOn (float hz, int velocity = kDefaultVelocity)`. The voice keeps a member `juce::SmoothedValue<float> velocityGain` (linear, 2 ms, set up in `prepare`, reset to 1.0 in `reset`). At note-on, an idle voice (`!adsr.isActive()`) snaps to the new gain with `setCurrentAndTargetValue`; a sounding voice ramps with `setTargetValue`. `render`: `sample *= velocityGain.getNextValue()` right after the ADSR multiply. `SynthEngine` passes `event.velocity`. | A bare latched float; a `v/127` curve | `100/100.0f` is exactly 1.0f, and an unsmoothed SmoothedValue returns its target exactly, so velocity 100 is bit-identical. A legato retrigger from 127 to 40 is a 0.87x gain step at sustain level, which would click. The snap keeps a fresh note's 127/100 ratio exact for testing. The ceiling is the fallback named in rule 6: if the no-clip test fails, lower `kMaxVelocityGain` in 0.05 steps until it passes. It always stays >= 1.0f, so 100 is unaffected. |
| D6 | Export: `MidiExportEvent::velocity` is appended last and compared by `==`. The timeline note-on is `{ tick, step.note, true, step.velocity }`. `MidiFileWriter` becomes `explicit MidiFileWriter (int outputChannel)`, the member `velocity` is removed, and note-on uses a local clamp of `event.velocity` to 1..127. The only caller, `BerlinAudioProcessor::exportMidiTo` (line 335), becomes `MidiFileWriter writer (kMidiChannel)`. | Keeping the constructor argument as a fallback | It is redundant once every event carries a velocity. Default sequences produce byte-identical files because each event carries 100. |
| D7 | Editing: `StepEditError::velocityOutOfRange`. `validateStepEdit (seq, scaleNotes, index, note, std::optional<int> velocity = {})` checks index, then note, then velocity. The processor checks busy after that. `editStep (int, int, bool, std::optional<bool> skipped = {}, std::optional<int> velocity = {})` assigns field by field (`s.note = …; s.active = …; if (skipped) …; if (velocity) …`) instead of rebuilding `Step{…}`. `StepEditResult::velocityOutOfRange` is added. | An options struct for the optional fields | Existing 4-argument call sites stay untouched. step-gate appends `std::optional<int> gate = {}` and one assignment line. Field-by-field assignment means a new field can never be reset by accident. |
| D8 | Bridge: a new `readOptionalInt (args, field, std::optional<int>&)` wraps `readRequiredInt` when the property is present. That gives the existing tokens for free: fractional or non-number gives `invalid type: velocity`, non-finite gives `non-finite: velocity`. `velocityOutOfRange` maps to `"velocity-out-of-range"`. `encodeSteps` adds `velocity`. `kCommands` stays at 16. | Range-checking in the bridge | Range lives in the processor, so the validation order index, note, velocity, busy holds in a single place. step-gate reuses `readOptionalInt`. |
| D9 | Mutations: **no production change**. `addNote` assigns only `.active` and `.note` (`MutationEngine.cpp:195-196`), so the target step keeps its own velocity. `removeNote` assigns only `.active`. Transforms copy whole `Step`s. Tests pin all three. | — | Rule 8 already holds. |
| D10 | React: `ui/src/lib/dynamics.ts` exports `VELOCITY_LEVELS = [{name:'ghost',value:40},{soft,70},{normal,100},{accent,127}]`, `nearestLevel (v)` (smallest distance, ties go to the higher level) and `nextLevel (v)` (the level after `nearestLevel(v)`, so accent wraps to ghost). `limits.ts` gets `VELOCITY = {min:1,max:127}` for the mock. | Raw numeric velocity in the UI | The 4 levels follow Franke; 8 levels later is a table edit. |
| D11 | `sendStep (store, index, step: StepState)`, where `StepState = Snapshot['steps'][number]`, sends `{ index, ...step }` with an absolute value for every field. The 6 callers become `sendStep (store, i, { ...current, note })` and similar. `StepCell` gets a `velocity` prop. The cell gets `data-velocity={level name}`. The slot becomes `.step` plus `<div class="step-controls">` holding `<button class="step-vel">` and then the existing `.step-skip`. The velocity button is `tabIndex={focusable ? 0 : -1}` with `aria-label="Velocity step N: <level>"`, shows a G, S, N or A glyph, and on click sends `nextLevel`. | A nested button; a modifier-click | This reuses the skip-button a11y pattern. step-gate adds a third button inside `.step-controls`, with no layout churn. |

## Data Flow

    Vel button ─▶ sendStep{…,velocity} ─▶ setStep ─▶ readOptionalInt ─▶ editStep ─▶ validate(index,note,velocity) ─▶ busy ─▶ publish(keepPosition)
    audio: adopt (no off for a velocity-only edit) ─▶ boundary ─▶ StepEvent{…,velocity} ─┬▶ translator clamp 1..127 ─▶ MIDI out
                                                                                        └▶ SynthEngine ─▶ voice.noteOn(hz,v) ─▶ gain v/100
    export: timeline event.velocity ─▶ MidiFileWriter(channel) per-event velocity

## File Changes (authored line estimates, added+deleted)

| File | Action | ~Lines |
|---|---|---|
| `Source/core/Step.h`, `Source/playback/{StepEvent.h,SequencePlayer.cpp}` | Modify | 16 |
| `Source/midi/{MidiMessageBytes.h,MidiEventTranslator.h,.cpp}` | Modify | 10 |
| `Source/synth/{SynthPatch.h,SynthVoice.h,SynthVoice.cpp,SynthEngine.cpp}` | Modify | 22 |
| `Source/export/{MidiExportTimeline.h,.cpp,MidiFileWriter.h,.cpp}` | Modify | 18 |
| `Source/generation/StepEdit.{h,cpp}`, `Source/plugin/BerlinAudioProcessor.{h,cpp}`, `Source/bridge/UiBridge.cpp` | Modify | 40 |
| `ui/src/lib/dynamics.ts` | Create | 25 |
| `ui/src/{bridge/protocol,bridge/limits,bridge/mock,store/actions}.ts` | Modify | 18 |
| `ui/src/components/StepView.tsx`, `styles/components.css` | Modify | 50 |
| C++ tests (Step, StepEvent, MidiMessageBytes, SequencePlayer, Handoff, SynthVoice, SynthEngine, MidiExportTimeline, MutationEngine, StepEdit, Processor, UiBridge) | Modify | 360 |
| Vitest (dynamics new, StepView, mock, store) | Modify/Create | 130 |

Production is about 200 lines and tests about 490, roughly 690 in total. Honest range: **700-900**, since step-model landed at 1.9x its forecast and tests dominated then too. Cut order to stay at or under 800:
1. Trim the `encodeSteps` backfill to the velocity key only.
2. Merge the MutationEngine cases into one test.
3. Merge the mock validation cases into the StepView tests.
4. Reduce the Processor MIDI-out velocity test to a single note-on (the translator clamp is covered by static_assert).
5. Reduce the CSS level visual to the LED opacity only.

Never cut the no-clip test, the keepPosition test, or the bit-identity guarantees.

## Interfaces / Contracts

```cpp
struct Step { int note = 0; bool active = false; bool skipped = false; int velocity = kDefaultVelocity; };
struct StepEvent { int sampleOffset, stepIndex, note; bool isNoteOn; int velocity = kDefaultVelocity; };
struct MidiExportEvent { long long tick; int note; bool noteOn; int velocity = kDefaultVelocity; };
constexpr int   clampNoteOnVelocity (int) noexcept;         // midi/MidiMessageBytes.h
constexpr float velocityToGain (int) noexcept;              // synth/SynthPatch.h
void SynthVoice::noteOn (float frequencyHz, int velocity = kDefaultVelocity) noexcept;
explicit MidiFileWriter (int outputChannel) noexcept;
StepEditResult editStep (int, int, bool, std::optional<bool> skipped = {}, std::optional<int> velocity = {});
```
```ts
Snapshot.steps: { note; active; skipped; velocity: number }[]
| { name: 'setStep'; args: { index; note; active; skipped?: boolean; velocity?: number } }
export type StepState = Snapshot['steps'][number];
```

## Testing Strategy (strict TDD, RED first in this order)

| # | Suite | Failing cases first |
|---|---|---|
| 1 | StepTests, StepEventTests, MidiMessageBytesTests | velocity defaults to 100, `==` sees it, aggregate holds; `isValidVelocity` at 0/1/127/128; static_assert `clampNoteOnVelocity` 0→1, 100→100, 200→127 |
| 2 | SequencePlayerTests, Handoff | a note-on carries the step velocity and the note-off is unchanged. keepPosition, velocity-only change on the sounding step: no event at adopt, and the next pass note-on has the new velocity. Velocity plus note change still gives a note-off at offset 0. |
| 3 | SynthVoiceTests | static_assert `velocityToGain(100) == 1.0f`, 40 → 0.4f, 127 → `min(1.27f, ceiling)`. A fresh note at 127 equals the 100 render × `velocityToGain(127)` **exactly**, sample by sample. Legato retrigger 127→40 against a 127→127 twin: the ratio at the retrigger sample is above 0.9, and once the ramp has finished it equals `0.4f/1.27f` within 1e-5. |
| 4 | SynthEngineTests (reuse the `makeSpec` helper and the shape of the existing no-clip test at line 301) | Fresh engine, default master 0.8 (not overridden), effects on, 44100 Hz, 512-sample blocks, 3 s. Velocity-127 note-ons cycle through notes 36/48/60/72 every 5512 samples, with a note-off at the same offset. Assert max \|sample\| ≤ 1.0f on both channels. |
| 5 | MidiExportTimelineTests | note-on events carry the step velocity; a default sequence gives all 100; note-offs are unchanged |
| 6 | MutationEngine, StepEdit | `addNote` keeps the target step's velocity and copies only the note; `removeNote` leaves velocity; reverse/rotate carry it. Velocity 0/128 gives `velocityOutOfRange`, 1/127 are ok; index before velocity, note before velocity; nullopt is ok. |
| 7 | Processor | velocity absent keeps it; present sets it; out of range is rejected with the sequence and `edited` unchanged; validation precedes busy; an edited velocity reaches the processBlock note-on (`getVelocity`) and the `exportMidiTo` file. The existing export tests stay green unchanged. |
| 8 | UiBridge | `setStep velocity:127` appears in the snapshot; absent keeps it; 0 and 128 give `velocity-out-of-range` with state unchanged; `"x"` and 1.5 give `invalid type: velocity`; ordering against step-out-of-range, note-out-of-scale and busy; `encodeSteps` emits all 4 keys with velocity 100; 16 commands. |
| 9 | Vitest | `nearestLevel` (55→70, 85→100, 113→100, 114→127), `nextLevel` wraps; StepView renders the velocity button with its aria-label and `data-velocity`, and a click sends `setStep{…,velocity:127}`; accent→ghost; mock rejects 0/128 and keeps velocity when absent; store `sendStep` object form |

Default neutrality is proven by every existing suite (goldens, ReproducibilityTests, synth bit-identity, export) running unchanged. Manual: listen to accents in standalone and plugin, play them on an external MIDI monitor, and check the exported file in a DAW.

## Threat Matrix

N/A: no routing, shell, subprocess, VCS/PR automation, executable-file classification, or process-integration boundary.

## Migration / Rollout

No migration and no schema change. Velocity edits reset on reload; this is a documented non-goal.

## Open Questions

- [ ] `data-gate` already means the on/off flag on `.step`. step-gate must use a different attribute (for example `data-gate-length`). This is noted here so step-gate does not collide with it.
- [x] `data-velocity` holds the level name (ghost/soft/normal/accent). This matches spec rule 9, reconciled by the orchestrator on 2026-10-17.
- [x] 2 ms gain smoother: the spec internal-synth-voice was amended to allow it. The target is latched at note-on, an idle voice jumps straight to the target, and the ramp runs only when retriggering a sounding voice. Reconciled by the orchestrator on 2026-10-17.
- [x] `kNoteVelocity` deletion: compatible with the spec, because the translator uses the event velocity. Reconciled.
