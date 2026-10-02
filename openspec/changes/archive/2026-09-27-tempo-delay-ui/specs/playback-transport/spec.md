# Delta for Playback Transport

## MODIFIED Requirements

### Requirement: Runtime BPM Mutation With Origin-Rebased Boundary Math

The system MUST expose a runtime `setBpm(double)` mutator on `Transport`, callable from the message thread, that recomputes `samplesPerStep` from the currently cached sample rate. Every call MUST rebase the boundary origin (the `position`/`nextStepCounter` pair or equivalent) at the moment of change, so boundaries already reported keep their original reported sample positions, and future boundaries are computed relative to the new origin under the new rate - never by substituting the new rate into the existing absolute `k * samplesPerStep` formula unchanged, which would retime already-elapsed steps. `bpm` remains fixed at its constructed/default value (120) until `setBpm` is called. The mutator MUST perform only cheap arithmetic - no heap allocation, no lock, no logging.
(Previously: "Fixed BPM and Step Resolution" - `bpm` was fixed at construction/config time with no runtime mutator, and the system explicitly MUST NOT expose any API to change it afterward.)

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

## Note for Archive (non-canonical)

The main spec's Purpose sentence - "No runtime BPM control surface exists in this change." - is now false and MUST be updated by hand at archive time alongside the Requirements merge; it is prose, not a delta-mergeable Requirement block.
