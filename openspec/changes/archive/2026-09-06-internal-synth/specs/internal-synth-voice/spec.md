# Internal Synth Voice Specification

## Purpose

A single monophonic voice — oscillator, low-pass filter with resonance, ADSR envelope, and one LFO — rendering the fixed default patch's sound for one note at a time. Built on JUCE's built-in DSP/audio primitives (`juce::dsp` and `juce_audio_basics`, per what each stage actually needs — see `design.md` for the exact class mapping), confined to `Source/synth/`, as a documented, scoped exception to this project's JUCE-free-core convention. No parameter UI or patch system; the patch is fixed for this phase.

## Requirements

### Requirement: Four Selectable Oscillator Waveforms

The system MUST render the voice's oscillator as one of four periodic waveforms — sawtooth, square, pulse, or triangle — selected by the fixed patch configuration. Naive (non-band-limited) waveform generation is accepted as-is; aliasing artefacts are not a defect.

#### Scenario: Each waveform produces its characteristic periodic shape
- GIVEN the fixed patch selects one of the four waveforms
- WHEN a note sounds at a given pitch
- THEN the rendered signal is periodic at that pitch's fundamental frequency, audibly distinct per waveform

#### Scenario: All four waveforms are demonstrable from the fixed patch
- GIVEN the fixed default patch
- WHEN each of the four waveform options is exercised in turn
- THEN each produces audible, distinguishable output with no missing or silent option

### Requirement: Low-Pass Filter With Resonance

The system MUST apply a low-pass filter with configurable cutoff frequency and resonance to the voice's signal path.

#### Scenario: Cutoff sweep changes brightness
- GIVEN a sounding note
- WHEN the filter cutoff is swept from low to high
- THEN the rendered signal's audible brightness increases correspondingly, with no dropout

#### Scenario: Resonance emphasizes the cutoff frequency
- GIVEN a sounding note with resonance set above its minimum
- WHEN the signal is rendered
- THEN energy is audibly emphasized near the cutoff frequency relative to zero resonance

### Requirement: ADSR-Gated Amplitude

The system MUST shape the voice's output amplitude through attack, decay, sustain, and release stages, gated by note-on/note-off. A note-on MUST (re)trigger the attack stage; a note-off MUST trigger the release stage; once release completes with no subsequent note-on, the voice MUST produce silence.

#### Scenario: Note-on triggers attack through sustain
- GIVEN the voice is idle
- WHEN a note-on is received
- THEN amplitude rises through attack and decay into the sustain level and stays there while held

#### Scenario: Note-off triggers release to silence
- GIVEN a note is sounding at the sustain level
- WHEN a note-off is received and no further note-on follows
- THEN amplitude falls through the release stage to silence and stays silent thereafter

### Requirement: Single LFO With Selectable Destination

The system MUST provide one LFO capable of modulating exactly one destination per fixed patch — pitch, filter cutoff, amplitude, or pulse width — continuously while a note is sounding.

#### Scenario: LFO modulates the configured destination
- GIVEN the fixed patch routes the LFO to one of the four destinations
- WHEN a note sounds for longer than one LFO cycle
- THEN the destination parameter audibly varies periodically at the LFO's rate

#### Scenario: Each of the four destinations is demonstrable
- GIVEN the fixed patch
- WHEN the LFO destination is set to each of pitch, filter cutoff, amplitude, and pulse width in turn
- THEN each produces an audibly distinct modulation effect

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

The system MUST NOT allocate heap memory when rendering the voice for a block. All fixed-size preparation (oscillator, filter, envelope, and LFO resource sizing) MUST occur before rendering begins, off the audio thread.

#### Scenario: Rendering a block performs no allocation
- GIVEN the voice has been prepared for a given sample rate and block size
- WHEN a block is rendered, at any note-on/off density
- THEN no heap allocation occurs during rendering

### Requirement: Silence When Idle

The system MUST produce silence (zero-valued samples) when no note has sounded since the last full envelope release.

#### Scenario: Freshly prepared voice is silent
- GIVEN the voice has just been prepared and no note-on has yet occurred
- WHEN a block is rendered
- THEN every rendered sample is silence
