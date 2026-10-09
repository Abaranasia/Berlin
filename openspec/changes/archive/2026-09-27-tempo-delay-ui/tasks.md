# Tasks: Tempo Control + Tempo-Synced Delay UI

## Review Workload Forecast

| Field | Value |
|---|---|
| Estimated changed lines | ~950-1100 (~350-450 test lines) |
| Project review budget | 800 changed lines |
| 400-line budget risk | High |
| 800-line budget risk | High — forecast exceeds it |
| Chained PRs recommended | Yes |
| Suggested split | PR A ~450 lines / PR B ~550 lines |
| Delivery strategy | single-pr-default |
| Chain strategy | pending — orchestrator/user must pick `feature-branch-chain` (PR A/B split below) or `size:exception` |

Decision needed before apply: Yes
Chained PRs recommended: Yes
Chain strategy: pending
400-line budget risk: High

### Suggested Work Units

| Unit | Goal | Likely PR | Focused test command | Runtime harness | Rollback boundary |
|---|---|---|---|---|---|
| A | TempoSync + Transport origin/setBpm + SequencePlayer plumbing + processor BPM + BPM UI + MIDI-export tempo fix | PR A | `BerlinTests.exe --category=Berlin --test=TempoSync,Transport,SequencePlayer` | Manual: drag BPM slider, confirm no click/skip, export MIDI, check tempo meta | Revert Phase 0-6 files; BPM UI/state removed, transport reverts to fixed-120 behavior, no persistence touched |
| B | SynthPatch bounds + SynthEffects setters/smoothing + SynthEngine forwarders + delay/reverb UI + sync wiring + preset schema v3 | PR B | `BerlinTests.exe --category=Berlin --test=SynthEffects,PresetManager` | Manual: adjust delay/reverb live, confirm audible no-glitch; save/load v3 preset, load old v1/v2 preset | Revert Phase 7-12 files; schema stays v2, effects fields stay pinned to kDefaultPatch (today's behavior) |

Note: schema v3 belongs entirely to PR B; PR A ships live-but-unpersisted BPM (resets to 120 on preset load — same as today's fixed behavior, an acceptable intermediate).

## Phase 0: Build Registration (blocking, do first)

- [x] 0.1 Register `Source/core/TempoSync.h/.cpp` in `Berlin.jucer`.
- [x] 0.2 Register `TempoSync.h/.cpp` + `Tests/Source/TempoSyncTests.cpp`, `TransportTempoChangeTests.cpp`, `SynthEffectsTests.cpp` in `BerlinTests.jucer`.
- [x] 0.3 Resave/regenerate both projects; confirm they build with stub-only TempoSync (empty header) before any RED test lands.

## Phase 1: TempoSync Unit — PR A (tempo-control spec)

- [x] 1.1 RED — `TempoSyncTests.cpp`: `delaySecondsFor` for 6 divisions at 40/120/240 BPM (120+quarter=0.5s; 90+dottedEighth=0.5s; eighthTriplet = exactly 1/3 of quarter-note seconds at any BPM); `bpm<=0` → 0.
- [x] 1.2 GREEN — `Source/core/TempoSync.h/.cpp`: JUCE-free `enum class SyncDivision`, `kNumSyncDivisions=6`, `factorFor`, `delaySecondsFor`.
- [x] 1.3 REFACTOR — doc comment: declaration order is persisted, never reorder without a schema bump.

## Phase 2: Transport Origin-Rebase + setBpm — PR A, CRITICAL (playback-transport spec)

- [x] 2.1 RED — `TransportTempoChangeTests.cpp`: mid-run `setBpm` → next boundary `> position`, `stepCounter==nextStepCounter` (no skip/double/negative offset); `samplesPerStep==sampleRate*60/(bpm*stepsPerBeat)`; post-change boundaries within ±1 sample of ideal grid; 500 successive `setBpm` calls accumulate no drift; `setBpm` safe before `prepare`/`bpm<=0`/while stopped/at `nextStepCounter==0`.
- [x] 2.2 GREEN — `Transport.h/.cpp`: add `originSample`/`originStep`/`sampleRate`; `boundarySampleFor(k)` chokepoint; route `countBoundaries`/`getBoundary`/`advance` through it; implement phase-preserving `setBpm`; `reset()`/`prepare()` zero origin.
- [x] 2.3 Regression gate — confirm existing `driveAndCheckBoundaries` drift suites pass unmodified.
- [x] 2.4 REFACTOR — header contract comment: `setBpm` legal only at block start, before queries.

## Phase 3: SequencePlayer Atomic Plumbing — PR A (playback-transport spec)

- [x] 3.1 RED — SequencePlayer test: mid-loop BPM change → monotonic `sampleOffset`s, preserved playhead/loop-count semantics, no stale origin after adopt.
- [x] 3.2 GREEN — `SequencePlayer.h/.cpp`: `std::atomic<double> pendingBpm`; `setBpm(double)` (message thread); one `transport.setBpm(pendingBpm.load(relaxed))` call after adopt, before `countBoundaries`.
- [x] 3.3 REFACTOR — confirm no allocation/lock introduced in `process()` diff (juce-app-dev gate).

## Phase 4: Processor BPM State + MIDI Export Fix — PR A (spec: realtime-audio-wiring, midi-file-output)

- [x] 4.1 RED — `BerlinAudioProcessorTests`: `setBpm(v)` clamps to [40,240], reaches player; `getBpm()` reflects `currentBpm`.
- [x] 4.2 RED — `MidiExportTimelineTests`: `exportMidiTo` tempo meta event matches live/changed BPM, not hardcoded 120. (Implemented in `BerlinAudioProcessorTests.cpp` instead — this is the integration layer per design.md's own Testing Strategy table; `MidiExportTimeline` itself carries no BPM/tempo concept, only tick math. Noted as a deviation from the task's literal file name.)
- [x] 4.3 GREEN — `BerlinAudioProcessor.h/.cpp`: `kBpm`→`kDefaultBpm`; add `currentBpm`; `setBpm`/`getBpm`; `.cpp:259` passes `currentBpm` instead of `kBpm`.
- [x] 4.4 REFACTOR — doc comment: `currentBpm` is single source of truth, editor is a view.

## Phase 5: BPM UI Widget — PR A (tempo-control spec)

- [x] 5.1 GREEN — `BerlinAudioProcessorEditor.h/.cpp`: TEMPO row via existing `configureSlider`/`placeLabelled`; `pushTempoFromWidgets()`; extend `refreshFromProcessor`; grow `setSize`.
- [x] 5.2 Manual verification — slider drag updates BPM live, no click/glitch (no automated GUI test per design's testing strategy — manual review gate only). VERIFIED 2026-09-27 by the user running the built app: passed, no glitches.

## Phase 6: PR A Integration Gate

- [x] 6.1 Run `BerlinTests.exe --category=Berlin` full suite green.
- [x] 6.2 Confirm PR A diff ≈450 lines before opening PR. ACTUAL ≈790 authored lines (excluding generated .vcxproj/.vcxproj.filters) — exceeds the ~450 estimate, driven mainly by Phase 2's 6 mandatory RED scenarios for the critical-path origin-rebase mechanism (`TransportTempoChangeTests.cpp` alone is 278 lines) plus thorough BerlinAudioProcessor/SequencePlayer integration coverage. Flagged as a risk; the user's `size:exception` single-PR decision already covers the overall change, so this does not block continuing.

## Phase 7: SynthPatch Bounds + Fields — PR B (internal-synth-output spec)

- [x] 7.1 RED — extend `TempoSyncTests.cpp`: `max(delaySecondsFor(kMinBpm,d))` over all divisions `== kMaxDelaySeconds` exactly at BPM=40 half-note `(60/40)*2.0=3.0`; a hypothetical longer division would fail this bound loudly.
- [x] 7.2 GREEN — `SynthPatch.h`: move `kMaxDelaySeconds` out of `SynthEffects.cpp`'s anon namespace, raise 2.0f→3.0f; add `kMinBpm/kMaxBpm/kDefaultBpm`, feedback/mix/reverb bounds; add `delaySynced`+`delayDivision` fields. Also dedupes Phase 4's local `BerlinAudioProcessor::kMinBpm/kMaxBpm/kDefaultBpm` against this canonical home; also added `kMinOutputLevel/kMaxOutputLevel` (needed by Phase 11's preset clamp).
- [x] 7.3 REFACTOR — `SynthEffects.cpp` includes `SynthPatch.h` for the constant; remove old anon-namespace definition.

## Phase 8: SynthEffects Atomic Setters — PR B, first-ever coverage (internal-synth-output spec)

- [x] 8.1 RED — new `SynthEffectsTests.cpp`: 7 setters clamp incl. NaN via `clampParameter`; delay time reaches target over N blocks; tail continuity (no discontinuity above threshold on delay-time jump).
- [x] 8.2 RED — silent-input test: near-noise-floor output (delay-line internal-state hard rule).
- [x] 8.3 RED — feedback-stability test: sustained input for hundreds of blocks, bounded peak amplitude at high feedback.
- [x] 8.4 GREEN — `SynthEffects.h/.cpp`: atomic `target` struct; 7 setters (delay time/feedback/mix, reverb room/damping/wet/dry); `applyParameters()` before channel loops; one-pole delay glide (~100ms) + `delayLine.setDelay()`; per-sample linear ramp for feedback/mix; reverb change-gated `setParameters()`.
- [x] 8.5 REFACTOR — confirmed by inspection: `ScopedNoDenormals` already covers this path at the single top-level audio callback (`BerlinAudioProcessor::processBlock`), matching the precedent that `SynthVoice::render`/`SynthEngine::render` don't each redeclare it; no allocation/lock in the `process()` diff.

## Phase 9: SynthEngine Forwarders — PR B

- [x] 9.1 RED — extended `SynthEngineTests.cpp` (not `SynthEffectsTests.cpp` — needs both `SynthEngine` and `SynthEffects` in scope, matching the file's existing `compareForwarder` precedent): 7 new `compareFxForwarder` cases prove each forwarder reaches `SynthEffects` identically to calling the setter directly.
- [x] 9.2 GREEN — `SynthEngine.h/.cpp`: 7 one-line forwarders mirroring lines 129-139.

## Phase 10: Delay/Reverb UI + Sync Wiring — PR B (tempo-control, internal-synth-output specs)

- [x] 10.1 GREEN — `BerlinAudioProcessorEditor.h/.cpp`: DELAY/REVERB sections (2 rows each) via a local `configureSliderEarly` lambda (the ctor's existing `configureSlider` lambda is declared later in the file, out of scope this early) + inline `placeLabelled`-style rows; Sync toggle + division combo box.
- [x] 10.2 GREEN — `recomputeSyncedDelayTime()`: Sync disables manual entry and tracks division-derived seconds (from live BPM); Free re-enables manual entry, retains last manual value across Sync→Free→Sync via `lastManualDelayTimeSeconds`.
- [x] 10.3 GREEN — D8: `fxToggle.onClick` calls `updateDelayReverbEnablement()`, which sets every `delayReverbWidgets` entry's enabled state (plus `delayTimeSlider`'s own extra Sync-mode gate).
- [x] 10.4 GREEN — extended `currentPatchFromWidgets`/`applyPatchToWidgets`/`refreshFromProcessor`; grew `setSize` by 4 rows. DEVIATION (flagged per skill rule): also extended `BerlinAudioProcessor::pushPatchToSynth` with the 7 effect pushes here rather than under 11.3 as tasks.md lists it — necessary now because Phase 10's sliders write into `currentPatch` via `setPatch()`, and without the forwarding those writes would never reach the running `SynthEngine` (RED-proved by `BerlinAudioProcessorTests.cpp`'s "setPatch() with delay/reverb fields reaches the synth engine").
- [x] 10.5 Manual verification — Sync/Free toggle scenario per spec; FX section greys out when off (no automated GUI test per design's testing strategy). VERIFIED 2026-09-27 by the user running the built app: passed, no glitches.

## Phase 11: Preset Schema v3 — PR B (preset-persistence, plugin-state-recall specs)

- [x] 11.1 RED — extended `PresetSerializationTests.cpp` (this is already the pure-core round-trip suite the v1→v2 precedent lives in, not a separate `PresetManagerTests.cpp`): v3 round-trips BPM/division/sync/8 effects fields; pre-v3 files default missing bpm→120, sync→Free, division→quarter, effects→kDefaultPatch per field; out-of-range BPM/effects values clamped; unknown division name→`parseFailed`; unrecognized `delaySynced` text→`parseFailed`; missing `<Transport>`/`bpm`→`parseFailed`.
- [x] 11.2 GREEN — `PresetManager.h/.cpp`: `kSchemaVersion`2→3; `divisionNames()` table (mirrors `scaleNames()`); `<Transport bpm="..."/>` node; 8 effects fields+`delaySynced`+`delayDivision` in `<Synth>`; `clampParameter`/hand-rolled NaN-safe double clamp on every new numeric field; `Preset.h` gains `double bpm`.
- [x] 11.3 GREEN — `BerlinAudioProcessor` `setStateInformation`/`loadPreset`/`save`/`getStateInformation` restore/persist BPM. (`pushPatchToSynth`'s 7 effect pushes landed under 10.4 instead — see that task's deviation note.)
- [x] 11.4 RED+GREEN — `BerlinAudioProcessorTests`: `setStateInformation` round-trips BPM + a preset save/load round-trip preserves BPM (plugin-state-recall inherits via existing `toValueTree`/`fromValueTree` reuse — confirmed, no separate delta needed).
- [x] 11.5 REFACTOR — confirmed migration mirrors the v2 pattern exactly (`version>=3` requires every new attribute else `parseFailed`; `version<3` defaults from `kDefaultBpm`/Free/quarter/`kDefaultPatch`'s own effects fields).

## Phase 12: PR B Integration Gate

- [x] 12.1 Run `BerlinTests.exe --category=Berlin` full suite green — 309 tests, all passed, exit code 0.
- [x] 12.2 Manual review gate: audible delay/reverb, no click on tempo/delay-time change, audio-path diff has no allocation/lock/logging. Allocation/lock absence confirmed by static inspection; audible/no-click claim VERIFIED 2026-09-27 by the user running the built app: passed, no glitches.
- [x] 12.3 Verify rollback claim: a v3 preset is rejected (not corrupting) by a reverted v2 build and silently dropped from `listPresetNames()`. VERIFIED 2026-09-27 by an actual build: checked out parent commit `8e2cf6b` (kSchemaVersion=2) into a worktree, built it, fed it a genuine v3 preset generated from the current build — `load()` returned `unsupportedVersion`, the output `Preset` was left untouched (no corruption, no crash), and `listPresetNames()` on a directory containing only the v3 file returned 0 names.
- [x] 12.4 Confirm PR B diff size before opening PR — see this batch's final report for the measured `git diff --stat` total; the user's `size:exception` single-PR decision (Engram obs #302) already covers the whole change regardless of this number.

## Phase 13: Archive-Prep Notes (non-code, flag for sdd-archive)

- [x] 13.1 Flag: `playback-transport` spec's old Purpose sentence ("No runtime BPM control surface exists") needs hand-update at archive time. (Documented as a "Non-canonical archive note" inside the reconciled spec artifact, obs #300.)
- [x] 13.2 Flag: `preset-persistence` spec's old Purpose sentence (8 effects fields "out of scope, pinned to kDefaultPatch") needs hand-update at archive time. (Same obs #300 artifact — now doubly true since Phase 11 actually persists all 8 fields.)
- [x] 13.3 Flag: `playback-transport` requirement-title rename vs `preset-persistence`'s keep-identical-title precedent — confirm archive merge doesn't orphan the old heading. (Documented in obs #300's "Risks / open items" list, item 1 — carried forward for `sdd-archive` to action, not resolved here.)

Note: Threat matrix is N/A for this change (no routing/shell/subprocess/VCS/process-integration boundary; the only untrusted-input surface, preset XML, is pre-existing and reuses its established clamp-on-parse rule) — no RED tests owed beyond Phase 11's XML-clamp cases already listed.
