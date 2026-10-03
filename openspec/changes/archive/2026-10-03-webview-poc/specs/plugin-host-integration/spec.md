# Delta for Plugin Host Integration

## MODIFIED Requirements

### Requirement: Editor Attach/Detach Does Not Affect Engine State

Creating or destroying an editor MUST NOT alter the processor's sequence, transport position, mutation state, or auto-evolve timer. This applies to both compile-time-selected editors: `BerlinAudioProcessorEditor` (flag off) and `WebEditor` (`BERLIN_WEB_UI` on). Widget and native-function callbacks MUST forward to the processor/`UiBridge` rather than holding independent state.

(Previously: applied only to `BerlinAudioProcessorEditor`.)

#### Scenario: Closing and reopening the editor loses no state
- GIVEN a processor mid-playback with an attached editor of either kind
- WHEN the editor is destroyed and a new one is created
- THEN playback continues uninterrupted and the new editor shows the same live state

#### Scenario: WebEditor teardown stops its timer
- GIVEN a `WebEditor` is attached
- WHEN it is destroyed
- THEN its playhead timer stops and the engine and auto-evolve timer are unaffected

### Requirement: Standalone Shell Preserves Today's Observable Behavior

`MainComponent` MUST be an `AudioAppComponent` shell owning one processor and one editor, held as `std::unique_ptr<juce::Component>`, forwarding `prepareToPlay`/`getNextAudioBlock`/`releaseResources`, and MUST keep feeding `MidiOutputSink` for OS MIDI output. With `BERLIN_WEB_UI` off, observable behavior and window size MUST be unchanged.

(Previously: owned a concrete editor member.)

#### Scenario: Flag off is indistinguishable
- GIVEN the standalone app built with the flag off
- WHEN launched and run
- THEN audio, OS MIDI output, UI, and window size match the pre-change behavior

#### Scenario: Flag on standalone shell (MANUAL)
- GIVEN the standalone app built with the flag on
- WHEN launched
- THEN `WebEditor` is hosted in the shell and audio and MIDI output still work
