# Playback Transport Specification

## Purpose

A JUCE-free, sample-accurate step clock. Converts a fixed BPM and step resolution into samples-per-step from the audio device's sample rate, and reports step boundaries crossed within each audio callback using drift-free absolute-position arithmetic. `Transport` is sequence-length-agnostic: it counts absolute step boundaries only and has no notion of a loop length or step position — that belongs to `SequencePlayer` (see `step-event-scheduling`). No runtime BPM control surface exists in this change.

## Requirements

### Requirement: Fixed BPM and Step Resolution

The system MUST configure `Transport` with a `bpm` fixed at construction/config time (default constant 120) and `stepsPerBeat` fixed at 4 (one step = a 16th note). The system MUST NOT expose any runtime API to change `bpm` after construction.

#### Scenario: Default BPM constant

- GIVEN `Transport` is constructed with the default configuration
- WHEN its BPM is inspected
- THEN it equals 120

#### Scenario: No runtime BPM mutation API

- GIVEN the `Transport` type definition
- WHEN inspecting its public members
- THEN no setter or method exists that changes `bpm` after construction

### Requirement: Samples-Per-Step Derivation

The system MUST provide `prepare(sampleRate)`, which computes `samplesPerStep = sampleRate * 60 / (bpm * stepsPerBeat)` and MUST NOT divide by zero or crash when `bpm <= 0`.

#### Scenario: Standard rate and BPM produce a fractional value

- GIVEN `sampleRate = 44100`, `bpm = 120`, `stepsPerBeat = 4`
- WHEN `prepare(sampleRate)` is called
- THEN `samplesPerStep` equals 5512.5

#### Scenario: Non-positive BPM never divides by zero

- GIVEN `bpm <= 0`
- WHEN `prepare(sampleRate)` is called
- THEN no division-by-zero occurs and no exception/crash results

### Requirement: Sample-Accurate, Drift-Free Boundary Advance

The system MUST provide `advance(numSamples)`, reporting every step boundary crossed within the block together with its in-block sample offset, derived from an absolute running sample position (never from repeated addition of a rounded integer), so cumulative error does not grow across irregular block sizes.

#### Scenario: Block shorter than one step reports no boundary

- GIVEN `samplesPerStep` greater than the block size
- WHEN `advance(numSamples)` is called with a block smaller than `samplesPerStep`
- THEN no step boundary is reported for that call

#### Scenario: Irregular block sizes still land on correct absolute offsets

- GIVEN a sequence of `advance` calls with varying, non-uniform block sizes
- WHEN each reported boundary's absolute sample position is computed
- THEN it equals `round(k * samplesPerStep)` within ±1 sample for the k-th boundary

#### Scenario: Long-run drift stays bounded

- GIVEN thousands of simulated steps driven by fractional `samplesPerStep`
- WHEN cumulative timestamp error is measured across the run
- THEN it does not grow unbounded and stays within ±1 sample of the ideal position

### Requirement: Running/Stopped State

The system MUST support running and stopped states. While stopped, `advance(numSamples)` MUST report no step boundaries. If `prepare` was never called, `advance` MUST behave safely (no boundary reported, no crash).

#### Scenario: Stopped transport emits nothing

- GIVEN a `Transport` in the stopped state
- WHEN `advance(numSamples)` is called
- THEN no step boundary is reported

#### Scenario: Unprepared transport is safe

- GIVEN a `Transport` on which `prepare` was never called
- WHEN `advance(numSamples)` is called
- THEN no step boundary is reported and no crash or undefined behavior occurs

### Requirement: Unbounded Boundary Counting

The system MUST continue reporting step boundaries indefinitely as `advance` is called repeatedly, with no concept of a loop length, maximum step count, or wraparound — `Transport` counts absolute boundaries only. Looping and step-position wraparound are owned by `SequencePlayer` (see `step-event-scheduling`'s "Step Position Wraps Indefinitely" requirement), which combines `Transport`'s boundary count with `Sequence::size()`.

#### Scenario: Boundary count keeps increasing across many blocks

- GIVEN a running `Transport` processed across many blocks
- WHEN the total number of boundaries reported so far is inspected
- THEN it keeps increasing with no reset, cap, or wraparound at any fixed count
