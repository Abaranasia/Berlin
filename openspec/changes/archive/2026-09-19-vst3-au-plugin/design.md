# Design: VST3/AU Plugin (roadmap Phase 11)

## Technical Approach

Split `MainComponent`'s triple duty into `BerlinAudioProcessor` (engine + commands + state, GUI-free) and `BerlinAudioProcessorEditor` (widgets). `MainComponent` becomes a standalone shell owning one of each plus the standalone-only `MidiOutputSink`. `Berlin.jucer` gains an `audioplug` export beside the existing `guiapp`.

The binding constraint the proposal missed: **`Tests/BerlinTests.jucer` does NOT declare `juce_audio_processors*`** (only `juce_audio_basics`, `juce_audio_formats`, `juce_core`, `juce_data_structures`, `juce_dsp`, `juce_events`). `juce_audio_processors_headless` is declared in `Berlin.jucer`, not the test project. So headless coverage requires adding that module to the test project, and `BerlinAudioProcessor.cpp` must never include `juce_gui_basics` or `juce_audio_devices`. That constraint drives D1/D2/D6 below.

Second correction: the class is `berlin::MidiEventTranslator` (not `StepEventTranslator`), and it **already** writes into a `juce::MidiBuffer&`. The plugin MIDI change is therefore a destination swap, not a rewrite.

## Architecture Decisions

### D1 — Processor owns parameter *values*, not widgets

**Choice**: Add JUCE-free `berlin::GenerationParams` (mode/pulses/rotation/stepProbability/lockSeed) in `Source/generation/`; the processor holds `SynthPatch` + `GenerationParams` + seed as members. Editor widgets push values in and read values back.
**Alternatives**: processor reads widgets via a back-pointer (today's shape); `AudioProcessorValueTreeState`.
**Rationale**: `regenerate`/`mutate`/`pushAllParametersToSynth`/`currentPatchFromWidgets` all read widgets today. A plugin editor is destroyed whenever its window closes, so widget-owned state would be lost and auto-evolve would crash. APVTS is an explicit non-goal.

### D2 — Processor returns results; the editor renders status text

**Choice**: `regenerate`/`mutate` return `bool`; preset ops return `berlin::PresetResult`; export returns `berlin::MidiFileWriteResult`. `describePresetFailure`, `describeWriteFailure`, `statusLabel`, the overwrite `NativeMessageBox` and the `FileChooser` all live in the editor.
**Alternatives**: processor sets a status string.
**Rationale**: keeps `BerlinAudioProcessor.cpp` free of `juce_gui_basics` so it links in the console test target; makes every command assertable headlessly.

### D3 — `juce::ChangeBroadcaster` for processor→editor notification

**Choice**: processor derives from `ChangeBroadcaster`; editor is a `ChangeListener` registering in its ctor and deregistering in its dtor. Broadcast after auto-evolve `mutate()` and after `setStateInformation`.
**Alternatives**: `std::function` callback set by the editor.
**Rationale**: a raw `std::function` dangles when the host destroys the editor. `juce_events` is already in both targets.

### D4 — Generation params are NOT persisted (proposal D3 closed)

**Choice**: accept the gap. `getStateInformation`/`setStateInformation` reuse `PresetManager::toValueTree`/`fromValueTree` verbatim — patch + seed only, no schema change, `kSchemaVersion` stays 1.
**Alternatives**: add a `Generation` child for mode/pulses/rotation/probability.
**Rationale**: user decision (obs #267). **Stated consequence**: a session saved in Euclidean or Probability mode reloads in Random mode and therefore re-derives a *different* sequence from the same seed. Listed as a risk, not a bug.

### D5 — Editor construction is compile-gated, not runtime-gated

**Choice**: define `createEditor()` in `BerlinAudioProcessor.cpp` behind `#if BERLIN_HEADLESS` (returns `nullptr`; `hasEditor()` returns `false`), with the editor `#include` under the same guard. Add `BERLIN_HEADLESS=1` to `Tests/BerlinTests.jucer`'s existing `defines`.
**Alternatives**: put `createEditor()` in the editor TU (unresolved external in the test link); always construct the editor (drags `juce_gui_basics` into the test target).
**Rationale**: `createEditor()` is pure virtual, so it must be defined in every build; this is the only seam that satisfies both link and module constraints.

### D6 — Extract two pure helpers into `Source/generation/`

**Choice**: move `MainComponent::buildSeededSequence` to `berlin::buildSeededSequence` in `SequenceBuilder.{h,cpp}`, and the auto-evolve re-baselining arithmetic (`lastMutationLoopCount`/`dueAtLoopCount`) into a JUCE-free `berlin::AutoEvolveSchedule`.
**Alternatives**: keep both inside the processor.
**Rationale**: both are pure, currently untested, and become coverable in the existing `juce_core`-only target without any new module — the cheapest real TDD win in this change.

### D7 — One `.jucer`, two targets (proposal D4, unchanged)

Add the `audioplug` export to `Berlin.jucer` so module and `<FILE>` lists cannot drift between binaries.

## Data Flow

```
                         ┌──────────────────────────────────┐
  message thread         │      BerlinAudioProcessor        │      audio thread
  ─────────────          │  SynthPatch · GenerationParams   │      ────────────
  Editor widget ──set*──▶│  seed · currentSequence          │
  Editor button ─cmd───▶ │  SequencePlayer · SynthEngine    │──▶ processBlock(buffer, midi)
                         │  MidiEventTranslator             │      buffer.clear()
  Timer (auto-evolve) ──▶│  PresetManager · Timer           │      player.process  → blockEvents
                         └──────────────┬───────────────────┘      translate(blockEvents, midi)
                          ChangeBroadcaster│                       synth.render(blockEvents, buffer, 0, n)
                                           ▼
                                   Editor refreshes

  PLUGIN     host ──▶ processBlock(buffer, midiMessages) ──▶ MIDI leaves via midiMessages
  STANDALONE MainComponent::getNextAudioBlock ──▶ processBlock(subView, midiBlock)
                                               └──▶ midiSink.dispatch(midiBlock, n)
```

`MidiOutputSink` is reachable only from `MainComponent`. It is never referenced by `BerlinAudioProcessor`.

## File Changes

| File | Action | Description |
|---|---|---|
| `Source/plugin/BerlinAudioProcessor.{h,cpp}` | Create | Engine owner, `processBlock`, commands, state recall, auto-evolve Timer |
| `Source/plugin/BerlinAudioProcessorEditor.{h,cpp}` | Create | ~40 widgets, `resized()`, callbacks, status text, dialogs |
| `Source/generation/GenerationParams.h` | Create | `RhythmMode` (lifted out of `MainComponent`) + params struct |
| `Source/generation/SequenceBuilder.{h,cpp}` | Create | `buildSeededSequence` moved verbatim (D6) |
| `Source/generation/AutoEvolveSchedule.{h,cpp}` | Create | Re-baselining arithmetic moved verbatim (D6) |
| `Source/MainComponent.{h,cpp}` | Modify | Reduced to `AudioAppComponent` shell + `MidiOutputSink` |
| `Berlin.jucer` | Modify | `audioplug` export, plugin characteristics, new `<FILE>` entries |
| `Tests/BerlinTests.jucer` | Modify | Add `juce_audio_processors_headless` module + `BERLIN_HEADLESS=1` + new `<FILE>` entries |
| `Tests/Source/BerlinAudioProcessorTests.cpp` | Create | Headless processor coverage |
| `Tests/Source/SequenceBuilderTests.cpp` | Create | Pure generation coverage |
| `Tests/Source/AutoEvolveScheduleTests.cpp` | Create | Pure schedule coverage |
| `Source/midi/MidiOutputSink.{h,cpp}` | Unchanged | Standalone-only |
| `Source/playback/*`, `Source/synth/*`, `Source/preset/*` | Unchanged | Reused verbatim |

## Interfaces / Contracts

```cpp
class BerlinAudioProcessor : public juce::AudioProcessor,
                             public juce::ChangeBroadcaster,
                             private juce::Timer
{
public:
    // ---- AudioProcessor ----
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;   // NOTE: arg order is flipped vs AudioAppComponent
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void releaseResources() override;                                       // synth.reset() + discard pending note
    bool isBusesLayoutSupported (const BusesLayout&) const override;        // stereo main out only, no input bus
    void getStateInformation (juce::MemoryBlock&) override;                 // PresetManager::toValueTree -> XML
    void setStateInformation (const void*, int) override;                   // fromValueTree; ignores malformed input
    bool acceptsMidi()  const override { return true; }                     // bus declared per VST3 spec (D8); content still ignored in processBlock
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }                    // generates its own audio -> instrument
    double getTailLengthSeconds() const override;                           // must cover the delay+reverb tail

    // ---- Commands: MESSAGE THREAD ONLY (MutationEngine allocates) ----
    bool regenerate (bool drawNewSeed);                                     // false = busy publish, state unchanged
    bool mutate();                                                          // false = busy publish, state unchanged
    berlin::PresetResult save (const juce::String& name);                   // unconditional; editor owns the overwrite prompt
    berlin::PresetResult loadPreset (const juce::String& name);
    bool                 presetExists (const juce::String& name) const;
    juce::StringArray    listPresetNames() const;
    berlin::MidiFileWriteResult exportMidiTo (const juce::File& destination) const;

    // ---- State in / out (message thread) ----
    void setPatch (const berlin::SynthPatch&);            const berlin::SynthPatch& getPatch() const;
    void setGenerationParams (const berlin::GenerationParams&);
    const berlin::GenerationParams& getGenerationParams() const;
    void setSeed (juce::int64);                           juce::int64 getSeed() const;
    void setSynthEnabled (bool);                          void setEffectsEnabled (bool);
    void setAutoEvolveEnabled (bool);                     void setAutoEvolveRate (int loops);
    int  getMutationCount() const;
    const berlin::Sequence& getCurrentSequence() const;

    // ---- Standalone-only seam: lets MainComponent keep today's OS note-off flush ----
    bool flushPendingNoteOff (juce::MidiBuffer& out) noexcept;
};
```

The standalone sub-buffer view is the one non-obvious mechanical detail — this constructor wraps existing pointers and, at 2 channels, stays inside `AudioBuffer`'s 32-slot inline channel array, so it does **not** allocate:

```cpp
void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& info)
{
    juce::AudioBuffer<float> block (info.buffer->getArrayOfWritePointers(),
                                    info.buffer->getNumChannels(),
                                    info.startSample, info.numSamples);
    processor.processBlock (block, midiBlock);       // clears block + midiBlock internally
    midiSink.dispatch (midiBlock, info.numSamples);  // standalone only
}
```

`processBlock` body, in order: `ScopedNoDenormals` → `buffer.clear()` → `midiMessages.clear()` (this *is* the "host MIDI cleared, not consumed" rule) → `player.process (n, blockEvents)` → `midiTranslator.translate (blockEvents, midiMessages)` → `synth.render (blockEvents, buffer, 0, n)`. No allocation, no lock, no logging.

### `Berlin.jucer` plugin characteristics

| Setting | Value | Why |
|---|---|---|
| `projectType` | `audioplug` (new export; `guiapp` export retained) | D7 |
| `pluginFormats` | `buildVST3,buildAU` | AU builds on macOS only |
| `pluginCharacteristics` | `pluginIsSynth`, `pluginProducesMidiOut` | generates its own audio **and** MIDI |
| `pluginWantsMidiIn` | `1` (**D8 correction**, see below) | required by the VST3 spec for `Instrument`-category plugins |
| *not* set | `pluginIsMidiEffect` | it is not an effect |
| `pluginManufacturerCode` | `Abar` | 4 chars, one uppercase (AU requirement) |
| `pluginCode` | `Brln` | 4 chars, unique |
| `pluginAUMainType` | `'aumu'` | AU music device (instrument) |
| `pluginVST3Category` | `Instrument,Synth` | VST3 classification |
| Bus layout | `BusesProperties().withOutput ("Output", stereo, true)`, no audio input bus (MIDI event input bus IS declared, per D8) | `SynthEngine::scratch` is stereo by design (Decision 4) |

### D8 — Declare a MIDI input bus for VST3 spec compliance; still ignore its content (post-manual-gate correction)

**Choice**: set `pluginWantsMidiIn="1"` in `Plugin/BerlinPlugin.jucer` and change `acceptsMidi()` to `return true`. `processBlock`'s existing unconditional `midiMessages.clear()` is UNCHANGED — the plugin still never reads, forwards, or acts on host MIDI input. Only the bus's *existence* changes, not its *behavior*.

**Why this correction exists**: the original D-something choice (`acceptsMidi() = false`, no `pluginWantsMidiIn`) was caught as spec-invalid during the Phase 8 manual DAW-load gate. Ableton Live's own log recorded the exact rejection: `"plugin has instrument category, but no valid event input bus"` / `"No valid input bus could be found"` / `"Failed: Berlin"`. The VST3 SDK hard-requires an event (MIDI) input bus on any plugin declaring the `Instrument` category — Cakewalk Sonar loaded the plugin anyway (lenient host), which is why this passed that DAW's manual check but not Ableton's.

**Alternatives considered**: (a) drop `Instrument` from `pluginVST3Category` — rejected, Berlin genuinely is an instrument and this would misclassify it in every host's plugin browser; (b) accept the Ableton incompatibility as a known limitation — rejected, Ableton is one of the most widely used DAWs and this is a real spec-compliance bug, not a host quirk.

**Consequence**: `acceptsMidi()` now returns `true`, so the "locked contract" test for `acceptsMidi/producesMidi/isMidiEffect` must be updated to expect `true` for `acceptsMidi()`. The existing "pre-filled input MidiBuffer is cleared; host events never observed" test already covers the actual behavioral guarantee (content ignored) and needs no change — it was testing the right thing all along; only the bus-declaration flag was wrong.

## Testing Strategy

Strict TDD: every row below is RED before the extraction lands.

| Layer | What to Test | Approach |
|---|---|---|
| Unit (pure, no new module) | `buildSeededSequence` determinism per rhythm mode; `AutoEvolveSchedule` re-baselining incl. busy-retry | `juce::UnitTest` in the existing target — D6 makes these reachable today |
| Unit (headless processor) | `regenerate` determinism + busy rejection leaves state unchanged; `mutate` advances `mutationCount` only on success; `getState`/`setState` round-trip; malformed `setStateInformation` is ignored; preset save/load round-trip via a temp dir | Direct `BerlinAudioProcessor` instantiation, no audio device, no editor (D5) |
| Integration (headless) | `prepareToPlay` → N× `processBlock` → `releaseResources` at block sizes 1/7/512/4096 and 44100/48000/96000 Hz; MIDI lands in the passed `MidiBuffer` at expected sample offsets; note-off precedes note-on; a pre-filled input `MidiBuffer` is cleared | Allocate `AudioBuffer<float>`(2, n) + `MidiBuffer`, drive directly |
| Integration (headless) | Silent/bounded output: several hundred blocks with synth+FX enabled stays bounded and finite | `juce-app-testing` feedback-stability rule — `SynthEffects` has a feedback path |
| **Manual gate** | **Plugin format compliance** | See below |

Headless tests MUST NOT call `setAutoEvolveEnabled(true)` — the console target has no running message loop, so the `Timer` never fires. D6 is what makes that logic testable anyway.

Not covered automatically, unchanged from today: the editor, `MidiOutputSink`, and `MidiEventTranslator`'s device-facing use.

### Manual verification gate (matches prior phases' pattern)

`BerlinTests.exe` cannot detect a wrong `pluginCode`, a rejected bus layout, a missing AU type, or editor-resize misbehaviour. Verification MUST record an explicit manual gate:

1. **pluginval** (strictness level 5+) against the built VST3 — the primary automated-but-not-in-harness check.
2. **DAW load**: VST3 in a real host — audio plays through the internal synth; the plugin's MIDI output routes to another track; close and reopen the editor and confirm no state loss and that auto-evolve kept running; save and reload the session and confirm patch + seed return (and record the D4 generation-param reset as expected).
3. **Standalone parity**: the `guiapp` binary behaves exactly as before, including OS MIDI device output and the CC123 panic guard on close.
4. **AU**: macOS-only; explicitly deferred/unverified if no macOS machine is available — say so rather than claiming it passed.

## Threat Matrix

Canonical matrix: **N/A** — this change adds no routing, shell command, subprocess, VCS/PR automation, or executable-file classification. Per-row: documentation-like paths N/A (no file classification); git repository selection, commit state, push state, PR commands all N/A (no VCS automation in this change). The one real adversarial boundary is the untrusted host process, handled as design requirements:

| Host boundary | Expected behavior | Planned RED test |
|---|---|---|
| Arbitrary/varying block size, including 1 and > prepared size | No allocation, no out-of-range write; `SynthEngine` clamps to `scratch` | Block sizes 1/7/512/4096 after preparing at 512 |
| `processBlock` before `prepareToPlay` | No crash, silent output | Direct call without prepare |
| Host supplies a non-empty input `MidiBuffer` | Cleared, never consumed | Pre-filled buffer asserted empty of host events |
| Host calls `setStateInformation` with garbage/truncated data | State left untouched, no crash | Random bytes + truncated XML |
| Host requests an unsupported bus layout | `isBusesLayoutSupported` returns false | Mono/5.1/with-input layouts |
| Editor destroyed while audio runs | Processor state and auto-evolve unaffected | Covered by the manual DAW gate |

## Migration / Rollout

No data migration — `kSchemaVersion` stays 1 and existing preset files load unchanged. Rollout is additive: the `guiapp` export is untouched in behavior, and reverting the `audioplug` export plus deleting `Source/plugin/` removes the plugin.

## Open Questions

None blocking. Two items to confirm at apply time, not design blockers:
- [ ] Whether `juce_audio_processors_headless` alone satisfies the test-target link, or whether `juce_audio_processors` is also required. D5's `BERLIN_HEADLESS` guard works either way; adjust the module list at apply time.
- [ ] `getTailLengthSeconds()` value — read the actual `SynthEffects` delay + reverb tail rather than guessing a constant.
