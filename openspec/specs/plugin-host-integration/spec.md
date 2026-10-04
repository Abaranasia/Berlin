# Plugin Host Integration Specification

## Purpose

Defines the `BerlinAudioProcessor`/`BerlinAudioProcessorEditor` pair that lets Berlin run as a VST3/AU plugin: engine ownership, the `processBlock` contract reusing `SequencePlayer`/`SynthEngine` unchanged, MIDI emission into the host's buffer instead of an OS device, editor attach/detach independent of engine lifetime, and the standalone shell that keeps today's behavior.

## Requirements

### Requirement: Processor Owns All Engine State Independent Of The Editor

The system MUST have `BerlinAudioProcessor` own `SequencePlayer`, `Transport`, `SynthEngine`, `MidiEventTranslator`, `PresetManager`, the current sequence/seed/mutation state, generation parameters, and the auto-evolve `Timer`. The auto-evolve `Timer` MUST keep running whether or not a `BerlinAudioProcessorEditor` is currently attached.

#### Scenario: Auto-evolve continues with no editor open

- GIVEN a `BerlinAudioProcessor` running with auto-evolve enabled
- WHEN its editor window is closed
- THEN the auto-evolve `Timer` keeps firing and mutating the live sequence

#### Scenario: Reopened editor reflects current engine state

- GIVEN the processor mutated its sequence while no editor was attached
- WHEN a new editor is created and attached
- THEN it displays the processor's current sequence/mutation state, not a stale or reset one

### Requirement: processBlock Reuses SequencePlayer And SynthEngine Unchanged

`processBlock` MUST call `buffer.clear()`, then `SequencePlayer::process` and `SynthEngine::render(startSample = 0, numSamples = buffer.getNumSamples())` exactly as `getNextAudioBlock` does today, with no allocation, lock, or logging call anywhere in its path.

#### Scenario: Block renders through the same engine path as standalone

- GIVEN a prepared `BerlinAudioProcessor` and an allocated audio buffer
- WHEN `processBlock` is called
- THEN the rendered audio matches what `SequencePlayer::process` + `SynthEngine::render` would produce for the same sequence/transport state in the standalone path

#### Scenario: No allocation or lock in processBlock

- GIVEN the implementation of `processBlock`
- WHEN its call path is inspected during code review
- THEN no heap allocation, logging call, or lock acquisition is present anywhere in the path

### Requirement: Plugin MIDI Output Goes Through The Host MidiBuffer Only

The system MUST write `MidiEventTranslator` output for the current block into `processBlock`'s `juce::MidiBuffer&` parameter. The system MUST NOT open, write to, or close any OS `juce::MidiOutput` device from the plugin path. The system MUST NOT read, forward, or otherwise act on incoming host MIDI input; the input buffer content is ignored. The system MUST declare a MIDI input bus (`acceptsMidi()` returns `true`) so hosts recognize it as a spec-compliant `Instrument`-category VST3/AU plugin — the VST3 SDK requires an event input bus for that category — but this bus's existence does not change the ignore-content rule above.

#### Scenario: Step events land in the host buffer at their sample offsets

- GIVEN a block containing scheduled `StepEvent`s
- WHEN `processBlock` runs
- THEN each event appears as a MIDI message in the host `MidiBuffer` at its `sampleOffset`, with no OS MIDI device opened

#### Scenario: Incoming host MIDI is ignored

- GIVEN the host passes note-on/note-off messages in `processBlock`'s input `MidiBuffer`
- WHEN the block is processed
- THEN those messages have no effect on sequence playback, mutation, or output — Berlin remains a pure generator

#### Scenario: Plugin declares a MIDI input bus despite ignoring its content

- GIVEN the plugin is loaded in a VST3 host that enforces the `Instrument` category's event-input-bus requirement (e.g. Ableton Live)
- WHEN the host scans/instantiates the plugin
- THEN a valid MIDI/event input bus is found and the plugin loads successfully, even though its content is never acted upon

### Requirement: Editor Attach/Detach Does Not Affect Engine State

Creating or destroying an editor MUST NOT alter the processor's sequence, transport position, mutation state, or auto-evolve timer. This applies to both compile-time-selected editors: `BerlinAudioProcessorEditor` (flag off) and `WebEditor` (`BERLIN_WEB_UI` on). Widget and native-function callbacks MUST forward to the processor/`UiBridge` rather than holding independent state.

#### Scenario: Closing and reopening the editor loses no state

- GIVEN a processor mid-playback with an attached editor of either kind
- WHEN the editor is destroyed and a new one is created
- THEN playback continues uninterrupted and the new editor shows the same live state

#### Scenario: WebEditor teardown stops its timer

- GIVEN a `WebEditor` is attached
- WHEN it is destroyed
- THEN its playhead timer stops and the engine and auto-evolve timer are unaffected

### Requirement: Standalone Shell Preserves Today's Observable Behavior

`MainComponent` MUST be an `AudioAppComponent` shell owning one processor and one editor, held as `std::unique_ptr<juce::AudioProcessorEditor>`, forwarding `prepareToPlay`/`getNextAudioBlock`/`releaseResources`, and MUST keep feeding `MidiOutputSink` for OS MIDI output. `MainComponent` MUST size itself to its editor on both paths. With `BERLIN_WEB_UI` off, audio, MIDI and UI behavior MUST be unchanged and the window MUST be 800x952, fixed, showing the full legacy editor. With it on, the window MUST be resizable within the `WebEditor` limits (min 720x520, max 1920x1200). `MainWindow` resize limits MUST follow the content component's editor.

#### Scenario: Flag off, full legacy editor [M]

- GIVEN the standalone app built with the flag off
- WHEN launched
- THEN audio, OS MIDI and UI behave as before and the window is 800x952, non-resizable, with no clipping

#### Scenario: Flag off, plugin editor unchanged [M]

- GIVEN the flag-off plugin build
- WHEN the editor opens in a host
- THEN its size is as before this change and it is non-resizable

#### Scenario: Flag on standalone shell (MANUAL)

- GIVEN the standalone app built with the flag on
- WHEN launched and resized
- THEN `WebEditor` is hosted, the window stays within 720x520 to 1920x1200, and audio and MIDI output still work

#### Scenario: Content follows window [M]

- GIVEN the flag-on standalone window
- WHEN it is resized
- THEN the editor fills the content area
