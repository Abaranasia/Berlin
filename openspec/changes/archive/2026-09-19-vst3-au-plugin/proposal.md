# Proposal: VST3/AU Plugin (roadmap Phase 11)

## Intent

Berlin only runs as a standalone binary, so musicians cannot use it inside a DAW next to their other instruments. `MainComponent` fuses GUI, audio lifecycle and business logic into one class with zero test coverage, so no plugin wrapper is possible without a real split. Ship VST3/AU on the existing engine, and make that engine headlessly testable for the first time.

## Scope

### In Scope
- New `BerlinAudioProcessor` + `BerlinAudioProcessorEditor` under `Source/plugin/`.
- `processBlock` reusing `SequencePlayer::process` / `SynthEngine::render` unchanged; MIDI written into the host's `juce::MidiBuffer&`.
- `getStateInformation`/`setStateInformation` over `PresetManager`'s versioned ValueTree.
- Plugin export target (VST3 + AU) in `Berlin.jucer`.
- `MainComponent` reduced to a standalone shell owning the same processor + editor, keeping `MidiOutputSink`.
- Headless `BerlinAudioProcessor` tests in the existing `Tests/BerlinTests.jucer` target.

### Out of Scope (locked constraints — do not reintroduce)
- Converting standalone to JUCE's Standalone-plugin wrapper. It stays a separate binary.
- Host tempo/transport sync. The plugin uses the internal fixed-BPM `Transport`.
- `AudioProcessorValueTreeState` / host automation lanes. Controls stay Sliders/ComboBoxes.
- CMake / `juce_add_plugin`. Projucer only.
- OS MIDI device output from the plugin.

## Capabilities

### New Capabilities
- `plugin-host-integration`: plugin lifecycle, bus layout, `processBlock` contract, MIDI emission to the host, editor attach/detach.
- `plugin-state-recall`: host session save/restore of processor state.

### Modified Capabilities
- `realtime-audio-wiring`: requirements currently bound to `MainComponent::getNextAudioBlock` must cover the shared processor callback; the `sendBlockOfMessages` lock exception does not exist on the plugin path.
- `midi-output-dispatch`: declared standalone-only; plugin MIDI leaves via the host buffer.
- `unit-test-harness`: adds headless processor-level coverage.

## Approach

**Ownership split**

| Owner | Holds |
|---|---|
| `BerlinAudioProcessor` | `SequencePlayer`, `Transport`, `SynthEngine`, `StepEventTranslator`, `PresetManager`, `currentSequence`/`currentSeed`/`sequenceSeed`/`mutationCount`, generation params, auto-evolve `Timer` + loop counters, `blockEvents`/`midiBlock`, and the commands `regenerate`, `mutate`, `savePreset`, `loadPreset`, `exportMidi` |
| `BerlinAudioProcessorEditor` | the ~40 widgets, `resized()` layout, widget callbacks, status label, preset list refresh — forwards every user action to the processor and reads back state to display |
| `MainComponent` (standalone only) | `AudioAppComponent` shell: owns a processor + editor, forwards `prepareToPlay`/`getNextAudioBlock`/`releaseResources`, and feeds `MidiOutputSink` from the translated buffer |

Widgets move once, not twice. The auto-evolve `Timer` lives on the processor so evolution keeps running while a plugin editor window is closed.

**`getNextAudioBlock` → `processBlock`**

`player.process` and `synth.render` are reused verbatim — both are already allocation-free and lock-free. Mechanical changes only: `prepareToPlay` argument order flips (`sampleRate, samplesPerBlock`); `clearActiveBufferRegion()` becomes `buffer.clear()`; `synth.render` takes `startSample = 0` and `buffer.getNumSamples()`; `ScopedNoDenormals` stays. The MIDI step is the one substantive change (D1).

**Decisions**

- **D1 — `MidiOutputSink` is excluded from the plugin build.** It opens a live OS `juce::MidiOutput` with a background thread; a plugin must not duplicate the host's own routing from inside a real-time callback. Plugin MIDI keeps the same musical intent through a different transport: `StepEventTranslator` output is written into `processBlock`'s `MidiBuffer&`. Host MIDI input is cleared, not consumed, this slice. `MidiOutputSink` and its device-selection UI remain standalone-only.
- **D2 — state recall is patch + seed only**, reusing `PresetManager::toValueTree`/`fromValueTree` so plugin state and preset files share one versioned schema. Stated consequence: a reloaded session re-derives the sequence from the seed; a mutated-in-place chain (`mutationCount`) is not restored.
- **D3 — generation params gap.** Rhythm mode, pulses, rotation and step probability are not in that ValueTree today, so session reload rebuilds with defaults. Design should decide whether to add a `generation` child under the same versioned root (schema-compatible) or accept the gap.
- **D4 — one `.jucer`, two targets.** Add `audioplug` export to the existing `Berlin.jucer` rather than forking a second project, so module and source lists cannot drift.

**Testing under Strict TDD.** `BerlinAudioProcessor` is constructible without an audio device, and `juce_audio_processors_headless` is already a declared module, so the currently-untested glue (`regenerate`, `mutate`, `savePreset`, preset wiring) becomes reachable from the existing console test target: drive `prepareToPlay` / `processBlock` over an allocated buffer pair across several block sizes and sample rates, assert MIDI events land in the host buffer at the expected sample offsets, assert deterministic regenerate/mutate results, and assert `getStateInformation`/`setStateInformation` round-trips. Tests are written before the extraction lands. The editor stays GUI-manual; the point of the split is that almost nothing untestable remains behind it.

## Affected Areas

| Area | Impact | Description |
|---|---|---|
| `Source/plugin/BerlinAudioProcessor.{h,cpp}` | New | Engine owner, `processBlock`, state recall |
| `Source/plugin/BerlinAudioProcessorEditor.{h,cpp}` | New | Widgets and layout moved from `MainComponent` |
| `Source/MainComponent.{h,cpp}` | Modified | Reduced to a standalone shell; keeps `MidiOutputSink` |
| `Berlin.jucer` | Modified | Plugin export target (VST3 + AU), plugin codes/manufacturer |
| `Source/midi/MidiOutputSink.*` | Unchanged | Standalone-only; excluded from the plugin path |
| `Source/playback/*`, `Source/synth/*`, `Source/generation/*`, `Source/preset/*` | Unchanged | Reused as-is |
| `Tests/Source/BerlinAudioProcessorTests.cpp` | New | Headless processor coverage |

## Risks

| Risk | Likelihood | Mitigation |
|---|---|---|
| Diff far exceeds the 800-line review budget (~40 widgets move) | High | Chain PRs: (1) processor + tests, (2) editor extraction, (3) `.jucer` plugin target. `sdd-tasks` must forecast this explicitly |
| Untested glue regresses during the move | High | Processor tests written first; `MainComponent` keeps identical observable behaviour |
| Projucer plugin config errors (bus layout, `isMidiEffect`, plugin codes) surface only in a host | Medium | Validate with pluginval / a real DAW as an explicit acceptance gate |
| `MutationEngine` allocates; if mutation is ever triggered from `processBlock` it becomes an RT violation | Low | Keep every mutate/regenerate trigger on the message thread; assert this in review |
| Session recall loses generation params (D3) | Medium | Resolve in design before spec freeze |

## Rollback

The plugin target is additive. Revert the `Berlin.jucer` plugin export and delete `Source/plugin/` to drop the plugin. Reverting the `MainComponent` reduction restores today's standalone behaviour; it is a single commit boundary and the existing 223-test suite gates it.

## Dependencies

- Projucer with VST3 SDK available; AU builds require macOS (Windows CI validates VST3 only).
- `juce_audio_processors` / `juce_audio_processors_headless` already declared in `Berlin.jucer` — confirm they were pre-staged intentionally.

## Success Criteria

- [ ] VST3 loads in a DAW, plays the generated sequence through the internal synth, and emits MIDI on its plugin MIDI output.
- [ ] All existing 223 tests pass, plus new headless `BerlinAudioProcessor` tests.
- [ ] Standalone binary behaves identically to today, including OS MIDI device output.
- [ ] Closing and reopening the plugin editor loses no state; auto-evolve keeps running with the editor closed.
- [ ] Saving and reloading a DAW session restores patch and seed.
- [ ] `processBlock` contains no allocation, lock, or logging on its per-block path.
