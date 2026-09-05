# Delta for Realtime Audio Wiring

## MODIFIED Requirements

### Requirement: Audio Output Governed By Synth Enable State

The system MUST produce audible audio output in `getNextAudioBlock` when the internal synth is enabled and a note is sounding: the internal synth (oscillator, filter, envelope, LFO, and optional delay/reverb) renders into the audio output buffer. When the internal synth is disabled, or no note is sounding, the audio output buffer MUST contain only silence (zero samples) — no oscillator, filter, envelope, or voice outside the internal synth path may introduce sound. This requirement governs the audio output path only; the callback continues to emit MIDI output as a distinct dispatch path.
(Previously: no oscillator, filter, envelope, or voice was permitted anywhere, and the audio buffer was required to stay cleared unconditionally.)

#### Scenario: Audio buffer carries synth output when enabled and a note sounds
- GIVEN the internal synth is enabled and a `StepEvent` note-on has been dispatched
- WHEN the block is rendered
- THEN the audio output buffer contains the synth's rendered signal, not silence

#### Scenario: Audio buffer stays silent when the synth is disabled
- GIVEN the internal synth enable toggle is off
- WHEN `getNextAudioBlock` processes a block, regardless of any `StepEvent` note-on
- THEN the audio output buffer remains at its cleared (silent) value

#### Scenario: Audio buffer stays silent when no note is sounding
- GIVEN the internal synth is enabled but no note-on has occurred since the last full envelope release
- WHEN a block is rendered
- THEN the audio output buffer remains silent

#### Scenario: MIDI output does not violate audio silence when the synth is disabled
- GIVEN a block in which MIDI events are dispatched to an output device and the internal synth is disabled
- WHEN the audio output buffer contents are inspected
- THEN the audio buffer remains cleared, because MIDI dispatch is a distinct output path from the audio buffer

### Requirement: Standalone App Still Launches And Runs Without Dropouts

The system MUST still allow the standalone app to launch, open its audio device, and run the transport continuously without dropouts or asserts, whether or not the internal synth is currently producing audible output. This is a manual smoke gate, not automatable in the headless test harness for the audio-device-touching path.
(Previously: the scenario asserted the app runs "silently"; Phase 8 makes audible synth output the expected default state, so the gate now asserts absence of dropouts/asserts/crashes, not absence of sound.)

#### Scenario: Manual launch gate passes with the synth producing audible sound
- GIVEN the built standalone app with the internal synth enabled by default
- WHEN it is launched manually
- THEN it opens its audio device and runs the transport continuously with no dropouts, asserts, or crashes, until manually closed, whether or not sound is currently audible

#### Scenario: Manual launch gate passes with the synth disabled
- GIVEN the built standalone app with the internal synth toggled off
- WHEN it is launched manually
- THEN it opens its audio device and runs the transport continuously, silently, with no dropouts, asserts, or crashes, until manually closed
