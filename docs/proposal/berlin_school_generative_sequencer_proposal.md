# Berlin School Generative Sequencer — Proposal & AI Development Plan

## 1. Project Overview

### Working title

**Berlin School Generative Sequencer**

### Vision

Build a cross-platform application for creating, generating, evolving, performing, and exporting Berlin School–inspired musical sequences.

The application should make generative sequencing approachable for users who do not want to program every note manually, while still exposing enough detailed controls for advanced users.

The core idea is:

> Generate musically coherent, hypnotic, evolving sequences rather than purely random note patterns.

The application should support:

- Automatic sequence generation.
- Controlled randomization.
- Manual parameter tweaking.
- Real-time playback.
- MIDI input/output.
- MIDI clock synchronization.
- MIDI pattern export.
- Internal sound generation for immediate auditioning.
- Presets.
- Eventually VST3/AU plugin versions.
- Potential hardware synthesizer integration.
- Multiple simultaneous sequences.
- Evolving/generative behavior over time.

The sequencing engine must be independent from the audio engine and user interface.

---

# 2. Product Philosophy

The application should sit between a traditional step sequencer and a generative music system.

A conventional sequencer asks the user to define almost everything:

```text
Step 1 → C
Step 2 → E
Step 3 → G
Step 4 → B
...
```

A purely random generator produces results that may technically be different but are often musically useless.

This project should instead operate at a higher musical level:

```text
Musical intention
        ↓
Generation rules
        ↓
Sequence
        ↓
Controlled evolution
        ↓
MIDI / Internal synth / External synth
```

The user should be able to say things like:

- "Make it more hypnotic."
- "Make it more sparse."
- "Increase movement."
- "Make it darker."
- "Add variation."
- "Keep the motif but change the rhythm."
- "Transpose every 8 bars."
- "Make it more classic Berlin School."
- "Give me something unexpected."

The system translates those high-level intentions into low-level sequencing parameters.

---

# 3. Recommended Technology Stack

## Primary recommendation

### Language

**C++**

Reasons:

- Excellent real-time audio performance.
- Precise MIDI timing.
- Mature audio ecosystem.
- Cross-platform.
- Suitable for DAW plugins.
- Large ecosystem of DSP libraries.
- Low latency.
- JUCE integration.

## Framework

**JUCE**

JUCE should provide:

- GUI.
- Audio processing.
- MIDI input.
- MIDI output.
- MIDI device management.
- Audio device management.
- Plugin hosting/integration where appropriate.
- VST3 support.
- Audio Unit support.
- Cross-platform application infrastructure.
- Timers and synchronization infrastructure.
- Parameter management.

## Build system

**CMake**

The project should be CMake-based from the beginning.

## IDE

Recommended:

- CLion
- Visual Studio Code
- Visual Studio on Windows
- Xcode on macOS

The project should not depend on one IDE.

## Version control

**Git**

Use a repository with:

- `main`
- feature branches
- tagged releases
- automated builds eventually

---

# 4. Alternative Technology Stack

A secondary possibility is:

- Rust
- egui
- cpal
- midir

Rust has advantages in safety and modern language design.

However, for this particular project, C++ + JUCE is the preferred option because:

- Audio tooling is more mature.
- Plugin development is easier.
- MIDI/audio APIs are mature.
- Cross-platform music software development is well established.
- There is a large community around JUCE.

Python can be useful for prototyping algorithms, data analysis, and experiments, but it should not be the primary production technology for real-time audio/MIDI.

---

# 5. High-Level Architecture

The architecture should look approximately like this:

```text
                    USER INTERFACE
                         │
                         ▼
                 PARAMETER MODEL
                         │
                         ▼
                SEQUENCING ENGINE
                         │
       ┌─────────────────┼─────────────────┐
       │                 │                 │
       ▼                 ▼                 ▼
    MIDI OUT          MIDI EXPORT      INTERNAL SYNTH
       │                 │                 │
       ▼                 ▼                 ▼
 Hardware Synth       .mid file          Audio Output
```

The critical architectural principle is:

> The sequencing engine must not know anything about the GUI or audio synthesis.

For example, the sequence engine should be able to generate:

```text
Sequence
 ├── Note
 ├── Start time
 ├── Duration
 ├── Velocity
 ├── Channel
 ├── Probability
 └── Metadata
```

The same sequence can then be:

- Played through the internal synthesizer.
- Sent to a hardware synthesizer.
- Sent to a DAW.
- Exported as MIDI.
- Displayed in a piano roll.

---

# 6. Core Domain Model

The most important objects should be conceptually separated.

## Sequence

Represents a musical sequence.

Possible properties:

- Number of steps.
- Tempo.
- Length in bars.
- Time signature.
- Scale.
- Root note.
- Notes.
- Gates.
- Velocities.
- Octaves.
- Timing offsets.
- Probability.
- Accent information.

Example:

```text
Sequence
    tempo = 128
    length = 16 steps
    scale = C minor
    root = C
    swing = 0.12

    steps:
        0 → C3
        1 → G3
        2 → Eb3
        3 → C4
        ...
```

## Step

A step may contain:

```text
note
velocity
gate
duration
probability
timingOffset
accent
active
```

## Pattern

A pattern is a reusable sequence or motif.

It should eventually support:

- Copying.
- Transformation.
- Mutation.
- Transposition.
- Reversal.
- Rotation.
- Inversion.

---

# 7. Musical Generation Pipeline

The generation system should be modular.

Suggested pipeline:

```text
User Parameters
       ↓
Scale / Harmony Generator
       ↓
Pitch Pattern Generator
       ↓
Rhythm Generator
       ↓
Octave Generator
       ↓
Velocity Generator
       ↓
Gate Generator
       ↓
Probability Engine
       ↓
Mutation Engine
       ↓
Sequence
```

Each module should be independently testable.

---

# 8. Pitch Generation

The pitch generator should not simply choose random MIDI notes.

It should understand musical relationships.

Possible strategies:

## Scale-based

Inputs:

- Root.
- Scale.
- Number of notes.
- Range.

Examples:

- Minor.
- Major.
- Dorian.
- Phrygian.
- Harmonic minor.
- Pentatonic.
- User-defined scales.

## Interval-based

Generate sequences using preferred intervals:

- Unison.
- Minor second.
- Major second.
- Minor third.
- Major third.
- Fourth.
- Fifth.
- Sixth.
- Octave.

Allow weighted probabilities.

Example:

```text
Fifth: 30%
Octave: 20%
Minor third: 20%
Second: 20%
Other: 10%
```

## Motif-based

Generate a small motif:

```text
C - G - Eb - G
```

Then transform it.

Possible transformations:

- Transpose.
- Invert.
- Reverse.
- Rotate.
- Expand.
- Contract.
- Add octave displacement.

This is likely to produce more coherent Berlin School material than independent random note selection.

---

# 9. Rhythm Generation

Rhythm should be independent from pitch.

Possible rhythm systems:

## Step-based

```text
1 0 1 0 1 0 1 0
```

## Euclidean rhythms

Allow parameters such as:

```text
steps = 16
pulses = 5
rotation = 2
```

## Probability-based rhythm

Each step has activation probability:

```text
step probability = 70%
```

## Density

Expose a simple control:

```text
Density
0% ─────────── 100%
```

Internally this modifies the rhythm generator.

## Polyrhythm

Eventually support independent lengths:

```text
Sequence A = 16 steps
Sequence B = 13 steps
Sequence C = 7 steps
```

This can produce long evolving cycles.

---

# 10. Gate Generation

Gate length should be independent of note placement.

Possible modes:

- Fixed.
- Random.
- Probability.
- Humanized.
- Short.
- Long.
- Legato.

Expose:

```text
Gate Length
0% ─────────── 100%
```

---

# 11. Velocity Generation

Velocity should also be generated independently.

Modes:

- Fixed.
- Accent pattern.
- Random within range.
- Humanized.
- Cyclic.
- Phrase-based.

Example:

```text
Velocity:
100 70 80 65 110 70 80 60
```

---

# 12. Swing and Timing

Support:

- Straight.
- Swing.
- Groove.
- Microtiming.

Parameters:

```text
Swing
0% ─────────── 100%

Humanize
0% ─────────── 100%
```

Humanization must remain controlled.

It should never destroy the rhythmic identity of the pattern.

---

# 13. Controlled Randomness

Randomness is a core feature.

However, randomness should be deterministic when desired.

Use a seed:

```text
Seed = 483920
```

This allows:

```text
Generate
      ↓
Interesting result
      ↓
Save seed
      ↓
Reproduce exact sequence
```

This is extremely useful for:

- Presets.
- Debugging.
- Sharing patterns.
- Reproducibility.
- AI-assisted generation.

The UI should have:

```text
Randomize
New Seed
Lock Seed
```

---

# 14. Mutation Engine

Mutation should modify an existing sequence rather than replacing it entirely.

Possible mutations:

- Change one note.
- Change several notes.
- Change rhythm.
- Change octave.
- Change velocity.
- Reverse.
- Rotate.
- Transpose.
- Remove notes.
- Add notes.
- Stretch.
- Compress.
- Replace motif.

Example:

```text
Original:

C G Eb G | C G Bb G

Mutation:

C G Eb G | C G C  G
```

The system should allow mutation intensity:

```text
Mutation
0% ─────────── 100%
```

---

# 15. Evolution Over Time

This is one of the most important features.

The sequence should be able to evolve while playing.

Possible parameters:

```text
Evolution Rate
Mutation Rate
Transpose Interval
Rhythm Variation
Pitch Variation
Octave Variation
Density Variation
```

Example:

```text
Bars 1–8
Original motif

Bars 9–16
Small mutation

Bars 17–24
Transpose +5

Bars 25–32
Rhythmic mutation

Bars 33–40
Return toward original
```

This allows long-form generative music.

---

# 16. High-Level Musical Controls

The UI should have high-level controls that manipulate multiple internal parameters.

Suggested controls:

## Density

Sparse ↔ Dense

Controls:

- Number of active notes.
- Rhythm density.
- Repetition.

## Repetition

Static ↔ Evolving

Controls:

- Mutation rate.
- Pattern transformations.
- Rhythm changes.

## Motion

Smooth ↔ Jumping

Controls:

- Interval sizes.
- Octave jumps.
- Pitch movement.

## Tension

Consonant ↔ Dissonant

Controls:

- Interval selection.
- Scale alterations.
- Chromatic probability.

## Complexity

Minimal ↔ Complex

Controls:

- Rhythmic complexity.
- Pattern length.
- Multiple motifs.
- Polyrhythm.

## Vintage / Berlin School character

Classic ↔ Modern

This should be treated as a macro control rather than an attempt to literally emulate a particular artist.

It can influence:

- Repetition.
- Arpeggio behavior.
- Long evolving patterns.
- Octave movement.
- Rhythmic regularity.
- Mutation frequency.

---

# 17. User Interface

The first version should prioritize usability over visual complexity.

Possible main layout:

```text
+----------------------------------------------------+
| BERLIN GENERATOR                         BPM 128    |
+----------------------------------------------------+
|                                                    |
|             PIANO ROLL / PATTERN                   |
|                                                    |
|  C4      ●       ●           ●                     |
|  B3          ●       ●                             |
|  A3              ●       ●                         |
|  G3      ●                   ●                     |
|                                                    |
+----------------------------------------------------+
| Density       [========----]                       |
| Repetition    [======------]                       |
| Motion        [=====-------]                       |
| Tension       [===---------]                       |
| Complexity    [=======-----]                       |
+----------------------------------------------------+
| Scale: C Minor    Steps: 16    Gate: 65%           |
| Swing: 12%        Seed: 483920                     |
+----------------------------------------------------+
|  GENERATE     MUTATE     RANDOMIZE     PLAY        |
+----------------------------------------------------+
```

---

# 18. Piano Roll

The piano roll should eventually allow direct editing.

Users should be able to:

- Move notes.
- Delete notes.
- Add notes.
- Change velocity.
- Change gate.
- Change probability.
- Change octave.
- Select multiple notes.
- Quantize.
- Transform selected notes.

The generated sequence should remain editable.

---

# 19. Step Sequencer View

Also provide a traditional step view:

```text
Step:       1  2  3  4  5  6  7  8

Note:       C  G  Eb G  C  G  Bb G
Velocity:  100 70 80 60 110 70 80 60
Gate:       70 60 70 60 70 60 70 60
```

---

# 20. Internal Synthesizer

The first internal synth should remain simple.

## Oscillators

Start with:

- Saw.
- Square.
- Pulse.
- Triangle.

Eventually:

- Multiple oscillators.
- Detuning.
- Sync.
- Noise.

## Filter

Start with:

- Low-pass.
- Resonance.

Eventually:

- High-pass.
- Band-pass.
- Multimode.

## Envelope

ADSR:

```text
Attack
Decay
Sustain
Release
```

## LFO

Possible destinations:

- Pitch.
- Filter cutoff.
- Amplitude.
- Pulse width.

## Effects

Start with:

- Delay.
- Reverb.

Potential future additions:

- Chorus.
- Phaser.
- Flanger.
- Tape-style delay.
- Saturation.

The internal synth is primarily an auditioning instrument in the initial versions.

---

# 21. MIDI Output

MIDI output is a first-class feature.

Support:

- MIDI note on/off.
- Velocity.
- MIDI channel.
- CC messages eventually.
- Pitch bend eventually.
- Aftertouch eventually.
- Program change eventually.

The user should be able to choose:

```text
MIDI Output Device
[ My Synthesizer ▼ ]

Channel
[ 1 ]

Clock
[ Internal ▼ ]
```

---

# 22. MIDI Input

Eventually support MIDI input for:

- Selecting root notes.
- Triggering generation.
- Transposing sequences.
- Controlling parameters.
- External MIDI controllers.

For example:

```text
MIDI keyboard
      ↓
Root note
      ↓
Generator
      ↓
Generated sequence
```

---

# 23. MIDI Clock

Support multiple synchronization modes.

## Internal clock

Application controls tempo.

## External MIDI clock

Another device controls tempo.

## DAW synchronization

When operating as a plugin, synchronize to host transport.

Important concepts:

- BPM.
- PPQ.
- Beat position.
- Bar position.
- Start.
- Stop.
- Continue.
- Transport position.

Timing must be implemented carefully and tested under load.

---

# 24. MIDI Export

Provide:

```text
Export MIDI
```

Output:

```text
BerlinSequence.mid
```

The MIDI file should contain:

- Tempo.
- Time signature.
- Notes.
- Velocities.
- Note durations.
- MIDI channel.

Eventually export:

- Multiple tracks.
- Multiple sequences.
- CC automation.
- Tempo changes.

---

# 25. Preset System

Presets should store:

- Generator parameters.
- Scale.
- Root.
- Sequence length.
- Rhythm settings.
- Mutation settings.
- Synth parameters.
- MIDI configuration where appropriate.

Presets should be human-readable.

JSON is recommended.

Example:

```json
{
  "tempo": 128,
  "root": "C",
  "scale": "minor",
  "steps": 16,
  "density": 0.65,
  "repetition": 0.85,
  "motion": 0.55,
  "tension": 0.25,
  "complexity": 0.40,
  "mutationRate": 0.10,
  "swing": 0.12
}
```

---

# 26. Project File

Separate presets from projects.

A project can contain:

- Multiple sequences.
- Presets.
- MIDI routing.
- Synth configuration.
- Current transport position.
- Arrangement/evolution configuration.

Potential format:

```text
project.berlin
```

Internally it could be JSON or a ZIP containing JSON and assets.

Do not over-engineer this in version 1.

---

# 27. Multiple Sequences

Eventually support multiple layers:

```text
Sequence 1 → Main Arpeggio
Sequence 2 → Bass
Sequence 3 → Secondary Motif
Sequence 4 → Percussion Trigger
```

Each sequence can have independent:

- Length.
- Scale behavior.
- Rhythm.
- Mutation.
- MIDI channel.
- MIDI output.
- Transposition.

This enables complete generative performances.

---

# 28. Generative Bass

Add a dedicated bass generator later.

It can derive material from the main sequence.

Example:

```text
Main sequence
     ↓
Extract roots
     ↓
Reduce rhythm
     ↓
Shift octave
     ↓
Bass sequence
```

This creates musical relationships between layers.

---

# 29. Generative Harmony

A future harmony engine can generate:

- Chord progression.
- Modal changes.
- Root movement.
- Long-form harmonic structure.

Important:

Do not make harmony mandatory.

The classic Berlin School concept can work with static harmony and evolving sequences.

---

# 30. Advanced Algorithms

The architecture should leave room for advanced generators.

Possible algorithms:

- Weighted random walks.
- Markov chains.
- Euclidean rhythms.
- Cellular automata.
- Turing Machine–style sequencers.
- Probability matrices.
- Constraint-based generation.
- Motif transformation.
- Genetic/evolutionary generation.
- Rule-based composition.

These should be plugins/modules to the generation system rather than hard-coded into one monolithic generator.

---

# 31. AI Integration — Future Possibility

AI should not be required for the core system.

However, the architecture could eventually support an AI layer.

For example:

```text
User:
"Generate a slow dark sequence in C minor
with lots of repetition and occasional octave jumps."

              ↓

AI interprets request

              ↓

Structured parameters

              ↓

Sequencer Engine

              ↓

Sequence
```

The AI should preferably generate structured parameters rather than directly generating MIDI.

Example:

```json
{
  "tempo": 110,
  "scale": "C minor",
  "density": 0.35,
  "repetition": 0.9,
  "motion": 0.55,
  "tension": 0.35,
  "octaveJumpProbability": 0.15
}
```

This keeps the deterministic musical engine in control.

---

# 32. Testing Strategy

The project should have automated tests from the beginning.

## Unit tests

Test:

- Scale generation.
- Interval selection.
- Rhythm generation.
- Euclidean rhythms.
- Mutation.
- Transposition.
- Rotation.
- Reversal.
- Seed reproducibility.
- MIDI conversion.

Example:

```text
Given seed X
    ↓
Generate sequence
    ↓
Generate again with seed X
    ↓
Sequences must be identical
```

## Musical constraints

Tests should verify:

- Notes remain within configured ranges.
- Notes belong to selected scale when required.
- Gate times remain valid.
- MIDI values stay between 0 and 127.
- Sequence duration remains valid.

---

# 33. Real-Time Safety

The audio/MIDI thread must be treated differently from the GUI thread.

Avoid on the real-time thread:

- Memory allocation.
- File I/O.
- Blocking locks.
- Expensive calculations.
- GUI operations.
- Logging that can block.

The generator can prepare sequences ahead of playback.

The real-time layer should primarily execute already-prepared events.

---

# 34. Separation of Concerns

A critical architectural rule:

```text
GUI
  ↓
Parameter Model
  ↓
Generation Engine
  ↓
Sequence Model
  ↓
Playback / Export
```

Do not do:

```text
GUI button
    ↓
Directly manipulate MIDI device
```

Instead:

```text
GUI
 ↓
Command / parameter change
 ↓
Engine
 ↓
Sequence
 ↓
MIDI
```

This will make the project much easier to extend.

---

# 35. Suggested Repository Structure

```text
berlin-school-sequencer/
│
├── CMakeLists.txt
├── README.md
├── LICENSE
│
├── docs/
│   ├── architecture.md
│   ├── musical-model.md
│   ├── midi.md
│   └── roadmap.md
│
├── src/
│   │
│   ├── core/
│   │   ├── Sequence.h
│   │   ├── Sequence.cpp
│   │   ├── Step.h
│   │   ├── Pattern.h
│   │   └── MusicalTime.h
│   │
│   ├── generation/
│   │   ├── PatternGenerator.h
│   │   ├── RhythmGenerator.h
│   │   ├── PitchGenerator.h
│   │   ├── MutationEngine.h
│   │   ├── ProbabilityEngine.h
│   │   └── EvolutionEngine.h
│   │
│   ├── midi/
│   │   ├── MidiOutput.h
│   │   ├── MidiInput.h
│   │   ├── MidiClock.h
│   │   └── MidiExporter.h
│   │
│   ├── audio/
│   │   ├── Synth.h
│   │   ├── Voice.h
│   │   ├── Oscillator.h
│   │   ├── Filter.h
│   │   └── Effects.h
│   │
│   ├── presets/
│   │   ├── Preset.h
│   │   └── Project.h
│   │
│   └── ui/
│       ├── MainComponent.h
│       ├── PianoRoll.h
│       ├── StepEditor.h
│       └── ParameterPanel.h
│
└── tests/
    ├── SequenceTests.cpp
    ├── RhythmTests.cpp
    ├── PitchTests.cpp
    ├── MutationTests.cpp
    └── MidiTests.cpp
```

---

# 36. MVP — Minimum Viable Product

Do not build everything immediately.

Version 0.1 should do only this:

```text
Generate
   ↓
16-step sequence
   ↓
Play
   ↓
Internal simple synth
   ↓
MIDI output
   ↓
Export MIDI
```

Parameters:

- BPM.
- Root.
- Scale.
- Number of steps.
- Density.
- Gate.
- Velocity.
- Random seed.
- Mutation amount.

UI:

- Piano roll.
- Generate button.
- Randomize button.
- Play/Stop.
- MIDI output selector.
- MIDI export.
- Basic synth controls.

This is enough to prove the concept.

---

# 37. Development Roadmap

## Phase 1 — Technical Foundation

Goals:

- Create JUCE project.
- Configure CMake.
- Create Git repository.
- Establish cross-platform build.
- Create basic application window.

Deliverable:

```text
Empty JUCE application
```

---

## Phase 2 — Sequence Model

Implement:

- Sequence.
- Step.
- Musical time.
- MIDI note representation.
- Serialization.

Deliverable:

```text
Create sequence in code
Save/load sequence
```

---

## Phase 3 — First Generator

Implement:

- Scale engine.
- Basic pitch generator.
- Basic rhythm generator.
- Random seed.

Deliverable:

```text
Generate 16-step musical patterns
```

---

## Phase 4 — Playback

Implement:

- Transport.
- BPM.
- Timing.
- Internal MIDI event scheduler.

Deliverable:

```text
Generated sequence plays correctly
```

---

## Phase 5 — MIDI Output

Implement:

- MIDI device enumeration.
- MIDI output.
- Note on/off.
- Channel selection.

Deliverable:

```text
Application controls external synthesizer
```

---

## Phase 6 — MIDI Export

Implement:

- MIDI file creation.
- Tempo.
- Notes.
- Velocity.
- Duration.

Deliverable:

```text
Exportable .mid files
```

---

## Phase 7 — User Interface

Implement:

- Piano roll.
- Parameter controls.
- Generate.
- Randomize.
- Mutate.
- Presets.

Deliverable:

```text
Usable standalone application
```

---

## Phase 8 — Internal Synth

Implement:

- Oscillator.
- Filter.
- Envelope.
- LFO.
- Delay.
- Reverb.

Deliverable:

```text
Application can sound sequences without external hardware
```

---

## Phase 9 — Evolution

Implement:

- Mutation engine.
- Evolution rate.
- Long-form sequence evolution.
- Transposition.
- Pattern transformations.

Deliverable:

```text
Sequence can evolve while playing
```

---

## Phase 10 — Advanced Generators

Implement:

- Euclidean rhythms.
- Polyrhythms.
- Multiple sequences.
- Motif transformations.
- Probability matrices.

Deliverable:

```text
Advanced generative sequencing system
```

---

## Phase 11 — Plugin

Build:

- VST3.
- AU where applicable.

The plugin should use the same core engine.

Deliverable:

```text
DAW plugin
```

---

# 38. AI-Assisted Development Strategy

The project should be developed in small, verifiable increments.

Do not ask an AI coding assistant:

> "Build the whole application."

Instead provide it with:

1. Architecture.
2. Current task.
3. Existing code.
4. Constraints.
5. Tests.
6. Expected result.

Example task:

```text
Implement RhythmGenerator.

Requirements:

- Input: number of steps.
- Input: density 0..1.
- Input: seed.
- Output: deterministic activation pattern.
- Must not allocate during playback.
- Add unit tests.
- Do not modify GUI code.
```

Then validate before proceeding.

---

# 39. AI Coding Rules

AI coding agents should follow these principles:

### Rule 1

Do not modify unrelated files.

### Rule 2

Do not introduce dependencies without explaining why.

### Rule 3

Every significant algorithm should have tests.

### Rule 4

Keep generation deterministic when a seed is supplied.

### Rule 5

Keep real-time audio/MIDI code allocation-free.

### Rule 6

Do not put musical logic inside UI classes.

### Rule 7

Keep interfaces small.

### Rule 8

Prefer composition over a huge generator class.

### Rule 9

Document musical assumptions.

### Rule 10

Build after every meaningful change.

---

# 40. Definition of Done

A feature is not finished merely because the code compiles.

For each feature:

```text
Code
 ↓
Unit tests
 ↓
Build
 ↓
Manual test
 ↓
Documentation
```

For real-time features additionally:

```text
CPU test
 ↓
Timing test
 ↓
Stress test
```

---

# 41. Important Design Principle: Musicality Over Randomness

The core differentiator should be musicality.

A random generator is easy:

```text
random MIDI note
```

A useful generator is harder:

```text
musical constraints
+
motif
+
rhythm
+
probability
+
controlled variation
+
long-term evolution
```

The system should prefer:

> "surprising but coherent"

over:

> "completely random."

---

# 42. Example Generation

A possible default generation process:

```text
User chooses:

Root = C
Scale = Minor
Steps = 16
Density = 60%
Repetition = 85%
Motion = 50%
Tension = 20%
Complexity = 30%
Seed = 92831

             ↓

Create scale

C D Eb F G Ab Bb

             ↓

Create motif

C G Eb G

             ↓

Repeat / transform motif

C G Eb G
C G Bb G
C G Eb G
C Bb G C

             ↓

Apply rhythm

X X - X
X - X X
X X - X
X - X X

             ↓

Apply octave variation

C3 G3 Eb3 G4
C3 G3 Bb3 G3
...

             ↓

Apply velocity

100 72 82 68 ...

             ↓

Apply probability

             ↓

Final Sequence

             ↓
      ┌──────┼──────┐
      ↓      ↓      ↓
    MIDI   MIDI    Synth
   output  file    audio
```

---

# 43. Potential Future Features

Once the core system is stable:

- Arrangement timeline.
- Scenes.
- Performance mode.
- Live mutation.
- MIDI controller mapping.
- MIDI CC generation.
- Automation.
- Multiple synth outputs.
- Per-sequence effects.
- Preset browser.
- Cloud preset sharing.
- Pattern sharing.
- AI text-to-sequence generation.
- Generative album/long-form mode.
- Hardware controller integration.
- MPE.
- Ableton Link if technically appropriate.
- OSC.
- Network synchronization.

These should remain outside the MVP.

---

# 44. Recommended First Implementation Task

The first coding task should NOT be the GUI.

Start with the core model.

Implement:

```text
Sequence
Step
Scale
PitchGenerator
RhythmGenerator
RandomSeed
```

Then create a simple command-line or unit-test-driven environment that can produce sequences.

For example:

```text
Seed: 12345

C3 G3 Eb3 G3
C3 G3 Bb3 G3
C3 Eb3 G3 C4
```

Once this works reliably, connect it to JUCE playback.

This reduces the risk of mixing musical algorithm development with UI and audio debugging.

---

# 45. Immediate Next Steps

Recommended order:

1. Create Git repository.
2. Create JUCE + CMake project.
3. Implement `Step`.
4. Implement `Sequence`.
5. Implement `Scale`.
6. Implement deterministic random number generator.
7. Implement first `PitchGenerator`.
8. Implement first `RhythmGenerator`.
9. Add unit tests.
10. Generate sequences from a command/test environment.
11. Implement MIDI event conversion.
12. Implement real-time playback.
13. Add MIDI output.
14. Add MIDI export.
15. Build first UI.
16. Add mutation.
17. Add internal synthesizer.
18. Add evolution.
19. Add advanced generators.
20. Consider plugin packaging.

---

# 46. Long-Term Product Goal

The final application should feel less like programming a sequencer and more like interacting with a musical organism.

The user should be able to:

```text
Choose a musical starting point
          ↓
Generate
          ↓
Listen
          ↓
Mutate
          ↓
Lock what they like
          ↓
Generate again
          ↓
Evolve over time
          ↓
Perform
          ↓
Export MIDI
```

The key concept is:

> **Generate, constrain, listen, mutate, evolve.**

The system should make it very easy to go from "I want a Berlin School sequence" to a useful musical result in seconds, while allowing an advanced user to drill down into every component of the generation process.

---

# 47. Suggested Initial AI Coding Prompt

The following prompt can be given to an AI coding agent as the starting point:

```text
You are helping develop a cross-platform C++/JUCE application called
Berlin School Generative Sequencer.

The application generates musical sequences inspired by Berlin School
electronic music. It must support deterministic generative algorithms,
real-time MIDI playback, MIDI export, an internal synthesizer, and
eventually VST3/AU plugin versions.

Architecture principles:

1. Keep the musical sequencing engine independent from the GUI.
2. Keep the sequencing engine independent from the audio engine.
3. Keep MIDI output independent from sequence generation.
4. Prefer small composable classes.
5. Use deterministic random generation when a seed is supplied.
6. Do not allocate memory or perform blocking operations in real-time
   audio/MIDI callbacks.
7. Add unit tests for musical algorithms.
8. Use CMake and JUCE.
9. Do not introduce unnecessary dependencies.
10. Do not implement future features until the current architecture
    supports them cleanly.

For the first implementation phase, create:

- Step
- Sequence
- MusicalTime
- Scale
- deterministic random generator

Then implement:

- PitchGenerator
- RhythmGenerator

The first generator should be capable of producing a 16-step sequence
from:

- root note
- scale
- number of steps
- pitch range
- density
- repetition
- motion
- seed

The output should be deterministic for a given seed.

Do not implement the GUI yet.

Create comprehensive unit tests.

Make sure the project builds successfully with CMake.

Before making architectural changes, explain the reasoning and keep the
implementation aligned with the project architecture described above.
```

---

# 48. Final Architectural Summary

The most important decision is to treat the project as a **musical generation engine first and an application second**.

The architecture should therefore be:

```text
                 ┌─────────────────────┐
                 │     User Interface  │
                 └──────────┬──────────┘
                            │
                            ▼
                 ┌─────────────────────┐
                 │   Parameter Model   │
                 └──────────┬──────────┘
                            │
                            ▼
                 ┌─────────────────────┐
                 │ Sequencing /        │
                 │ Generation Engine   │
                 └──────────┬──────────┘
                            │
                     Sequence Model
                            │
          ┌─────────────────┼─────────────────┐
          ▼                 ▼                 ▼
       MIDI OUT         MIDI EXPORT      INTERNAL SYNTH
          │                 │                 │
          ▼                 ▼                 ▼
      Hardware          DAW / File         Audio
```

If this separation is maintained, the same musical engine can eventually power:

- Standalone application.
- MIDI generator.
- Hardware controller.
- VST3 plugin.
- AU plugin.
- Generative performance tool.
- AI-assisted sequence generator.
- MIDI pattern library.

That gives the project a strong foundation without forcing all of those features into the first version.
