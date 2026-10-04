# Delta for Plugin Host Integration

## MODIFIED Requirements

### Requirement: Standalone Shell Preserves Today's Observable Behavior

`MainComponent` MUST be an `AudioAppComponent` shell owning one processor and one editor, held as `std::unique_ptr<juce::AudioProcessorEditor>`, forwarding `prepareToPlay`/`getNextAudioBlock`/`releaseResources`, and MUST keep feeding `MidiOutputSink` for OS MIDI output. `MainComponent` MUST size itself to its editor on both paths. With `BERLIN_WEB_UI` off, audio, MIDI and UI behavior MUST be unchanged and the window MUST be 800x952, fixed, showing the full legacy editor. With it on, the window MUST be resizable within the `WebEditor` limits (min 720x520, max 1920x1200). `MainWindow` resize limits MUST follow the content component's editor.
(Previously: flag off required observable behavior and window size unchanged, i.e. 800x680 clipping the legacy editor.)

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
