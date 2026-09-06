# Internal Synth Voice Specification

## Purpose

A single monophonic voice — oscillator, low-pass filter with resonance, ADSR envelope, and one LFO — rendering one note at a time, with every parameter (waveform, pulse width, filter cutoff/resonance, ADSR, LFO destination/rate/depth) live user-adjustable while a note sounds. Built on JUCE's built-in DSP/audio primitives (`juce::dsp` and `juce_audio_basics` per what each stage needs — see `design.md`), confined to `Source/synth/`, as a documented, scoped exception to this project's JUCE-free-core convention. Parameter changes apply at the existing control-rate cadence via message-thread setters and lock-free atomics; no preset save/load yet.

## Requirements

### Requirement: Four Selectable Oscillator Waveforms

The system MUST render the voice's oscillator as one of four periodic waveforms — sawtooth, square, pulse, or triangle — selectable live by the user, including while a note is sounding. Waveform generation MUST be table-free (no lookup-table allocation) so live switching allocates nothing. Naive generation is accepted; aliasing is not a defect. Table-free rendering MAY differ slightly in audible character from a lookup-table rendering of the same waveform, provided it remains audibly equivalent. The pulse waveform's duty cycle (pulse width) MUST be exposed as its own live user-adjustable control, clamped to `[0.05, 0.95]` — without it, pulse at its default 50% duty is indistinguishable from square, making it a redundant menu entry.

#### Scenario: Each waveform produces its characteristic periodic shape
- GIVEN the patch selects one of the four waveforms
- WHEN a note sounds at a given pitch
- THEN the rendered signal is periodic at the fundamental, audibly distinct per waveform

#### Scenario: All four waveforms are demonstrable
- GIVEN the default patch
- WHEN each waveform option is exercised in turn
- THEN each produces audible, distinguishable output with no missing or silent option

#### Scenario: Waveform switch mid-note allocates nothing and produces no dropout
- GIVEN a note is sounding with one waveform selected
- WHEN the user selects a different waveform while the note continues
- THEN the new waveform renders within one control block, no heap allocation occurs, and no dropout or glitch is heard

#### Scenario: Live pulse width change reshapes the pulse waveform
- GIVEN a note is sounding on the pulse waveform
- WHEN the user changes pulse width within its documented range
- THEN the duty cycle audibly changes immediately, without waiting for the next note-on, and remains distinct from the square waveform except at the range's extremes

#### Scenario: Table-free rendering stays audibly equivalent to the prior table-based rendering
- GIVEN the same waveform, pitch, and patch used by the Phase 8 lookup-table implementation
- WHEN the table-free renderer produces output instead
- THEN the two are audibly equivalent, confirmed by an automated before/after comparison test and a manual listen

### Requirement: Low-Pass Filter With Resonance

The system MUST apply a low-pass filter with configurable cutoff and resonance. Both MUST be live user-adjustable at any time, including mid-note, applied at the existing control-rate cadence, clamped to a documented range (`cutoffHz` in `[20, 20000]` Hz; `resonance` in `[0.7071068, 8.0]`). Cutoff's range excludes silence (too low) and invalid/Nyquist-adjacent values (too high). Resonance's range bounds peak gain to a finite, non-clipping-guaranteed-safe ceiling; the filter itself is unconditionally stable and cannot diverge or self-oscillate at any resonance value, so the cap is a loudness/clipping choice, not a stability requirement.

#### Scenario: Cutoff sweep changes brightness
- GIVEN a sounding note
- WHEN cutoff is swept low to high
- THEN audible brightness increases correspondingly, with no dropout

#### Scenario: Resonance emphasizes the cutoff frequency
- GIVEN a sounding note with resonance above its minimum
- WHEN the signal is rendered
- THEN energy is audibly emphasized near cutoff relative to zero resonance

#### Scenario: Live cutoff/resonance change while a note sounds
- GIVEN a note is sounding
- WHEN the user changes cutoff or resonance
- THEN the audible change is heard immediately, without waiting for the next note-on

### Requirement: ADSR-Gated Amplitude

The system MUST shape output amplitude through attack, decay, sustain, and release, gated by note-on/off. Note-on MUST (re)trigger attack; note-off MUST trigger release; once release completes with no subsequent note-on, the voice MUST be silent. A/D/S/R MUST be live user-adjustable at any time, including mid-note, applied at the existing control-rate cadence, with attack/decay/release each clamped to a documented range (`attack`/`decay` in `[0.001, 4.0]` s, `release` in `[0.005, 8.0]` s; `sustain` in `[0.0, 1.0]`) whose lower floor is strictly greater than zero — a zero-length stage is a well-defined, deliberately reachable instant transition (see scenario below), not an unclamped edge case. Changing a value while a stage is in progress MUST immediately retarget that stage to the new value(s) — an audible jump is expected, not a defect.

#### Scenario: Note-on triggers attack through sustain
- GIVEN the voice is idle
- WHEN a note-on is received
- THEN amplitude rises through attack and decay into sustain and stays there while held

#### Scenario: Note-off triggers release to silence
- GIVEN a note is sounding at sustain
- WHEN a note-off is received and no further note-on follows
- THEN amplitude falls through release to silence and stays silent

#### Scenario: Live ADSR change mid-stage snaps to the new trajectory
- GIVEN a note is in attack, decay, or release
- WHEN the user changes one or more ADSR values
- THEN the envelope immediately follows the new timing/level from its current amplitude; the resulting jump is expected

#### Scenario: Setting sustain to zero during release is an instant cut, not a defect
- GIVEN a note is in its release stage
- WHEN the user sets sustain to its minimum
- THEN the voice may fall silent immediately as a deliberate consequence of the user's own action, not an unintended click or bug

### Requirement: Single LFO With Selectable Destination

The system MUST provide one LFO modulating exactly one destination at a time — pitch, filter cutoff, amplitude, or pulse width — continuously while a note sounds. Destination, rate, and depth MUST be live user-adjustable at any time, including mid-note, applied at the existing control-rate cadence. Rate MUST be clamped to `[0.05, 20.0]` Hz, excluding negative/reversed-phase behavior.

#### Scenario: LFO modulates the configured destination
- GIVEN the patch routes the LFO to one of the four destinations
- WHEN a note sounds longer than one LFO cycle
- THEN the destination parameter audibly varies periodically at the LFO's rate

#### Scenario: Each of the four destinations is demonstrable
- GIVEN the patch
- WHEN the LFO destination is set to each of pitch, cutoff, amplitude, and pulse width in turn
- THEN each produces an audibly distinct modulation effect

#### Scenario: Switching destination mid-note leaves no parameter parked at a stale modulated value
- GIVEN a note is sounding and the LFO modulates destination A away from its base value
- WHEN the user switches the LFO destination to B while the note continues
- THEN destination A is re-applied to its base value on the next control block, and only B receives the LFO's delta thereafter — A MUST NOT remain parked at its last modulated value

### Requirement: Strictly Monophonic, No Voice Pool Or Stealing

The system MUST sound at most one note at a time. The system MUST NOT implement a voice pool or a voice-stealing policy. A note-on received while another note is already sounding MUST immediately replace the currently sounding note; the previous note MUST NOT continue to sound alongside the new one.

#### Scenario: Overlapping note-on replaces the sounding note
- GIVEN a note is currently sounding
- WHEN a new note-on is received before that note's note-off
- THEN the previously sounding note stops and only the new note sounds

#### Scenario: No second voice is ever created
- GIVEN any sequence of overlapping note-on/note-off events
- WHEN the events are processed
- THEN at most one note is ever audible at once, with no independent second voice

### Requirement: Allocation-Free Voice Rendering

The system MUST NOT allocate heap memory when rendering a block; all fixed-size preparation MUST occur off the audio thread before rendering. Applying any live parameter change (waveform, cutoff, resonance, ADSR, LFO destination/rate/depth) at control rate MUST also allocate no heap memory, acquire no lock, and call no logging/formatting function; values MUST cross message-thread-to-audio-thread only via lock-free atomics.

#### Scenario: Rendering a block performs no allocation
- GIVEN the voice has been prepared for a given sample rate and block size
- WHEN a block is rendered, at any note-on/off density
- THEN no heap allocation occurs during rendering

#### Scenario: Live parameter application, including waveform switch, allocates nothing
- GIVEN the voice is rendering a sounding note
- WHEN any live parameter is changed via its message-thread setter and applied at the next control block
- THEN no heap allocation, lock acquisition, or logging/formatting call occurs anywhere in the application path

### Requirement: Silence When Idle

The system MUST produce silence (zero-valued samples) when no note has sounded since the last full envelope release.

#### Scenario: Freshly prepared voice is silent
- GIVEN the voice has just been prepared and no note-on has yet occurred
- WHEN a block is rendered
- THEN every rendered sample is silence
