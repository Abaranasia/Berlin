# Delta for plugin-host-integration

Purpose line change (non-requirement text): replace `BerlinAudioProcessor`/`BerlinAudioProcessorEditor` pair with `BerlinAudioProcessor`/`WebEditor` pair.

Scenario tags: [V] Vitest, [T] BerlinTests, [M] manual, [B] build.

## MODIFIED Requirements

### Requirement: Processor Owns All Engine State Independent Of The Editor

The system MUST have `BerlinAudioProcessor` own `SequencePlayer`, `Transport`, `SynthEngine`, `MidiEventTranslator`, `PresetManager`, the current sequence/seed/mutation state, generation parameters, and the auto-evolve `Timer`. The auto-evolve `Timer` MUST keep running whether or not a `WebEditor` is currently attached.

(Previously: "a `BerlinAudioProcessorEditor` is currently attached".)

#### Scenario: Auto-evolve continues with no editor open [M]

- GIVEN a `BerlinAudioProcessor` running with auto-evolve enabled
- WHEN its editor window is closed
- THEN the auto-evolve `Timer` keeps firing and mutating the live sequence

#### Scenario: Reopened editor reflects current engine state [M]

- GIVEN the processor mutated its sequence while no editor was attached
- WHEN a new editor is created and attached
- THEN it displays the processor's current sequence/mutation state, not a stale or reset one

### Requirement: Editor Attach/Detach Does Not Affect Engine State

Creating or destroying the editor MUST NOT alter the processor's sequence, transport position, mutation state, or auto-evolve timer. The editor is always a `WebEditor`, created by `createBerlinEditor`, which the standalone calls with its `StandaloneHost` and the plugin calls with none. Native-function callbacks MUST forward to the processor/`UiBridge` rather than holding independent state.

(Previously: applied to both compile-time-selected editors, `BerlinAudioProcessorEditor` (flag off) and `WebEditor` (`BERLIN_WEB_UI` on); "Widget and native-function callbacks".)

#### Scenario: Closing and reopening the editor loses no state [M]

- GIVEN a processor mid-playback with an attached `WebEditor`
- WHEN the editor is destroyed and a new one is created
- THEN playback continues uninterrupted and the new editor shows the same live state

#### Scenario: WebEditor teardown stops its timer [M]

- GIVEN a `WebEditor` is attached
- WHEN it is destroyed
- THEN its playhead timer stops and the engine and auto-evolve timer are unaffected

#### Scenario: Plugin editor has no host settings [M]

- GIVEN the plugin editor created with no `StandaloneHost`
- WHEN it opens in Cakewalk Sonar
- THEN it is the resizable `WebEditor` and the Settings controls are not available

### Requirement: Standalone Shell Preserves Today's Observable Behavior

`MainComponent` MUST be an `AudioAppComponent` shell owning one processor and one editor, held as `std::unique_ptr<juce::AudioProcessorEditor>`, forwarding `prepareToPlay`/`getNextAudioBlock`/`releaseResources`, and MUST keep feeding `MidiOutputSink` for OS MIDI output. `MainComponent` MUST size itself to its editor. The editor is always the `WebEditor`, so the standalone window MUST always be resizable: initial size 960x680, minimum 720x520, maximum 1920x1200, with the native frame included. `MainWindow` resize limits MUST always follow the content component's editor; there MUST be no fixed-size window path. Audio and OS MIDI output behavior MUST be unchanged.

(Previously: with `BERLIN_WEB_UI` off, the window was 800x952, fixed, showing the full legacy editor; with it on, resizable within the `WebEditor` limits. Both branches are replaced by the single resizable behavior.)

#### Scenario: Standalone window resizable [M]

- GIVEN the standalone app
- WHEN launched and resized
- THEN `WebEditor` is hosted, the initial size is 960x680, the window stays within 720x520 to 1920x1200 including the native frame, and audio and MIDI output still work

#### Scenario: No fixed-size window [M]

- GIVEN the standalone app
- WHEN the user drags the window edge
- THEN the window resizes; it is never locked to 800x952

#### Scenario: Content follows window [M]

- GIVEN the standalone window
- WHEN it is resized
- THEN the editor fills the content area

## REMOVED Requirements

(No whole requirements are removed. The flag-off scenarios "Flag off, full legacy editor", "Flag off, plugin editor unchanged" and "Flag on standalone shell" are removed together with their flag-off behavior and replaced by the scenarios above.)
