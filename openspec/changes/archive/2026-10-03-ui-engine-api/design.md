# Design: Engine API for the UI

Change `ui-engine-api` (UI redesign Slice 0/6). Implements `proposal.md`. Capabilities: `ui-bridge` (new), `playback-transport` and `internal-synth-output` (modified). C++ only. The old editor source is not touched, with one approved exception: `currentPatchFromWidgets` keeps `owner.getPatch().outputLevel` instead of the default (see File Changes and Open Questions).

## Technical Approach

Reuse the atomic patterns already in the code. `pendingBpm` is the model for the play request, `enabled` is the model for the master-level atomic, and the derivation of synced delay time moves from the editor into `BerlinAudioProcessor`. A new `UiBridge` (message thread only) has a fixed table of 15 commands. Each handler parses and validates all of its args into locals before it calls any mutator, so a failed call changes no state. Field-descriptor tables (pointer to member, plus bounds) drive both `setPatch`/`setGenerationParams` decoding and `snapshot()` encoding, so the two cannot drift apart.

## Architecture Decisions

| # | Decision | Rejected | Rationale |
|---|---|---|---|
| D1 | `SequencePlayer::std::atomic<bool> playRequested`, relaxed. It is adopted in `process()` after the `pendingBpm` adopt and before `countBoundaries`, **only on a transition**: stop only when `!playRequested && transport.isRunning()`, start only when `playRequested && !transport.isRunning()`. A stop is adopted as `transport.stop()`, then `{0, pendingStep, pendingNote, false}` is pushed if a note is sounding, then `pendingNote = -1` | Call `Transport::start/stop` from the message thread; `flushPendingNoteOff` from the message thread; adopt the request unconditionally every block | Message-thread calls race on non-atomic `running`, `pendingNote`, and `blockEvents`. Transition-only adoption is required because a direct `stop()` stops the transport and keeps `pendingNote` (D2); an unconditional adopt would flush that note on the next block and break the existing `SequencePlayerStopTests` contract. It also makes steady-state blocks a no-op. The sequence-adopt branch already clears `pendingNote`, so the same block can never get a double note-off. |
| D2 | `start()`/`stop()` also store `playRequested`. The ctor initialises it from `transport.isRunning()`, the same way as `pendingBpm` | A separate flag that `start()`/`stop()` do not set | Without this, the next `process()` would undo a direct `stop()`. The existing `SequencePlayerStopTests` contract still holds: a direct `stop()` keeps `pendingNote` so a later flush can still emit it. |
| D3 | `isPlaying()` returns `playRequested` (the requested state) | The adopted `transport.isRunning()` | Pinned. The snapshot is correct even when no audio callback runs. |
| D4 | Restart **resumes**. This is verified: `Transport::stop()` only clears `running` (Transport.cpp:48-51), and `advance()` does nothing while stopped, so `position`, `nextStepCounter`, and the origin are kept. If stopped during step 5, on restart playback resumes at the next step boundary (step 6) at its original grid position; the remainder of step 5 elapses silently and step 5 is not replayed. Three things reset to step 0: `prepareToPlay`, a sequence adopted while stopped (regenerate, mutate, or load), and `reset()` | Reset to step 0 on play | This is how the code already behaves. No change is needed. |
| D5 | `SynthEngine`: `std::atomic<float> masterLevel{0.8f}` and `juce::SmoothedValue<float, Linear> masterGain` (50 ms). `prepare()` calls `reset(sr, 0.05)` and `setCurrentAndTargetValue(atomic)`. `render()`, after FX and before the terminal `addFrom` (SynthEngine.cpp:106), calls `setTargetValue(atomic)` then `masterGain.applyGain(scratch, numSamples)`. The disabled early-return path snaps the smoother to the target | Manual ramp; a gain applied to `destination`; a multiplicative smoother | `applyGain(AudioBuffer&, int)` is verified `noexcept` in JUCE 9 and does not allocate. A linear ramp can reach 0. Only `scratch` is scaled, so MIDI is not affected. |
| D6 | `setMasterLevel(float)` applies `clampParameter(v, kMinOutputLevel, kMaxOutputLevel)`, writes `currentPatch.outputLevel`, then calls `synth.setMasterLevel`. `pushPatchToSynth()` forwards `outputLevel`, which makes it live for `setPatch`, `loadPreset`, `setStateInformation`, and `prepareToPlay` | A separate `masterLevel` member | One source of truth, the same as the `currentBpm` pattern. |
| D7 | Synced delay: a private `applySyncedDelayTime()`. If `delaySynced`, it sets `currentPatch.delayTimeSeconds = clampParameter((float) delaySecondsFor(currentBpm, delayDivision), …)`. It is called in `setPatch()` before `pushPatchToSynth()`, and in `setBpm()` followed by `synth.setDelayTimeSeconds` | Leave it in the editor; derive it in the bridge | Pinned. `loadPreset` and `setStateInformation` go through `setPatch` and `setBpm`, so they are covered automatically. |
| D8 | The enum name tables are shared through 8 public static forwarders on `PresetManager` (`waveformName`/`parseWaveform`, `lfoDestinationName`/`parseLfoDestination`, `scaleTypeName`/`parseScaleType`, `divisionName`/`parseDivision`). They delegate to the existing anonymous-namespace helpers | Move the tables to a new header; copy them in UiBridge | One table, a diff of about 20 lines, and no move. New identifiers avoid shadowing the anonymous `waveformToName` inside `PresetManager` members, which would otherwise recurse. The `RhythmMode` names (`random`/`euclidean`/`probability`) are not persisted, so they live only in `UiBridge.cpp`. |
| D9 | Dispatch is a `constexpr` array of `{const char* name, juce::String (*run)(BerlinAudioProcessor&, const juce::var&)}` with `static_assert(size == 15)`. A handler returns `""` on success or an error | `std::map<String, std::function>` | No allocation at static-init time, the list of 15 is checked at compile time, and the lookup is linear over 15 entries. |
| D10 | Arg rules. `args` must be an object, otherwise `missing arg: X`. Every documented arg is **required**. A number field accepts `isInt`, `isInt64`, or `isDouble`, and must be finite (`non-finite: X`). A bool field accepts only `isBool`. Int fields use `lround` after the clamp. Seed is a string matching `-?[0-9]+`, parsed by `strtoll` with an end-pointer and `ERANGE` check. Unknown keys fail only in the two merge commands (`unknown field: X`) | Coerce strings to numbers; optional `overwrite` | Strict and predictable. The rules map onto pinned decision 3. |
| D11 | Error tokens: `unknown command: X`, `missing arg: X`, `invalid type: X`, `non-finite: X`, `unknown field: X`, `invalid enum: X`, `busy`, `exists`, `path not absolute`, the `PresetResult` names (`fileNotFound`, `nameInvalid`, …), and the `MidiFileWriteResult` names | Free-form prose | Machine-matchable in Slice 2. |
| D12 | Clamp bounds: SynthPatch `kMin*/kMax*`. Generation: pulses `[0,16]`, rotation `[0,15]`, stepProbability `[0,1]`, root `[0,11]`, range `[0,127]` followed by `normalizePitchRange`. autoEvolveRate `[1,16]`. BPM uses `setBpm`'s own clamp | — | These mirror the editor widget ranges (Editor.cpp:285-379). |
| D13 | `exportMidi` checks `juce::File::isAbsolutePath(path)` **before** constructing a `juce::File`; a relative path returns `path not absolute` with no `File` constructed and nothing written. Otherwise `exportMidiTo(juce::File(path))`, mapping a non-`ok` `MidiFileWriteResult` to its enum name | Construct `juce::File(path)` and test `exists()`/parent | `juce::File`'s constructor asserts in debug on a relative path, so the check must come first. |
| D14 | Busy is checked at the root. `BerlinAudioProcessor::regenerate(drawNewSeed)` returns `false` when `player.isPublishPending()` **before** it draws a new seed, so a busy regenerate changes nothing (seed, sequence, `mutationCount` unchanged). The `publishSequence` result check stays as a defensive second guard. `mutate()` already changes no state before its busy return (the candidate derives deterministically from `sequenceSeed`/`mutationCount`), so it needs no change. The seed is drawn only when `drawNewSeed && !generationParams.lockSeed` (existing behavior, kept) | Detect busy in `UiBridge` before calling `regenerate` | Today a busy `regenerate(true)` overwrites `currentSeed` and then returns `false`, leaving the seed out of sync with the playing sequence. The root fix covers the bridge and also the old editor's Randomize button, which has the same latent flaw. |

## Data Flow

    play/stop:  dispatch("setPlaying") -> proc.setPlaying -> player.playRequested.store   [msg]
                processBlock -> player.process: adopt seq -> setBpm -> adopt play/stop     [audio]
                    stop: transport.stop(); push note-off@0 -> translator (MIDI) + synth.render (release)
    level:      dispatch("setMasterLevel"|"setPatch"|loadPreset|setState) -> proc.setMasterLevel/pushPatchToSynth
                    -> currentPatch.outputLevel + synth.masterLevel.store                 [msg]
                synth.render: voice -> FX -> masterGain.applyGain(scratch) -> destination.addFrom  [audio]
    delay sync: setPatch/setBpm -> applySyncedDelayTime -> synth.setDelayTimeSeconds       [msg]

## Threading

| Call | Thread |
|---|---|
| `UiBridge::dispatch/snapshot`, all processor setters and getters, `listPresetNames`, I/O | Message |
| `SequencePlayer::setPlaying`, `SynthEngine::setMasterLevel` (relaxed stores) | Message |
| `isPlayRequested`, `getPlayheadStep`, `getLoopCount`, `isEnabled`, `isEffectsEnabled`, `getMasterLevel` (atomic loads) | Any |
| Adopting the play request, `masterGain` ramp | Audio (`process`/`render`), with no allocation, lock, or log |

## Old Editor Equivalence (D7)

`recomputeSyncedDelayTime` sets `delayTimeSlider` (`setRange(min,max)`, interval 0, Editor.cpp:98) to the double `delaySecondsFor(owner.getBpm(), div)`. `currentPatchFromWidgets` then casts it to `float`. The engine computes the same float from the same `currentBpm` and division, so the overwrite in `setPatch` produces identical bits and changes nothing. In `pushTempoFromWidgets`, `setBpm` now pushes the derived value first, and the editor's `setPatch` that follows pushes the same value again, so the audio target is identical. Free mode is never touched. The only observable difference is a hand-edited preset whose stored synced time is inconsistent: it now loads with the correctly derived time. That is intended.

## Interfaces / Contracts

```cpp
// SequencePlayer
void setPlaying (bool shouldPlay) noexcept;      // message thread
bool isPlayRequested() const noexcept;           // any thread
// SynthEngine
void  setMasterLevel (float) noexcept;  float getMasterLevel() const noexcept;
bool  isEnabled() const noexcept;       bool  isEffectsEnabled() const noexcept;
// BerlinAudioProcessor (message thread)
void setPlaying (bool);  bool isPlaying() const;  int getPlayheadStep() const;  int getLoopCount() const;
bool isSynthEnabled() const;  bool areEffectsEnabled() const;
void setMasterLevel (float);  float getMasterLevel() const;   // == currentPatch.outputLevel
bool isAutoEvolveEnabled() const;  int getAutoEvolveRate() const;
// UiBridge (Source/bridge/UiBridge.h; forward-declares BerlinAudioProcessor; headless-safe)
explicit UiBridge (BerlinAudioProcessor&) noexcept;
juce::var dispatch (const juce::String& command, const juce::var& args); // {ok, error, snapshot?}
juce::var snapshot() const;
```

The snapshot keys are `patch` (21 fields, named like the preset XML attributes: enums as names, `delaySynced` as a bool), `generationParams` (9 fields), `seed` (decimal string), `bpm`, `playing`, `playheadStep`, `loopCount`, `synthEnabled`, `effectsEnabled`, `masterLevel`, `autoEvolveEnabled`, `autoEvolveRate`, `mutationCount`, `steps` (16 × `{note, active}`), and `presetNames`.

`patch` keys (the preset XML attribute names): `waveform`, `cutoffHz`, `resonance`, `pulseWidth`, `attack`, `decay`, `sustain`, `release`, `lfoRateHz`, `lfoDepth`, `lfoDestination`, `delayTimeSeconds`, `delayFeedback`, `delayMix`, `reverbRoomSize`, `reverbDamping`, `reverbWetLevel`, `reverbDryLevel`, `outputLevel`, `delaySynced`, `delayDivision`. `generationParams` keys: `mode`, `pulses`, `rotation`, `stepProbability`, `lockSeed`, `scaleType`, `rootPitchClass`, `rangeLow`, `rangeHigh`. Enum values are the PresetManager.cpp table names: waveform `saw|square|pulse|triangle`; lfoDestination `pitch|cutoff|amplitude|pulseWidth`; scaleType `minor|major|dorian|phrygian|mixolydian|harmonicMinor`; delayDivision `half|quarter|dottedEighth|eighth|eighthTriplet|sixteenth`; mode `random|euclidean|probability`. Command args: `setPlaying {playing}`, `setBpm {bpm}`, `setSeed {seed}`, `regenerate {randomize}`, `mutate {}`, `setSynthEnabled {enabled}`, `setEffectsEnabled {enabled}`, `setMasterLevel {level}`, `setAutoEvolveEnabled {enabled}`, `setAutoEvolveRate {rate}`, `savePreset {name, overwrite}`, `loadPreset {name}`, `exportMidi {path}`; `setPatch`/`setGenerationParams` take any subset of their keys. Result: `{ok: true, error: "", snapshot}` or `{ok: false, error: <token>}` (no `snapshot` key on failure).

## File Changes

| File | Action | ~Lines |
|---|---|---|
| `Source/playback/SequencePlayer.h/.cpp` | Modify: D1-D2 | +26 |
| `Source/synth/SynthEngine.h/.cpp` | Modify: D5, getters | +36 |
| `Source/synth/SynthPatch.h` | Modify: `outputLevel` comments only | ±4 |
| `Source/plugin/BerlinAudioProcessor.h/.cpp` | Modify: new API, D6, D7, D14 (busy check before seed draw), `#include "core/TempoSync.h"` | +55 |
| `Source/preset/PresetManager.h/.cpp` | Modify: D8 forwarders | +22 |
| `Source/plugin/BerlinAudioProcessorEditor.cpp` | Modify: `currentPatchFromWidgets` keeps `owner.getPatch().outputLevel` (approved exception to pin 14) | +1 |
| `Source/bridge/UiBridge.h/.cpp` | Create (incl. D13 absolute-path check) | +292 |
| `Berlin.jucer` | UiBridge h/cpp (new `bridge` group) | +4 |
| `Plugin/BerlinPlugin.jucer` | UiBridge h/cpp **and TempoSync h/cpp** (TempoSync is currently missing even though the editor calls `delaySecondsFor`) | +8 |
| `Tests/BerlinTests.jucer` | `UiBridgeTests.cpp`, plus `UiBridge.h/.cpp` under its own ids (the tests jucer compiles shared Source files itself, e.g. `tlBapC`) | +6 |
| `Tests/Source/UiBridgeTests.cpp` | Create (incl. its own `TempPresetDir` copy) | +250 |
| `Tests/Source/SequencePlayerStopTests.cpp`, `SynthEngineTests.cpp`, `BerlinAudioProcessorTests.cpp` | Modify | +155 |

**Estimate: about 865 changed lines (production about 460, tests about 405). This exceeds the 800 ceiling. Delivery decision (user, 2026-10-02): ONE PR with a recorded `size:exception`, same as Slice 2's tempo-delay-ui. The split below is kept only as the suggested review order inside that PR (engine first, then bridge):**
- **PR1 (engine + tests, about 290 lines):** `SequencePlayer` (D1-D4), `SynthEngine` (D5), `SynthPatch.h` comments, `BerlinAudioProcessor` (D6, D7, D14, getters), the one-line editor exception, and the `SequencePlayerStopTests`/`SynthEngineTests`/`BerlinAudioProcessorTests` additions.
- **PR2 (UiBridge + jucers + bridge tests, about 575 lines):** `UiBridge.h/.cpp` (D9-D13), the `PresetManager` forwarders (D8, consumed only by the bridge), the three `.jucer` registrations (plus TempoSync in `BerlinPlugin.jucer`), and `UiBridgeTests.cpp`.

PR2 depends on PR1. Smaller cuts inside PR2 if needed: table-driven invalid-arg tests (one loop).

## Testing Strategy (Strict TDD, RED first)

| Layer | What | How |
|---|---|---|
| SequencePlayer | `setPlaying(false)` gives exactly one note-off at offset 0 on the next `process()` and nothing afterwards, with the playhead frozen. `isPlayRequested()` updates before any `process()`. A direct `stop()` still keeps `pendingNote`, and the following `process()` emits nothing (transition-only adopt). **Restart:** all-active sequence, 2-sample blocks; stop is adopted when the position is 2 samples into step 5 (step 5 note-on already emitted); after `setPlaying(true)`, the next 4-sample `process()` emits exactly one event, `{offset 2, step 6, note-on}` (no note-off, since the stop already flushed it), and no event carries step 5 or step 0 | `sampleRate=4`, `bpm=60`, `spb=1` fixture (`samplesPerStep==4`), the same as the existing Stop tests |
| SynthEngine | Level 0.5 set before `prepare` gives exactly 0.5× of a reference engine. Switching 1→0 ramps linearly (ratio ≈ `1 - i/2205` at 44.1 kHz), with exact zeros after 2205 samples. Clamp and NaN set `getMasterLevel` to 0. Enable getters. **Default master level is 0.8:** `SynthPatch{}.outputLevel == 0.8f`, a fresh `SynthEngine::getMasterLevel() == 0.8f`, and a fresh processor's `getMasterLevel() == 0.8f` | Two identically prepared engines with the same events |
| Processor | Adoption is observed headlessly with `prepareToPlay(44100,512)` and `processBlock` (the existing pattern): stop gives a note-off in `midi` and no later note-on. Level 0 keeps MIDI identical. `setPatch`, `loadPreset` (`TempPresetDir`), and `setStateInformation` apply `outputLevel` live. Synced delay is derived on `setPatch`/`setBpm`/`loadPreset`/state, Free mode is untouched, and an editor-style pre-derived patch is a no-op. **D14:** `regenerate(false)` then `regenerate(true)` with no `processBlock` returns `false` and leaves `getSeed()`, `getCurrentSequence()`, and `getMutationCount()` unchanged; busy `mutate()` likewise | Existing `BerlinAudioProcessorTests` style |
| UiBridge | 15 commands × (valid, invalid). Unknown command. Generic invalid matrix: missing, wrong type, NaN/inf, unknown field, bad enum, each asserting the exact D11 token. **No state change is asserted as `JSON::toString(snapshot())` being equal before and after.** Clamping, partial merge, seed overflow, `exists`, `busy` (two regenerates with no `processBlock`). **Busy randomize:** after a pending `regenerate {randomize:false}`, `regenerate {randomize:true}` returns `{ok:false, error:"busy"}` and the snapshot JSON (seed included) is identical before and after. **lockSeed:** with `lockSeed:true`, `regenerate {randomize:true}` keeps the seed; with `false` it changes (each non-busy regenerate test uses a fresh processor or drains the pending publish with `prepareToPlay` + `processBlock` first, otherwise it reads `busy`). **Relative export path** (D13): `exportMidi {path:"output.mid"}` returns `path not absolute`, no debug assertion fires, no file appears. Snapshot shape (16 steps, string seed) | `UiBridgeTests.cpp` defines its own copy of the `TempPresetDir` helper (the existing one is file-local to `BerlinAudioProcessorTests.cpp:30`); a temp file for export |

## Threat Matrix

N/A. There is no routing, shell, subprocess, VCS/PR automation, executable-file classification, or process-integration boundary. `exportMidi` writes to a user-chosen absolute path. The existing `MidiFileWriter` handles it, and a relative path is rejected before any `juce::File` is constructed (D13). Preset names keep `fileForName`'s containment guard.

## Migration / Rollout

No schema change. The live `outputLevel` makes default output 0.8× (about -1.9 dB). The proposal accepts this.

## Open Questions

- [x] The old editor's `currentPatchFromWidgets` always sends `outputLevel=0.8`. Touching any knob after loading a preset with a different level would reset it to 0.8. **Resolved (orchestrator, 2026-10-02):** pinned decision 14 is relaxed for this one line. `currentPatchFromWidgets` carries over `owner.getPatch().outputLevel` instead of the default, preventing a regression this change would otherwise introduce.
- [ ] `UiBridge` commands do not call `sendChangeMessage()`. Slice 1 has to decide how the old editor is refreshed if both editors ever coexist.
- [ ] Every snapshot includes `presetNames`, which parses every preset file. Slice 1 should measure this for high-rate commands such as knob drags.
