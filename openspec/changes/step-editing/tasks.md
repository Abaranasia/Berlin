# Tasks: Step Editing (mute and note per step)

Strict TDD is active: every behavior task is RED (write the failing test, run it, confirm it fails for the right reason), then GREEN (minimal code), then run the suite. Package manager is pnpm only (`pnpm --dir ui ...`); npm is broken, never use it. MSBuild: `C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe`. Projucer: `H:\Proyectos\Juce\Projucer\Projucer.exe --resave <jucer>`. C++ tests: `Tests/Builds/VisualStudio2026/x64/Debug/ConsoleApp/BerlinTests.exe --category=Berlin`. Skills to load when applying: `juce-app-dev` (audio thread no alloc/locks/logging, one-slot handoff ordering), `juce-app-testing`, `work-unit-commits` (paths in `.atl/skill-registry.md`).

Authoritative inputs: `proposal.md`, `design.md` (D1-D8), `specs/*/spec.md` (spec and design are reconciled; D4 order is index, then note, then busy).

Verification legend (from the specs): [V] Vitest/jsdom, [T] BerlinTests, [M] manual, [B] build.

## Review Workload Forecast

| Field | Value |
|---|---|
| Estimated authored lines | ~835 (code + tests; excludes delta specs, lockfile, generated `dist`/embedded assets) |
| Review budget | 800 authored lines per PR |
| Delivery strategy | single-pr-default |
| Chain strategy | not needed unless apply exceeds 800 |
| 400-line budget risk | High as a single PR (~835) |
| 800-line budget risk | Medium (about 4% over; the proposal's 470-600 left out most test lines) |
| Chained PRs recommended | No by default; split only if apply goes past 800 |
| Decision needed before apply | No (single PR; keep fixtures tight; split at the point below if the running total passes 800) |

Per-commit estimates:

| # | Commit | Est. lines |
|---|---|---|
| 1 | `PublishMode` + keepPosition adopt + handoff tests | ~95 |
| 2 | `StepEdit` pure unit + tests + jucer | ~120 |
| 3 | `editStep`, `edited`, `setStep`, snapshot + processor/bridge tests | ~175 |
| 4 | Protocol, mock, store busy retry, `sendStep` + tests | ~135 |
| 5 | `lib/stepEdit`, interactive StepView, CSS + tests | ~310 |
| | Total | ~835 |

Natural split point (use only if apply passes 800):

| PR | Commits | Content | Est. lines | Rollback |
|---|---|---|---|---|
| PR A | 1-3 | Engine only: keep-position handoff, `StepEdit`, `editStep`/`edited`, `setStep` bridge command, snapshot fields | ~390 | Revert A: no edit path exists; `PublishMode` default keeps all old callers unchanged |
| ===== SPLIT BOUNDARY ===== | | PR B targets PR A's branch if chained | | |
| PR B | 4-5 | Protocol/mock/store retry, `lib/stepEdit`, interactive StepView, CSS | ~445 | Revert B: UI returns to the read-only step view; engine command stays registered but unused |

Note: PR A is autonomous. The `setStep` command and snapshot fields exist but nothing in the UI calls them yet (the UI ignores unknown snapshot fields). No intermediate state is broken.

## Phase 0: Baseline

- [x] 0.1 `git status` and `git branch --show-current`: confirm branch `feat/step-editing`; verify git state yourself (`?? openspec/changes/step-editing/` is expected).
- [x] 0.2 `pnpm --dir ui test`; record the Vitest count (baseline: 490 expected, confirm).
- [x] 0.3 Build `Builds/VisualStudio2026/Berlin.sln` and `Tests/Builds/VisualStudio2026/BerlinTests.sln` (Debug x64); run `BerlinTests.exe --category=Berlin`; capture the exit code directly (`$LASTEXITCODE`) and record the pass count (baseline: 411 expected, confirm).

## Phase 1: PublishMode and keep-position adopt (commit 1, ~95 lines, strict TDD)

Files: `Source/playback/SequencePlayer.{h,cpp}`, `Tests/Source/SequencePlayerHandoffTests.cpp`. Specs: generation-live-control "Audio-Thread-Safe Regeneration Handoff" (keep-position variant); step-editing "Keep-Position Edit Playback"; sequence-mutation "Publish Always Resets Playback Phase". Design D1, D2.

- [x] 1.1 RED handoff tests (extend `SequencePlayerHandoffTests.cpp`), all failing for missing `PublishMode`: keepPosition adopt keeps playhead and step counter (next boundary is step k+1, not 0) and leaves `loopCount` unchanged; restart mode still resets to step 0 (regression); sounding step whose note changed gets a note-off at offset 0 emitted first; sounding step whose gate went off gets a note-off; same content gives no event; another step edited gives no event; nothing sounding gives no event; sequence-size mismatch (`pendingStep >= size`) is safe and emits the note-off; `static_assert (noexcept (...))` on `publishSequence`. Satisfies: Keep-position adoption does not reset [T]; Whole-sequence publish still resets [T]; Playhead continues after edit [T]; Editing the sounding step sends note-off [T]; Editing another step sends no note-off [T]; Edit does not restart the loop [T].
- [x] 1.2 GREEN `enum class PublishMode { restart, keepPosition }`; `publishSequence (Sequence&, PublishMode = PublishMode::restart) noexcept`; plain `PublishMode pendingMode` written before the `sequencePending` release store and read after the acquire load.
- [x] 1.3 GREEN `process()` adopt: for keepPosition, `sequence.swap (pendingSequence)`, no `transport.reset()`, no playhead store; apply the D2 note-off rule (push `{0, pendingStep, pendingNote, false}` first and set `pendingNote = -1`); restart path byte-identical. Confirm by review: no allocation, no lock, no logging on the audio thread.
- [x] 1.4 Confirm the 4 existing callers (regenerate, mutate, loadPreset via regenerate, evolve via mutate) compile unchanged (default argument). Build standalone and tests; `BerlinTests.exe --category=Berlin` green (411 + new). Commit 1: `feat: add keep-position sequence publish mode`.

## Phase 2: StepEdit pure unit (commit 2, ~120 lines, strict TDD)

Files: `Source/generation/StepEdit.{h,cpp}`, `Tests/Source/StepEditTests.cpp`, `Berlin.jucer`, `Tests/BerlinTests.jucer`. Specs: step-editing "Step Index Limit", "Absolute Note and Gate Assignment", "Scale-Aware Stepping Notes" (`scaleNotes` list). Design D3.

- [x] 2.1 Add `StepEdit.{h,cpp}` (declarations and empty stubs only) and `StepEditTests.cpp` to both `.jucer` files following the existing group conventions; run Projucer `--resave` for `Berlin.jucer` and `Tests/BerlinTests.jucer`; confirm both solutions compile.
- [x] 2.2 RED `scaleNotesFor`: C minor 36..72 exact list; root change; scale change; inverted range and narrow range normalized (same `normalizePitchRange` as generation); list ascending and never empty. Satisfies: scaleNotes follows scale, root and range [T]; scaleNotes tracks generation params [T] (major, root 0, 48..60 gives `[48,50,52,53,55,57,59,60]`). Confirm fails.
- [x] 2.3 GREEN `scaleNotesFor` and `kEditableSteps = 16`.
- [x] 2.4 RED `validateStepEdit`: index -1, 16 on a 32-step sequence, 8 on an 8-step sequence give `stepOutOfRange`; index 15 on 16-step is `none`; note out of scale and out of range give `noteOutOfScale`; check order index before note; muted-step note stays valid; `static_assert noexcept`. Satisfies: Valid index edits the step [T]; Index beyond 16 or size is rejected [T]; Negative index is rejected [T]; Out-of-scale note is rejected [T]; Mute keeps the note [T].
- [x] 2.5 GREEN `StepEditError`, `validateStepEdit`.
- [x] 2.6 Build both solutions and tests; `BerlinTests.exe --category=Berlin` green. Commit 2: `feat: add pure step edit validation and scale note list`.

## Phase 3: Processor editStep, edited flag, setStep, snapshot (commit 3, ~175 lines, strict TDD)

Files: `Source/plugin/BerlinAudioProcessor.{h,cpp}`, `Source/bridge/UiBridge.cpp`, `Tests/Source/BerlinAudioProcessorTests.cpp`, `Tests/Source/UiBridgeTests.cpp`. Specs: step-editing "Edited Flag Lifecycle", "Edits Are Session State", "Keep-Position Edit Playback" (busy); sequence-mutation "Mutation builds on edits"; auto-evolution "Evolve Runs On Edited Patterns"; ui-bridge "setStep Command", "Fixed Error Tokens", "Snapshot Contents", "Message-Thread-Only Dispatch". Design D4, D5.

- [x] 3.1 RED processor tests: `editStep` ok updates `currentSequence` and sets `edited`; rejected edit (range, scale) leaves sequence and `edited` unchanged; busy (publish pending) fails with sequence and `edited` unchanged; validation precedes busy (out-of-range index or out-of-scale note while pending returns that error, not `busy`, checked in that order); idempotent re-send gives identical sequence and still publishes; regenerate, randomize and loadPreset clear `edited`; `setStateInformation` path clears it; mutate and auto-evolve keep it; mutate after an edit uses the edited base (transpose keeps a muted step muted); evolve mutates the edited sequence; a pending edit publish at a due tick makes auto-evolve skip `mutate()` with no status and retry later; export of an edited sequence omits the muted step; regenerate with the same seed equals the seeded original. Satisfies: Edit sets, regenerate clears [T]; Randomize and loadPreset clear it [T]; Mutate keeps it [T]; Rejected edit does not set it [T]; Idempotent re-send [T]; Busy rejects the edit [T]; Validation precedes busy [T]; Export reflects edits [T]; Regenerate discards edits [T]; Mutation builds on edits [T]; Evolve mutates the edited sequence [T]; Edit publish pending at a due tick [T]. Confirm fails.
- [x] 3.2 GREEN `StepEditResult { ok, busy, stepOutOfRange, noteOutOfScale }`; `BerlinAudioProcessor::editStep (int index, int note, bool active)` per D4 (validate index then note, then `isPublishPending()` busy, then copy, assign, `publishSequence (copy, PublishMode::keepPosition)`, commit `currentSequence`, set `edited`); `edited` member and getter; `regenerate()` success clears `edited`; `mutate()` untouched.
- [x] 3.3 RED bridge tests: dispatch command table has exactly 16 commands; `setStep {index:2, note:50, active:true}` returns `ok`, `snapshot.steps[2] == {50, true}`, `snapshot.edited == true`; `step-out-of-range` (index 16); `note-out-of-scale`; `busy` has no `snapshot` key and state unchanged; `{index:1, note:50}` gives `missing arg: active`; `{index:"1", ...}` and `index: 2.5` give `invalid type: index`; non-finite gives `non-finite: <field>`; out-of-int-range values fail as range errors, not cast garbage; snapshot contains `edited` and `scaleNotes` and `scaleNotes` tracks `setGenerationParams`; settings functions are not dispatch commands. Satisfies: Valid edit succeeds [T]; Out-of-range index [T]; Out-of-scale note [T]; Busy [T]; Malformed args [T]; scaleNotes tracks generation params [T]; Snapshot includes every documented field; Dispatch is a message-thread operation (review). Confirm fails.
- [x] 3.4 GREEN `UiBridge.cpp`: `readRequiredInt` (number, finite, integral; sentinel clamp for out-of-int-range) and `readRequiredBool ("active")`; `handleSetStep`; tokens `busy`, `step-out-of-range`, `note-out-of-scale`; `kCommands` becomes 16; snapshot adds `edited` and `scaleNotes` (from `scaleNotesFor (getGenerationParams())`).
- [x] 3.5 Build standalone and plugin (`Plugin/Builds/VisualStudio2026/Berlin.sln`), Debug x64; `BerlinTests.exe --category=Berlin` green (exit code captured). Commit 3: `feat: add setStep command with edited flag and scale notes snapshot`.

SPLIT BOUNDARY (only if the running total exceeds 800): PR A = commits 1-3 (Phases 1-3). PR B = commits 4-5 (Phases 4-5).

## Phase 4: Protocol, mock, store retry, sendStep (commit 4, ~135 lines, strict TDD)

Files: `ui/src/bridge/{protocol,mock}.ts`, `ui/src/store/{store,actions,status}.ts`, `ui/src/store/store.test.ts`, `ui/src/bridge/{protocol,mock}.test.ts`. Specs: web-ui-controls "Per-Step Coalescing With Busy Retry", "Typed Protocol"; ui-bridge "Dispatch surface is 16 commands". Design D6.

- [x] 4.1 RED protocol tests: command list has 16 names including `setStep`; `setStep` args are typed `{index, note, active}` (a type-level test that omitting `active` fails to compile); snapshot type has `edited: boolean` and `scaleNotes: number[]`; error tokens include `step-out-of-range` and `note-out-of-scale`. Satisfies: Dispatch surface is 16 commands [V]; setStep args are typed [V]; Unknown command is a type error.
- [x] 4.2 RED mock tests: `setStep` validates index and note like the engine (index, then note), updates `steps` and sets `edited`; mock snapshot has fixed `scaleNotes` fixture; regenerate-like commands clear `edited`.
- [x] 4.3 RED store tests (fake timers): per-step keys `step.<i>` with optimistic `{note, active}`; fast wheel (10 sends while the first is in flight) ends with exactly one further `setStep` carrying the latest absolute note; busy then ok lands the final value with no status and the display never reverts; a newer send during a retry replaces the old one; retry limit exhausted settles with "Busy, try again"; different steps do not coalesce; non-busy failure (`note-out-of-scale`) reverts and shows a mapped human message; other commands do not retry on busy (`RETRY_ON_BUSY = {'setStep'}`). Satisfies: Fast wheel ends on the last note [V]; Busy retry lands the last value [V]; Different steps do not coalesce [V]; Non-busy failure reverts [V]. Confirm fails.
- [x] 4.4 GREEN `protocol.ts` (command, snapshot fields, tokens), `mock.ts`, `store.ts` `run()` busy retry (`busyRetryDelayMs` 25, `busyRetryLimit` 40, keep overlay and `inFlight`, latest-wins `pending` first), `actions.ts` `sendStep`, `status.ts` messages for the two new tokens.
- [x] 4.5 `pnpm --dir ui test`, `typecheck`, `build` (no mock chunk; existing `assertNoMockChunk`). Commit 4: `feat: add setStep protocol, mock and store busy retry`.

## Phase 5: lib/stepEdit, interactive StepView, CSS (commit 5, ~310 lines, strict TDD)

Files: `ui/src/lib/stepEdit.ts` + `stepEdit.test.ts`, `ui/src/components/StepView.tsx` + `StepView.test.tsx`, `ui/src/styles/components.css`. Specs: step-editing "Scale-Aware Stepping Notes", "Interactive Step Grid"; web-ui-layout "Step View". Design D7, D8.

- [x] 5.1 RED `stepEdit.test.ts`: `nextNote` in-scale up and down (`[48,50,51,53]` at 50: up 51, down 50, down 48), snap from out-of-scale both ways (`[48,50,51]` at 49: up 50, down 48), ends stop (no wrap); `noteAfter`; `classifyGesture (dy, slopPx=4)`; `dragSteps (startY, y, pxPerNote=12)`; `accumulateWheel` (small deltas accumulate, remainder carried, 100 px or 3 lines per note, up = higher note); `focusTarget (index, key, count)` for Left/Right/Home/End. Satisfies: Up and down within scale [V]; Out-of-scale note snaps by direction [V]; Ends are clamped [V]. Confirm fails.
- [x] 5.2 GREEN `lib/stepEdit.ts`.
- [x] 5.3 RED `StepView.test.tsx`: click without movement dispatches `setStep` with its note and `active:false`; vertical drag past one note threshold sends the new scale note with no mute toggle; wheel notch up moves one scale note and `defaultPrevented` is true (non-passive listener on `.steps`); keyboard Space toggles, ArrowUp steps the note up, ArrowRight moves focus, End moves to the last cell, with a roving tabindex (only one cell tabbable); aria-label "Step 3, C3, off" / "..., on" and `aria-pressed`; muted note shown dimmed (`data-gate=false`); edited badge shown while `snapshot.edited` is true and removed on a snapshot with `edited:false`; chained presses read the overlay (shown value) so fast input accumulates; Slice 1-4 StepView assertions (gate LED, playhead, "C3" naming) still pass. Satisfies: Click toggles mute [V]; Drag does not toggle [V]; Wheel steps and prevents scroll [V]; Keyboard [V]; Accessibility and dimming [V]; Edited indicator [V]; Gate distinction [V]; Playhead [V]; Muted note dimmed [V]; Edited indicator follows snapshot [V]. Confirm fails.
- [x] 5.4 GREEN `StepView.tsx`: `StepCell` as `div role="button"`, roving `tabIndex`, `data-step`, pointer handlers (click vs drag), one `useEffect` non-passive `wheel` listener with `preventDefault` and accumulator reset when the cell changes, keyboard handlers, `shown (state, 'step.i', engineStep)` values, "Edited" badge with a title explaining the seed.
- [x] 5.5 CSS in `components.css` using existing tokens only (no raw hex; `styles.test.ts` still passes): dimmed muted note, accent focus ring, edited badge, 44 px minimum targets, `touch-action`/cursor for drag.
- [x] 5.6 `pnpm --dir ui test`, `typecheck`, `build` (no mock chunk). Commit 5: `feat: add interactive step grid with mute and note editing`.

## Phase 6: Build and test commands (run before each commit and again at the end)

- [x] 6.1 `pnpm --dir ui test` (all Vitest green; Phase 0.2 baseline 490 plus the new tests).
- [x] 6.2 `pnpm --dir ui typecheck` (tsc, no errors).
- [x] 6.3 `pnpm --dir ui build` with no mock chunk (existing `assertNoMockChunk`).
- [x] 6.4 Standalone Debug x64 build (`Builds/VisualStudio2026/Berlin.sln`) and plugin Debug x64 build (`Plugin/Builds/VisualStudio2026/Berlin.sln`), `/t:Rebuild`; zero errors.
- [x] 6.5 `BerlinTests.exe --category=Berlin` (baseline 411 plus the new tests); capture the exit code directly (`$LASTEXITCODE`) and record it.

## Phase 7: Open-question handling and archive note

- [x] 7.1 Open question: D4 order (validation before busy) matches the spec scenarios (the "Validation precedes busy" scenario is in `step-editing`). Resolved; record in apply-progress.
- [x] 7.2 Open question: the mock keeps a fixed `scaleNotes` fixture and does not recompute on `setGenerationParams`. Accepted for tests and dev only; record in apply-progress.
- [ ] 7.3 Archive note for `sdd-archive`: the ui-bridge main spec's Purpose line ("Exactly 15 commands") must become 16 when the delta is merged (the delta only modifies requirement bodies, not Purpose). Diff the merged result and verify the archive output (folder duplication, dropped files).

## Phase 8: Manual verification [M] (record result and date per line)

Run in the flag-ON standalone app and in the VST3 inside Cakewalk Sonar (WebView2) with auto-evolve on where noted.

- [ ] 8.1 The loop keeps its position while editing during playback (standalone and VST3). Satisfies: Playhead continues after edit; WebView2 input [M].
- [ ] 8.2 No hung notes when muting or retuning the sounding step (internal synth and external MIDI).
- [ ] 8.3 The mouse wheel over the steps does not scroll the page. Satisfies: Wheel steps and prevents scroll; WebView2 input [M].
- [ ] 8.4 Keyboard focus and keys work: Space toggles, Up/Down change the note, Left/Right/Home/End move focus, accent focus ring visible. Satisfies: Focus ring on cells [M].
- [ ] 8.5 Drag changes the note and click toggles mute; a fast wheel ends on the last note.
- [ ] 8.6 Muted steps show their note dimmed.
- [ ] 8.7 The "edited" indicator appears after an edit and clears on Generate, Randomize and Load preset.
- [ ] 8.8 MIDI export includes the edits (muted step produces no note, retuned step has the new note).
- [ ] 8.9 Auto-evolve keeps running on an edited pattern and the next mutation builds on the edits; the indicator stays on. Satisfies: Edit while evolve is on (manual) [M].
- [ ] 8.10 Record all results and the final test counts in apply-progress; hand off to `sdd-verify`.

## Spec Scenario Coverage Map

| Spec scenario | Task(s) |
|---|---|
| step-editing: Valid index edits the step [T] | 2.4, 2.5, 3.1 |
| step-editing: Index beyond 16 or size is rejected [T] | 2.4, 2.5 |
| step-editing: Negative index is rejected [T] | 2.4, 2.5 |
| step-editing: Mute keeps the note [T] | 2.4, 3.1 |
| step-editing: Out-of-scale note is rejected [T] | 2.4, 2.5, 3.1 |
| step-editing: Idempotent re-send [T] | 3.1, 3.2 |
| step-editing: Up and down within scale [V] | 5.1, 5.2 |
| step-editing: Out-of-scale note snaps by direction [V] | 5.1, 5.2 |
| step-editing: Ends are clamped [V] | 5.1, 5.2 |
| step-editing: scaleNotes follows scale, root and range [T] | 2.2, 2.3, 3.3 |
| step-editing: Edit sets, regenerate clears [T] | 3.1, 3.2 |
| step-editing: Randomize and loadPreset clear it [T] | 3.1, 3.2 |
| step-editing: Mutate keeps it [T] | 3.1, 3.2 |
| step-editing: Rejected edit does not set it [T] | 3.1, 3.2 |
| step-editing: Export reflects edits [T] | 3.1, 8.8 |
| step-editing: Regenerate discards edits [T] | 3.1 |
| step-editing: Playhead continues after edit [T] | 1.1-1.3, 8.1 |
| step-editing: Editing the sounding step sends note-off [T] | 1.1-1.3, 8.2 |
| step-editing: Editing another step sends no note-off [T] | 1.1-1.3 |
| step-editing: Busy rejects the edit [T] | 3.1, 3.2 |
| step-editing: Validation precedes busy [T] | 3.1, 3.2 |
| step-editing: Click toggles mute [V] | 5.3, 5.4, 8.5 |
| step-editing: Drag does not toggle [V] | 5.1, 5.3, 5.4, 8.5 |
| step-editing: Wheel steps and prevents scroll [V] | 5.1, 5.3, 5.4, 8.3 |
| step-editing: Keyboard [V] | 5.1, 5.3, 5.4, 8.4 |
| step-editing: Accessibility and dimming [V] | 5.3, 5.4, 8.6 |
| step-editing: Edited indicator [V] | 5.3, 5.4, 8.7 |
| step-editing: WebView2 input [M] | 8.1, 8.3, 8.4, 8.5 |
| ui-bridge: setStep Valid edit succeeds [T] | 3.3, 3.4 |
| ui-bridge: setStep Out-of-range index [T] | 3.3, 3.4 |
| ui-bridge: setStep Out-of-scale note [T] | 3.3, 3.4 |
| ui-bridge: setStep Busy [T] | 3.3, 3.4 |
| ui-bridge: setStep Malformed args [T] | 3.3, 3.4 |
| ui-bridge: Fixed Error Tokens (new tokens) | 3.3, 3.4, 4.1, 4.4 |
| ui-bridge: Snapshot includes every documented field | 3.3, 3.4 |
| ui-bridge: scaleNotes tracks generation params [T] | 2.2, 3.3 |
| ui-bridge: Dispatch is a message-thread operation | 3.4 (code review) |
| ui-bridge: Dispatch surface is 16 commands [V] | 3.3, 4.1, 7.3 |
| ui-bridge: Registered only with a host / Get / Apply / Callback after teardown | Unchanged by this change; existing tests stay green (6.5, 6.1) |
| web-ui-controls: Fast wheel ends on the last note [V] | 4.3, 4.4 |
| web-ui-controls: Busy retry lands the last value [V] | 4.3, 4.4 |
| web-ui-controls: Different steps do not coalesce [V] | 4.3, 4.4 |
| web-ui-controls: Non-busy failure reverts [V] | 4.3, 4.4 |
| web-ui-controls: Typed Protocol (enum lists, unknown command, setStep args) | 4.1, 4.4 |
| web-ui-layout: Gate distinction / Playhead [V] | 5.3 (regression assertions kept) |
| web-ui-layout: Muted note dimmed [V] | 5.3, 5.4, 8.6 |
| web-ui-layout: Edited indicator follows snapshot [V] | 5.3, 5.4, 8.7 |
| web-ui-layout: Focus ring on cells [M] | 5.5, 8.4 |
| generation-live-control: Keep-position adoption does not reset [T] | 1.1-1.3 |
| generation-live-control: Whole-sequence publish still resets [T] | 1.1, 1.4 |
| generation-live-control: Existing handoff scenarios (no hung note, no torn Sequence) | 1.1, 1.3, 8.2 |
| sequence-mutation: Mutate restarts the loop at step 0 | 1.1 (restart regression), 1.4 |
| sequence-mutation: Mutation builds on edits [T] | 3.1 |
| sequence-mutation: Edit does not restart the loop [T] | 1.1-1.3, 8.1 |
| auto-evolution: Evolve mutates the edited sequence [T] | 3.1 |
| auto-evolution: Edit publish pending at a due tick [T] | 3.1 |
| auto-evolution: Edit while evolve is on (manual) [M] | 8.9 |
