# Tasks: Step Velocity (Per-Step Accents End to End)

## Review Workload Forecast

| Field | Value |
|-------|-------|
| Estimated changed lines | ~800 (range 700-900; prod ~200, tests ~490 per design, step-model landed at 1.9x its forecast) |
| 800-line budget fit | Borderline: likely at or over 800 |
| Chained PRs recommended | Yes |
| Suggested split | PR A: Phases 1-5 (C++ engine: core, player, translator, synth, export, mutation pins). PR B: Phases 6-7 (StepEdit, processor, bridge) + Phase 8 (React) |
| Delivery strategy | single-pr was preselected (800 budget); chained split is the safer alternative |
| Chain strategy | stacked: PR B branches from PR A |

Decision needed before apply: Yes (keep single-pr and accept a possible size:exception, or chain A/B as above)
Chained PRs recommended: Yes
Chain strategy: pending user decision
400-line budget risk: High

Cut order if over 800 (design; apply in this order):
1. Trim the `encodeSteps` backfill to the velocity key only.
2. Merge the MutationEngine cases into one test.
3. Merge the mock validation cases into the StepView tests.
4. Reduce the Processor MIDI-out velocity test to a single note-on (translator clamp is covered by static_assert).
5. Reduce the CSS level visual to the LED opacity only.

NEVER cut: the no-clip test (4.x), the keepPosition velocity-only test (2.x/7.x), the bit-identity guarantees.

Strict TDD: every phase lists RED (failing test) before GREEN.

### Suggested Work Units

| Unit | Goal | Phases | Focused test command | Rollback boundary |
|------|------|--------|----------------------|-------------------|
| 1 | Core, player, translator | 1-3 | `BerlinTests.exe --category=Berlin` | core/, playback/, midi/ |
| 2 | Synth, export, mutation pins | 4-6 | same | synth/, export/ |
| 3 | Edit, processor, bridge | 7-8 | same | generation/StepEdit, plugin/, bridge/ |
| 4 | React | 9 | `pnpm test` in `ui/` | ui/src only |

## Phase 0: Baseline
- [x] 0.1 Build `Tests/Builds/VisualStudio2026/BerlinTests.sln` (Debug|x64) and run `Tests/Builds/VisualStudio2026/x64/Debug/ConsoleApp/BerlinTests.exe --category=Berlin`; record count (expected 495).
- [x] 0.2 `pnpm test` and `pnpm tsc` in `ui/`; record count (expected 554).

## Phase 1: Core model (sequencing-core, step-event-scheduling data)
- [x] 1.1 RED `StepTests`: `velocity` defaults to 100, `==` sees it, aggregate `Step{note,active,skipped}` still compiles; `isValidVelocity` at 0/1/127/128 (static_assert); constants 100/1/127.
- [x] 1.2 RED `StepEventTests`: `velocity` defaults 100, `==` sees it, existing aggregate initializers still compare equal.
- [x] 1.3 GREEN `Source/core/Step.h`: `kDefaultVelocity`, `kMinVelocity`, `kMaxVelocity`, `isValidVelocity`, `Step::velocity` last, `operator==`. `Source/playback/StepEvent.h`: `velocity` last, `operator==`.

## Phase 2: Player + handoff (step-event-scheduling)
- [x] 2.1 RED `SequencePlayerTests`: note-on carries the step velocity; note-off unchanged (default 100); default sequence yields 100.
- [x] 2.2 RED `SequencePlayerHandoffTests`: keepPosition adopt with a velocity-only change on the sounding step emits no event at adopt, next pass note-on has the new velocity (D3 pin, never cut); velocity plus note change still emits note-off at offset 0.
- [x] 2.3 GREEN `SequencePlayer.cpp`: note-on `{ off, idx, step.note, true, step.velocity }`. No change to the adopt invalidation predicate.

## Phase 3: Translator (midi-output-dispatch)
- [x] 3.1 RED `MidiMessageBytesTests`: static_assert `clampNoteOnVelocity` 0->1, 100->100, 200->127, -5->1.
- [x] 3.2 (deviation: no MidiEventTranslator test target exists, the project is juce_core-only for it; covered by the Processor processBlock note-on velocity test in 7.2 and the clampNoteOnVelocity static_asserts) RED `MidiEventTranslatorTests`: note-on uses event velocity; out-of-range event velocity clamped to 1..127; `kNoteVelocity` no longer referenced.
- [x] 3.3 GREEN `MidiMessageBytes.h` add `clampNoteOnVelocity`; `MidiEventTranslator.{h,cpp}` use it, delete `kNoteVelocity`; fix remaining references (grep tests and Processor).

## Phase 4: Synth (internal-synth-voice, internal-synth-output)
- [x] 4.1 RED `SynthVoiceTests`: static_assert `velocityToGain(100) == 1.0f`, 40 -> 0.4f, 127 -> `min(1.27f, kMaxVelocityGain)`.
- [x] 4.2 RED `SynthVoiceTests` bit-identity: default `noteOn(hz)` render equals pre-change output (existing suites unchanged and green).
- [x] 4.3 RED `SynthVoiceTests` idle voice: fresh note at 127 equals the 100 render times `velocityToGain(127)` exactly, sample by sample (no ramp on an idle voice).
- [x] 4.4 RED `SynthVoiceTests` retrigger: legato retrigger 127->40 vs 127->127 twin: ratio at the retrigger sample > 0.9, and after the 2 ms ramp equals `0.4f/1.27f` within 1e-5.
- [x] 4.5 RED `SynthEngineTests` no-clip (never cut): fresh engine, default master 0.8, effects on, 44100 Hz, 512-sample blocks, 3 s, velocity-127 note-ons cycling notes 36/48/60/72 every 5512 samples with note-off at the same offset; max |sample| <= 1.0f on both channels. Reuse `makeSpec` and the shape of the test near line 301.
- [x] 4.6 RED `SynthEngineTests`: engine forwards `event.velocity` to `noteOn`.
- [x] 4.7 GREEN `SynthPatch.h` (`kMaxVelocityGain`, `velocityToGain`); `SynthVoice.{h,cpp}` `noteOn(hz, velocity = kDefaultVelocity)`, `SmoothedValue<float> velocityGain` (linear 2 ms; set up in `prepare`, 1.0 in `reset`; idle snaps via `setCurrentAndTargetValue`, sounding ramps via `setTargetValue`; `sample *= velocityGain.getNextValue()` after ADSR); `SynthEngine.cpp` passes `event.velocity`. No allocation or lock on the audio thread.
- [x] 4.8 (applied: `kMaxVelocityGain` lowered 1.27 -> 1.12; effects-on peak already exceeds 1.0 at velocity 100, see apply-progress) If 4.5 fails, lower `kMaxVelocityGain` in 0.05 steps (never below 1.0f) until it passes; update the 127 expectation in 4.1.

## Phase 5: Export (midi-export-timeline, midi-file-output)
- [x] 5.1 RED `MidiExportTimelineTests`: note-ons carry step velocity; default sequence gives all 100; note-offs unchanged; player-vs-export equivalence still holds.
- [x] 5.2 (deviation: tests live in MidiExportTimelineTests, there is no MidiFileWriterTests target) RED `MidiFileWriterTests`: `MidiFileWriter(int outputChannel)` writes per-event velocity, clamps to 1..127; default events yield byte-identical output to before.
- [x] 5.3 GREEN `MidiExportTimeline.{h,cpp}`: `MidiExportEvent::velocity` last, `==`, note-on copies `step.velocity`. `MidiFileWriter.{h,cpp}`: `explicit MidiFileWriter(int)`, drop member `velocity`, local clamp. `BerlinAudioProcessor::exportMidiTo` -> `MidiFileWriter writer (kMidiChannel)`; fix other callers/tests.

## Phase 6: Mutation pins (sequence-mutation, no production change)
- [x] 6.1 Tests `MutationEngineTests` (may be one merged test per cut order 2): `addNote` keeps the target step's velocity and copies only the note; `removeNote` leaves velocity; reverse/rotate/stretch carry it. Expected to pass at once (D9); if any fails, fix `MutationEngine.cpp`.

## Phase 7: StepEdit + processor (step-editing)
- [x] 7.1 RED `StepEditTests`: velocity 0/128 -> `velocityOutOfRange`; 1/127 ok; nullopt ok; index before velocity; note before velocity.
- [x] 7.2 RED `BerlinAudioProcessorTests`: absent keeps velocity; present sets it; out of range rejected with sequence and `edited` unchanged; validation precedes busy; edited velocity reaches processBlock note-on (`getVelocity`, single note-on per cut order 4) and the `exportMidiTo` file; existing export tests unchanged.
- [x] 7.3 GREEN `StepEdit.{h,cpp}`: `StepEditError::velocityOutOfRange`, `validateStepEdit(..., std::optional<int> velocity = {})`. `BerlinAudioProcessor.{h,cpp}`: `editStep(int,int,bool,optional<bool> = {}, optional<int> velocity = {})` with field-by-field assignment; `StepEditResult::velocityOutOfRange`.

## Phase 8: Bridge (ui-bridge)
- [x] 8.1 RED `UiBridgeTests`: `setStep velocity:127` appears in snapshot; absent keeps; 0 and 128 -> `velocity-out-of-range` with state unchanged; `"x"` and 1.5 -> `invalid type: velocity`; non-finite -> `non-finite: velocity`; ordering vs step-out-of-range, note-out-of-scale, busy; `encodeSteps` emits all 4 keys with velocity 100 (trim backfill to velocity key per cut order 1 if needed); `kCommands` still 16.
- [x] 8.2 GREEN `UiBridge.cpp`: `readOptionalInt (args, field, std::optional<int>&)` wrapping `readRequiredInt`; map `velocityOutOfRange` -> `"velocity-out-of-range"`; `encodeSteps` adds `velocity`.

## Phase 9: React (web-ui-controls)
- [x] 9.1 RED Vitest `ui/src/lib/dynamics.test.ts` (new): `nearestLevel` 55->70, 85->100, 113->100, 114->127, ties to higher; `nextLevel` ghost->soft->normal->accent->ghost.
- [x] 9.2 RED Vitest store/mock: `sendStep(store, index, step)` object form sends `{ index, ...step }`; mock stores velocity, keeps it when absent, rejects 0/128 (may merge into StepView tests per cut order 3).
- [x] 9.3 RED Vitest `StepView`: velocity button with `aria-label="Velocity step N: <level>"`, `tabIndex` roving, `data-velocity` = level name on cell; click sends `setStep{...,velocity:127}`; accent click sends ghost; skip button still works; existing callers updated.
- [x] 9.4 GREEN `ui/src/lib/dynamics.ts` (create: `VELOCITY_LEVELS`, `nearestLevel`, `nextLevel`); `bridge/limits.ts` `VELOCITY`; `bridge/protocol.ts` (`Snapshot.steps[].velocity`, `setStep.args.velocity?`); `bridge/mock.ts`; `store/actions.ts` `sendStep` refactor with `StepState`, 6 callers become `{ ...current, note }` etc.
- [x] 9.5 GREEN `components/StepView.tsx` (`velocity` prop, `.step-controls` wrapper with `.step-vel` then `.step-skip`, G/S/N/A glyph) and `styles/components.css` (LED opacity per level; extra level visuals only if budget allows, cut order 5).

## Phase 10: Build and test
- [x] 10.1 MSBuild (via vswhere) `Builds/VisualStudio2026/Berlin.sln` and `Tests/Builds/VisualStudio2026/BerlinTests.sln` Debug|x64; run `BerlinTests.exe --category=Berlin`; 0 failures, count >= 495 plus new tests; goldens and ReproducibilityTests unchanged.
- [x] 10.2 `pnpm tsc` and `pnpm test` in `ui/`; 0 failures.
- [x] 10.3 `git diff --stat`; compare with the 800 budget and record the result (apply cut order if over).

## Phase 11: Manual [M] (user)
- [x] 11.1 Standalone and plugin: edit a pattern with ghost/soft/normal/accent; hear the accents; accent does not clip at the default level.
- [x] 11.2 External MIDI monitor shows velocities 40/70/100/127 on note-on; velocity-only edit while playing causes no retrigger.
- [x] 11.3 Export a file and open it in a DAW; per-note velocities match; default pattern exports as before.

## Phase 12: Archive (separate)
- [x] 12.1 `sdd-verify`, then `sdd-archive`; diff the archive output (folder-duplication and dropped-file risk). Archived 2026-11-14 (see archive-report.md).
