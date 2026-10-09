# Proposal: Tempo Control + Tempo-Synced Delay UI

Slice 2 of 5, "Berlin School authenticity" (Slice 1 `scale-aware-generation` archived as `8e2cf6b`).
Builds on Engram `sdd/berlin-school-authenticity/explore-codebase-audit` (#283) and `.../synthesis` (#285) — findings cited, not re-derived.

## Intent

Tempo-synced delay is the single best-sourced Berlin School convention, and Berlin can express neither half of it today:

- BPM is a compile-time constant (`BerlinAudioProcessor.h:145` `kBpm = 120.0`); `Transport` has no mutator at all.
- `SynthPatch` delay/reverb fields exist but have **zero UI** — reachable only by hand-editing a preset file (`BerlinAudioProcessor.cpp:102-105`).

Users cannot set a tempo, cannot hear delay, and cannot relate one to the other. This slice closes both gaps and produces the sync math Slice 3 reuses.

## Scope

### In Scope

- User-adjustable BPM (range 40–240, default 120) with real-time-safe propagation to `Transport`.
- First-ever delay/reverb UI: delay time, feedback, mix; reverb room/damping/wet/dry.
- Selectable sync divisions (1/4, 1/8, 1/8T, dotted-1/8, 1/16, 1/2 at minimum) via `seconds = (60/BPM) x factor`; plus a Sync/Free toggle so delay time can still be set manually.
- Delay time clamped to `SynthEffects::kMaxDelaySeconds` (currently 2.0s) — reachable at low BPM with long divisions.
- Exported MIDI tempo meta event follows live BPM (`BerlinAudioProcessor.cpp:259` currently passes `kBpm`).
- Persist BPM + effects fields through the existing versioned preset `ValueTree`, defaulting when absent (same precedent as Slice 1's scale fields).

### Out of Scope

- Delay-recommendation panel (Slice 3), MIDI device/channel config (Slice 4), synth-optional UI redesign (Slice 5).
- Host-tempo sync in the VST3/AU build (BPM stays user-owned).
- In-piece tempo automation/accelerando (unverified as genre-authentic per #285).
- New reverb sync concepts; reverb gets plain controls only.

## Capabilities

### New Capabilities

- `tempo-control`: user-facing BPM value, valid range, real-time-safe propagation, and note-division → delay-time conversion.

### Modified Capabilities

- `playback-transport`: replaces "Fixed BPM / No runtime BPM mutation API" with a real-time-safe runtime tempo mutation requirement that preserves drift-free boundary math.
- `internal-synth-output`: delay/reverb become live user-adjustable (currently pinned to `kDefaultPatch`), with delay time optionally tempo-derived.
- `realtime-audio-wiring`: tempo/effect parameter changes must reach the audio thread with no allocation, lock, or logging.
- `midi-file-output`: tempo meta event reflects current BPM, not a constant.
- `preset-persistence`: preset scope grows beyond "11 live parameters plus seed" to include BPM, sync division, and effects fields. (`plugin-state-recall` inherits this via `PresetManager`; no separate delta expected.)

## Approach

1. **Transport**: add `setBpm(double)` writing a `std::atomic<double>`. The audio thread recomputes `samplesPerStep` from the cached sample rate only when the atomic changed, and **rebases the boundary origin** (`position`/`nextStepCounter`) at that point so already-emitted boundaries are not retimed and no step is skipped or doubled. Cheap arithmetic only — no lock, no allocation (per `juce-app-dev` hard rules).
2. **Sync math**: a small JUCE-free unit mapping `(bpm, division) → seconds`, unit-tested independently; reused verbatim by Slice 3.
3. **SynthEffects**: message-thread setters publish to atomics; the audio thread applies `setDelay`/reverb params at block start, smoothing delay time to avoid clicks. Clamp to `kMaxDelaySeconds`; design may raise that constant (allocated in `prepare`, memory-only cost).
4. **UI**: add a TEMPO + DELAY/REVERB section to `BerlinAudioProcessorEditor`, following the existing flat-section layout — no conditional-visibility framework (that's Slice 5).

## Affected Areas

| Area | Impact | Description |
|---|---|---|
| `Source/playback/Transport.h/.cpp` | Modified | Atomic BPM + origin-rebasing recompute |
| `Source/synth/SynthEffects.h/.cpp` | Modified | Live setters, atomic publish, clamp/smoothing |
| `Source/synth/SynthPatch.h` | Modified | Effects bounds constants; sync division field |
| `Source/plugin/BerlinAudioProcessor.h/.cpp` | Modified | Drop `kBpm` as the only source; pass live BPM to export |
| `Source/plugin/BerlinAudioProcessorEditor.h/.cpp` | Modified | New tempo + delay/reverb widgets |
| `Source/preset/PresetManager.cpp` | Modified | Schema version bump, defaulting |
| Tempo-sync math unit | New | `(bpm, division) → seconds` |
| `Tests/Source/*` | New/Modified | TDD-first coverage |

## Risks

| Risk | Likelihood | Mitigation |
|---|---|---|
| Tempo change corrupts sample-accurate step timing (`round(k * samplesPerStep)` retimes history) | High | Origin rebase on change; drift + no-skip/no-double tests before implementation |
| Real-time safety regression on the audio path | Medium | Atomics only; no lock/alloc; audit against `juce-app-dev` output contract |
| Delay time exceeds 2.0s buffer at low BPM + long division | Medium | Clamp; optionally raise `kMaxDelaySeconds` in `prepare` |
| Audible click/zipper when delay time changes | Medium | Smooth delay time; test tail continuity |
| Preset schema change breaks older files (both directions) | Medium | Version bump + default-on-missing, matching Slice 1 precedent |
| Slice exceeds the 800-line review budget | Medium | If forecast is high, `sdd-tasks` splits: (a) Transport tempo core, (b) effects UI + sync |

## Rollback Plan

Single revert of the slice commit(s). All production changes are additive (new setters, new widgets, new section); no existing call site is removed. Preset files written by the new schema must be verified to still load on the reverted build (ignore-unknown-fields behavior) — a named verification item, not an assumption.

## Dependencies

- None external. Slice 1 (`scale-aware-generation`) is already landed; its preset-versioning precedent is reused.

## Success Criteria

- [ ] Changing BPM while playing retimes future steps only — no skipped, doubled, or retimed past boundary (test).
- [ ] `samplesPerStep` matches `sampleRate * 60 / (bpm * stepsPerBeat)` after every change (test).
- [ ] Delay time equals `(60/BPM) x factor` for every exposed division, clamped at the buffer bound (test).
- [ ] Delay/reverb are audibly adjustable from the UI with no preset hand-editing.
- [ ] Exported `.mid` carries the live BPM in its tempo meta event (test).
- [ ] Preset save/load round-trips BPM + division + effects; older presets load with defaults (test).
- [ ] No allocation, lock, or logging introduced on the audio-thread path (review gate).
- [ ] `BerlinTests.exe --category=Berlin` green.

## Proposal question round

Execution mode is automatic, so these were resolved by assumption. Flag any you want changed before `sdd-spec`/`sdd-design`:

1. **BPM range** — assumed 40–240. Narrower (e.g. 60–180) would sidestep the delay-buffer overflow case entirely.
2. **Preset scope** — assumed BPM + effects become persisted. The alternative (leave presets untouched) is smaller, but a loaded preset would then silently disagree with live tempo/effects state.
3. **Free vs. Sync delay** — assumed both, with the manual slider disabled while synced. Sync-only is smaller.
4. **Clamp behavior** — assumed silent clamp at 2.0s. Alternative: grey out divisions unreachable at the current BPM (clearer, more UI work).
5. **Reverb controls** — assumed included, since they share the same "no UI at all" gap. Deferring them shrinks the slice.
