## Exploration: UI Redesign Slice 0 — Engine API for the UI (C++ only)

### Current State

**Transport/SequencePlayer thread model (investigation 1)**
- `Transport::start()/stop()/reset()` (Source/playback/Transport.cpp:43-51,35-41) write plain non-atomic fields (`running` bool, `position`, `nextStepCounter`, `originSample`, `originStep`) with zero synchronization.
- `SequencePlayer::start()/stop()/reset()` (Source/playback/SequencePlayer.cpp:31-47) forward directly to Transport with no handoff.
- Today's only call site is `BerlinAudioProcessor`'s ctor (`player.start()`, BerlinAudioProcessor.cpp:34), which runs before `prepareToPlay`/`processBlock` ever execute — so no real concurrent access occurs today. `MainComponent.cpp:60` explicitly comments "Deliberately does NOT call player.stop()" — stop()/reset() are literally unused in production beyond construction.
- BPM already has the correct pattern to copy: `SequencePlayer::setBpm` stores into `std::atomic<double> pendingBpm` (message thread), adopted at the top of the next `process()` call (design.md Decision 4), right before `countBoundaries`.
- Held note on stop: `SequencePlayer::flushPendingNoteOff` (SequencePlayer.cpp:122) is the only existing note-off-flush mechanism, called from `BerlinAudioProcessor::releaseResources`/`MainComponent::releaseResources` — both contexts have JUCE's "no concurrent processBlock" guarantee. A new `setPlaying(false)` UI command has NO such guarantee.
- Recommendation: add an atomic playing-request (mirroring `pendingBpm`) on `SequencePlayer`, adopted inside `process()`; when a stop is adopted, emit the pending note-off through the normal event path (same pattern already used in the sequence-adopt branch, lines 70-71) rather than calling `flushPendingNoteOff` from the message thread.
- Plugin vs standalone: `processBlock` already unconditionally clears the buffer/MIDI; `Transport::advance`/`countBoundaries` already no-op while `!running`. The only new risk is the message-thread-to-audio-thread request itself, equally applicable to both targets since the future UiBridge is driven from the message thread in both.

**Master gain / outputLevel (investigation 2)**
- `SynthPatch::outputLevel` (SynthPatch.h:125, bounds at :82) is explicitly commented "preset-persistence bound only ... no live setter exists". `SynthEngine::pushPatchToSynth` (BerlinAudioProcessor.cpp:273-298) forwards 18 SynthPatch fields to `synth`/`effects` setters but never `outputLevel`.
- Real-time render path: `SynthEngine::render()` (SynthEngine.cpp:24-107) renders the voice into `scratch`, conditionally applies `effects.process()`, then does the terminal `destination.addFrom(ch, startSample, scratch, ch, 0, numSamples)` loop (line 104-106) — this is the correct real-time-safe chokepoint for a master-gain multiply.
- `enabled`/`effectsEnabled` on `SynthEngine` are already `std::atomic<bool>` written from the message thread, read once per `render()` call with relaxed ordering (Decision 6 in the file header) — the established pattern to copy for a new `std::atomic<float> masterLevel` on `SynthEngine`, driving a `juce::SmoothedValue<float>` (or manual ramp) applied to `scratch` before the terminal `addFrom`.
- Recommend keeping `SynthPatch::outputLevel` as the PERSISTED value only; add `BerlinAudioProcessor::setMasterLevel(float)`/`getMasterLevel()` that updates `currentPatch.outputLevel` (message thread) AND forwards to the new `synth.setMasterLevel()` atomic — exactly mirroring how `setBpm` is single-source-of-truth for `currentBpm` while also forwarding to `player.setBpm`.
- MIDI scaling: confirmed NO. `MidiEventTranslator::translate` (Source/midi/MidiEventTranslator.cpp:22) emits note on/off + fixed velocity only from the same `StepEventBuffer` `SynthEngine::render` consumes — it has no audio/gain concept at all. A master output level only ever touches `destination` (the audio buffer); zero effect on `midiMessages`.

**synthEnabled/effectsEnabled getters (investigation 3)**
- Both already `std::atomic<bool>` private members on `SynthEngine` (SynthEngine.h:111,114), written via `setEnabled`/`setEffectsEnabled`. No getters exist on `SynthEngine` or `BerlinAudioProcessor`. Trivial addition: `bool isEnabled() const noexcept { return enabled.load(std::memory_order_relaxed); }` style getters (same convention as `getPlayheadStep`/`getLoopCount`), then thin forwarders on `BerlinAudioProcessor`.
- `autoEvolveEnabled`/`autoEvolveRate` (BerlinAudioProcessor.h:180-181) are plain `bool`/`int`, not atomics — only ever read/written on the message thread (editor clicks, Timer callback), so plain getters suffice, no atomics needed.

**UiBridge design inputs (investigation 4)**
- `juce::var`/`juce::JSON`/`juce::DynamicObject` all live in `juce_core`, already a transitive dependency of `juce_audio_processors_headless` (which `BerlinAudioProcessor.h` already includes) — zero new module dependency, usable in the headless test target as-is.
- Test pattern confirmed in `Tests/Source/BerlinAudioProcessorTests.cpp`: tests that don't touch presets construct `berlin::BerlinAudioProcessor a;` (default ctor); tests that DO touch presets use `TempPresetDir temp; berlin::BerlinAudioProcessor processor (temp.dir);` (lines 173-174, 200, 632) — new UiBridge tests should follow the same TempPresetDir pattern whenever `save`/`loadPreset`/`listPresetNames` commands are exercised.
- Editor action inventory (`BerlinAudioProcessorEditor.cpp`) → processor call → bridge command:
  | Editor action | Processor call | Notes |
  |---|---|---|
  | Export MIDI button | `exportMidiTo(file)` | native file chooser stays in editor per the plan; bridge only needs the write call |
  | Synth toggle | `setSynthEnabled(bool)` | needs new getter for snapshot |
  | FX toggle | `setEffectsEnabled(bool)` | needs new getter for snapshot |
  | Tempo slider | `setBpm(double)` | single source of truth already |
  | Delay sync toggle / division box | `setPatch(patch)` (recomputed delayTimeSeconds) | pure helpers `delaySecondsFor`/`formatDelayRecommendations` (core/TempoSync.h) are JUCE-free, reusable |
  | Delay/reverb sliders (7) | `setPatch(patch)` | whole-struct rebuild from ALL widgets each time (`currentPatchFromWidgets`) |
  | Seed editor | `setSeed(int64)` | editor-side validates digits only; bridge must validate independently |
  | Generate button | `setGenerationParams(params)` then `regenerate(false)` | |
  | Randomize button | `setGenerationParams(params)` then `regenerate(true)` | |
  | Mutate button | `mutate()` | |
  | Lock-seed toggle | `setGenerationParams(params)` (sets lockSeed) | |
  | Save preset | `presetExists(name)` then `save(name)` | native overwrite confirm stays in editor |
  | Load preset | `loadPreset(name)` | |
  | Auto-evolve toggle | `setAutoEvolveEnabled(bool)` | needs new getter |
  | Evolve-rate box | `setAutoEvolveRate(int)` | needs new getter |
  | Waveform/pulse-width/cutoff/resonance/ADSR/LFO (11 widgets) | `setPatch(patch)` | same whole-struct rebuild |
  | Rhythm mode/pulses/rotation/probability/scale/root/range (7 widgets) | **staged only** — committed via `setGenerationParams` solely inside Generate/Randomize/LockSeed handlers, no individual onChange push | Important nuance: today's editor has NO live single-field commit path for these 7 fields; the bridge, being stateless, should commit `setGenerationParams` immediately on each call — a (minor, non-blocking) UX simplification vs. today's staged widgets |
  | Status label / preset list refresh | read-only, `listPresetNames()`/`getPatch()`/`getGenerationParams()`/`getSeed()` | feeds `snapshot()` |
- Snapshot must contain: full `SynthPatch` (patch fields incl. the new live `outputLevel`), `GenerationParams`, seed, bpm, transport (`isPlaying`, `getPlayheadStep`, `getLoopCount`), toggles (`isSynthEnabled`, `areEffectsEnabled`), auto-evolve state+rate (new getters), mutation count (`getMutationCount`, existing), sequence steps note/active (`Sequence`/`Step`, Source/core/Sequence.h, Step.h — `note`/`active` fields), preset names (`listPresetNames`), playhead (already atomic).

**File registration (investigation 5)**
- Confirmed 3-way `.jucer` XML pattern via existing `BerlinAudioProcessor.{h,cpp}` entries:
  - `Berlin.jucer:78-79` — `<FILE id="plBapH" .../>` / `<FILE id="plBapC" .../>`, `file="Source/plugin/BerlinAudioProcessor.h"` (relative to repo root)
  - `Plugin/BerlinPlugin.jucer:13-16` — same ids, `file="../Source/plugin/..."` (relative to `Plugin/`)
  - `Tests/BerlinTests.jucer:60-61` — test-only files use their own ids (e.g. `btBapC`), `file="Source/BerlinAudioProcessorTests.cpp"` (relative to `Tests/`)
- A new production file (e.g. `Source/bridge/UiBridge.h/.cpp`) needs one `<FILE>` pair added to BOTH `Berlin.jucer` and `Plugin/BerlinPlugin.jucer` (different relative path prefixes). A new test file (`Tests/Source/UiBridgeTests.cpp`) needs one `<FILE>` entry added to `Tests/BerlinTests.jucer` only.
- A forgotten registration fails loudly as an unresolved-external link error (established project convention, documented in Transport.h/SequencePlayer.h file headers) — a safety net, not a silent bug.
- Recommended new location: `Source/bridge/UiBridge.h`/`.cpp` (new directory, sibling to `Source/plugin/`) rather than inside `Source/plugin/` — keeps `BerlinAudioProcessor` focused and gives the Slice 1+ `ui/src/bridge/` mirror a 1:1 naming match, per the plan's own "hexagonal port/adapter" framing.

### Affected Areas
- `Source/playback/SequencePlayer.h`/`.cpp` — add atomic play-request handoff + adopt-time note-off-on-stop logic
- `Source/synth/SynthEngine.h`/`.cpp` — add `masterLevel` atomic + smoothed gain in `render()`, add `isEnabled()`/`isEffectsEnabled()` getters
- `Source/synth/SynthPatch.h` — no field changes; `outputLevel`'s doc comment should be updated once a live setter exists
- `Source/plugin/BerlinAudioProcessor.h`/`.cpp` — add `getPlayheadStep()`/`getLoopCount()` passthroughs, `setPlaying(bool)`/`isPlaying()`, `isSynthEnabled()`/`areEffectsEnabled()`, `setMasterLevel(float)`/`getMasterLevel()`, `isAutoEvolveEnabled()`/`getAutoEvolveRate()` getters
- `Source/bridge/UiBridge.h`/`.cpp` (new) — command dispatcher + `snapshot()`
- `Berlin.jucer`, `Plugin/BerlinPlugin.jucer` — register new `UiBridge.{h,cpp}` FILE entries
- `Tests/BerlinTests.jucer` — register new `UiBridgeTests.cpp` FILE entry
- `Tests/Source/UiBridgeTests.cpp` (new) — command/validation/snapshot coverage, following the `TempPresetDir` pattern for preset-touching commands

### Approaches

1. **String-command dispatcher with `juce::var` args** — `juce::var dispatch(const juce::String& command, const juce::var& args)` + `juce::var snapshot() const`, backed by an internal name→handler table; each handler extracts/validates its own args inline.
   - Pros: 1:1 match with JUCE 9 `WebBrowserComponent::withNativeFunction`'s actual call shape (name + var array) that Slice 1 will wire directly; minimal type surface; easy to extend per command.
   - Cons: stringly-typed at the boundary; typos/shape mistakes are runtime errors, not compile-time.
   - Effort: Low-Medium.

2. **Typed C++ command structs + separate var codec** — a `std::variant<SetBpmCommand, SetPatchCommand, ...>` of ~18-20 structs, each with its own var encode/decode pair.
   - Pros: compile-time exhaustiveness via `std::visit`; each command's shape is a named, testable type independent of var encoding.
   - Cons: doubles the surface (struct + codec per command) without removing the stringly-typed outer JS boundary (still need a string→command-type lookup somewhere); pushes this slice past its ~400-600 line budget for no behavioral gain yet (no second consumer of the typed layer exists until Slice 2).
   - Effort: High.

3. **Hybrid: dispatch table + inline typed extraction, no separate codec types (recommended)** — same single `dispatch()`/`snapshot()` entry points as Option 1, but implemented via an explicit dispatch table (array/map of name→`std::function<juce::var(BerlinAudioProcessor&, const juce::var&)>`) instead of an if/else chain, with each handler validating/clamping its own args and returning a small `{ok, error}`-shaped `juce::var` result.
   - Pros: same low type surface as Option 1, but more readable/extensible than a long if-chain; keeps validation explicit and unit-testable per command (one `beginTest` per command + invalid-input case, matching the plan's "unit-tested for every command, including invalid input" requirement); gives Slice 2's `ui/src/bridge/` a clean 1:1 protocol to mirror.
   - Cons: still stringly-typed (same as Option 1) — acceptable since the real boundary (JS) is string-keyed regardless.
   - Effort: Low-Medium.

### Recommendation
Option 3 (hybrid dispatch table, no separate typed-command layer). It matches the plan's own stated design intent, fits comfortably inside the medium (~400-600 line) size estimate, and gives Slice 1/2 a protocol shape that requires no redesign later. Defer Option 2's typed layer indefinitely — introduce it only if a second non-JS consumer of commands ever appears (none is planned).

For the three C++-engine sub-problems (transport play/stop, master gain, enable-getters): copy the existing `pendingBpm`/`enabled`/`effectsEnabled` atomic patterns exactly rather than inventing new cross-thread primitives — the codebase has zero prior art for anything heavier (no `AbstractFifo`/lock use anywhere in this area), and nothing in Slice 0's scope needs anything heavier than a few more atomics plus one adopt-time note-off tweak inside `SequencePlayer::process()`.

### Risks
- Reusing `Transport::start()/stop()` directly from a message-thread `setPlaying()` call would introduce a genuine data race on the non-atomic `running` bool (and other non-atomic Transport fields) the moment the audio thread is live — must go through a new atomic handoff adopted inside `process()`, mirroring `pendingBpm`.
- Calling `flushPendingNoteOff` from the message thread (instead of adopting the stop + emitting the note-off inside `process()` on the audio thread) would race `pendingNote`/`blockEvents`, which are documented audio-thread-exclusive state outside the `releaseResources` exclusivity guarantee.
- `outputLevel` duplicated between `currentPatch.outputLevel` (persisted) and a new live atomic on `SynthEngine` — must be kept in sync inside one setter (`setMasterLevel`), exactly like `setBpm` keeps `currentBpm` and `player`'s internal BPM state in sync, to avoid persisted/audible drift.
- Three-file `.jucer` registration (two production, one test) is easy to under-register; the project's existing convention makes a miss fail loudly at link time rather than silently, but still costs a build cycle.
- The 7 "staged" generation-param widgets (rhythm/pulses/rotation/probability/scale/root/range) have no individual live-commit path today — the bridge committing `setGenerationParams` immediately on each call is a (minor, intentional) behavior change from today's editor; flag for `sdd-propose` as a conscious scope decision, not an oversight.
- CodeGraph's watcher flagged `Transport.cpp`, `SequencePlayer.cpp`, `BerlinAudioProcessor.h`/`.cpp`, `SynthEngine.cpp` as "edited Nms ago, pending sync" during this exploration; all five were re-read directly from disk and their content matches what CodeGraph returned — treated as filesystem/watcher noise, not an actual pending edit, but worth re-verifying if `sdd-propose`/`sdd-design` see the same stale warning later.

### Ready for Proposal
Yes. Scope, affected files, and the three thread-safety/design sub-problems (play/stop handoff, master-gain placement, enable-getters) are well understood with concrete code references. Recommend `sdd-propose` proceed with: `SequencePlayer` atomic play-request + adopt-time note-off, `SynthEngine::masterLevel` atomic + smoothed gain in the terminal `addFrom` chokepoint, trivial atomic-backed getters for `enabled`/`effectsEnabled`, and a hybrid string-dispatch-table `UiBridge` in a new `Source/bridge/` directory registered across all three `.jucer` files.
