# Playback Transport Specification

## Purpose

A JUCE-free, sample-accurate step clock. Converts a BPM and step resolution into samples-per-step from the audio device's sample rate, and reports step boundaries crossed within each audio callback using drift-free absolute-position arithmetic. `Transport` is sequence-length-agnostic: it counts absolute step boundaries only and has no notion of a loop length or step position — that belongs to `SequencePlayer` (see `step-event-scheduling`). Runtime BPM mutation is exposed via `setBpm()` with origin-rebasing to preserve already-reported boundaries.

## Requirements

### Requirement: Runtime BPM Mutation With Origin-Rebased Boundary Math

The system MUST expose a runtime `setBpm(double)` mutator on `Transport`, callable from the message thread, that recomputes `samplesPerStep` from the currently cached sample rate. Every call MUST rebase the boundary origin (the `position`/`nextStepCounter` pair or equivalent) at the moment of change, so boundaries already reported keep their original reported sample positions, and future boundaries are computed relative to the new origin under the new rate - never by substituting the new rate into the existing absolute `k * samplesPerStep` formula unchanged, which would retime already-elapsed steps. `bpm` remains fixed at its constructed/default value (120) until `setBpm` is called. The mutator MUST perform only cheap arithmetic - no heap allocation, no lock, no logging.

#### Scenario: Default BPM constant

- GIVEN `Transport` is constructed with the default configuration
- WHEN its BPM is inspected before any `setBpm` call
- THEN it equals 120

#### Scenario: setBpm recomputes samplesPerStep from the cached sample rate

- GIVEN a prepared `Transport` at a known sample rate
- WHEN `setBpm(newBpm)` is called
- THEN `samplesPerStep` equals `sampleRate * 60 / (newBpm * stepsPerBeat)`

#### Scenario: BPM change does not retime already-reported boundaries

- GIVEN a running `Transport` that has already reported several step boundaries at the old BPM
- WHEN `setBpm` changes the rate mid-stream
- THEN every boundary already reported keeps its original reported sample position - none are recomputed or shifted

#### Scenario: BPM change causes no skipped or doubled step across the change

- GIVEN a running `Transport` mid-sequence
- WHEN `setBpm` is called and playback continues across the change
- THEN exactly one step boundary is reported per step interval spanning the change - none is skipped, none is reported twice

#### Scenario: Rapid repeated BPM changes remain drift-free

- GIVEN several `setBpm` calls in quick succession while running
- WHEN boundaries are reported across many subsequent blocks
- THEN cumulative timing error stays bounded to +/-1 sample, matching the existing drift-free guarantee

#### Scenario: setBpm introduces no allocation or lock

- GIVEN `setBpm`'s implementation
- WHEN it is inspected during code review
- THEN it contains no heap allocation, no lock acquisition, and no logging call

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
