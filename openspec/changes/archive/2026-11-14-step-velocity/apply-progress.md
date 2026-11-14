# Apply Progress: step-velocity

Status: phases 0-10 done (36 tasks marked [x]); Phase 11 (manual) and 12 (archive) pending. Nothing committed or staged.

## Results

- C++ (`BerlinTests.exe --category=Berlin`): 495 -> 522 test cases, exit 0, 0 failures. `Berlin.sln` Debug|x64 builds with no errors (no LNK1168, so no scratch OutDir was needed).
- Vitest: 554 -> 567 passing (40 files). `pnpm tsc`: clean.
- Diff size (excluding openspec): tracked `git diff --shortstat` 1060 insertions + 121 deletions in 43 files, plus 54 lines in the 2 new files (`ui/src/lib/dynamics.ts`, `ui/src/lib/dynamics.test.ts`) = about 1235 changed lines. Over the 800 budget and the ~900 mark; size:exception was pre-accepted. Cut-order items 2 and 3 were applied as written (merged mutation test, mock validation inside the mock tests) and item 5 (CSS reduced to LED opacity plus an accent tint on the velocity button). Tests dominate.

## TDD evidence

| Phase | Test files | RED | GREEN |
|---|---|---|---|
| 1 | StepTests, StepEventTests | compile failure (no kDefaultVelocity / velocity) | 499 cases |
| 2 | SequencePlayerTests, SequencePlayerHandoffTests | 3 runtime failures | 503 cases, 0 fail |
| 3 | MidiMessageBytesTests | compile failure (no clampNoteOnVelocity) | 504 |
| 4 | SynthVoiceTests, SynthEngineTests | compile failure (no velocityToGain); golden fingerprint captured BEFORE the voice changed | 511, after the ceiling change |
| 5 | MidiExportTimelineTests | compile failure; one test-index bug fixed in the test | 514 |
| 6 | MutationEngineTests | pins passed at once (D9, no production change) | 515 |
| 7 | StepEditTests, BerlinAudioProcessorTests | compile failure (5-arg validateStepEdit / editStep) | 520 |
| 8 | UiBridgeTests | runtime failures (velocity key absent, no velocity arg) | 522 |
| 9 | dynamics.test.ts (new), store.test.ts, mock.test.ts, StepView.test.tsx | module missing / 6 + 5 + 18 runtime failures | 567 |

## Deviations and findings (read before verify)

1. No-clip with effects ON cannot hold, even at velocity 100. Measured with the design's pattern (default master 0.8, 44.1 kHz, 512 blocks, notes 36/48/60/72): effects off peak at velocity 127 with gain 1.27 = 1.097; effects on peak at velocity 100 = 1.99 (a single sustained note gives 1.10-1.23 at velocity 100). This is pre-existing effects gain staging (the "standalone clipping follow-up"). The design fallback (lower the ceiling, never below 1.0, velocity 100 stays exactly 1.0) cannot fix it. What was done: `kMaxVelocityGain` lowered 1.27 -> 1.12 (0.05 steps: 1.22, 1.17 gives 1.011, 1.12 gives 0.967) so the effects-bypassed default state never clips an accent (test asserts <= 1.0). With effects on the test asserts the achievable relative guarantee: accent peak <= velocity-100 peak x ceiling. The spec scenario "Accent at the default level does not clip" with effects enabled is therefore NOT literally met; it needs a decision (fix effects gain staging in a follow-up, or amend the spec to effects-bypassed). `velocityToGain(127)` = 1.12, so "exact 1.27 ratio" in the idle-voice scenario is `velocityToGain(127)/velocityToGain(100)`, which the ceiling clause allows.
2. No MidiEventTranslator and no MidiFileWriter test targets exist (they would need new Projucer-registered files). 3.2 is covered by the Processor processBlock note-on velocity test and `clampNoteOnVelocity` static_asserts; 5.2 by MidiExportTimelineTests, which write through `MidiFileWriter(1)` into a memory stream and read the bytes back (per-event velocity, clamp 0 -> 1 and 200 -> 127, note-offs stay 0).
3. Bit identity at velocity 100: the voice render fingerprint (FNV-1a over raw float bits, noteOn + noteOff release) was captured from the pre-change voice and is pinned as a test constant (0x8dd3ab4822fcb12c). Existing suites (goldens, ReproducibilityTests, export) pass unchanged.
4. `Source/ui/generated` (embedded web assets) was NOT regenerated; the C++ build does not depend on it for tests. Run the UI build step before a manual check.
5. `git add -N` was run once by mistake while measuring the diff, and undone with `git reset` on the same paths; the index is back to untracked for the new files. Nothing is staged.

## Files

C++ production: core/Step.h, playback/{StepEvent.h,SequencePlayer.cpp}, midi/{MidiMessageBytes.h,MidiEventTranslator.h,MidiEventTranslator.cpp}, synth/{SynthPatch.h,SynthVoice.h,SynthVoice.cpp,SynthEngine.cpp}, export/{MidiExportTimeline.h,.cpp,MidiFileWriter.h,.cpp}, generation/StepEdit.{h,cpp}, plugin/BerlinAudioProcessor.{h,cpp}, bridge/UiBridge.cpp.
C++ tests: Step, StepEvent, MidiMessageBytes, SequencePlayer, SequencePlayerHandoff, SynthVoice, SynthEngine, MidiExportTimeline, MutationEngine, StepEdit, BerlinAudioProcessor, UiBridge test files.
UI: lib/dynamics.ts (+test), bridge/{protocol,limits,mock}.ts, store/{actions,store}.ts, components/StepView.tsx, styles/components.css, and the matching test updates (App, mock, protocol, StepView, store).
