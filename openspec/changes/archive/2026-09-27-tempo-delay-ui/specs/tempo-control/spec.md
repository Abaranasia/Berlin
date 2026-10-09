# Tempo Control Specification

## Purpose

User-facing BPM control and the note-division-to-seconds conversion that tempo-synced features build on (delay-sync now; Slice 3's recommendation panel later). Owns the BPM value/range contract, the real-time-safe UI-to-audio publish path, and the pure `(bpm, division) -> seconds` conversion. Does not own `Transport`'s internal step-boundary math (see `playback-transport`) or the delay buffer clamp (see `internal-synth-output`).

## Requirements

### Requirement: BPM Value, Range, and Default

The system MUST expose a user-adjustable BPM in the inclusive range [40, 240], defaulting to 120 at first launch. A value outside this range MUST be clamped to the nearest bound, never rejected outright or left unset.

#### Scenario: Default BPM at first launch
- GIVEN a freshly launched application with no saved preset loaded
- WHEN BPM is inspected
- THEN it equals 120

#### Scenario: Out-of-range input is clamped to the nearest bound
- GIVEN the user attempts to set BPM below 40 or above 240
- WHEN the value is applied
- THEN BPM is clamped to 40 or 240 respectively, not rejected

#### Scenario: In-range values pass through unchanged
- GIVEN the user sets BPM to any value within [40, 240]
- WHEN the value is applied
- THEN BPM equals exactly that value

### Requirement: Real-Time-Safe Propagation to Transport

The system MUST propagate a BPM change from the message thread to `Transport`'s audio-thread-read state using only atomic operations on the path exercised at every block - no heap allocation, no lock, no logging/formatting call.

#### Scenario: BPM change reaches Transport without audio-thread allocation or lock
- GIVEN the UI changes BPM while audio is running
- WHEN the propagation path is inspected during code review
- THEN it contains no heap allocation, no lock acquisition, and no logging call

#### Scenario: Rapid repeated BPM changes remain allocation-free
- GIVEN the user drags a BPM control, producing many changes per second
- WHEN each change propagates
- THEN no change introduces allocation, unbounded queuing, or blocking

### Requirement: Sync Division Enum and Seconds Conversion

The system MUST provide a fixed set of sync divisions - quarter (factor 1.0), eighth (0.5), eighth-triplet (1/3), dotted-eighth (0.75), sixteenth (0.25), half (2.0) - and MUST compute `seconds = (60 / BPM) x factor` for any (BPM, division) pair, independent of JUCE.

#### Scenario: Quarter note at 120 BPM
- GIVEN BPM = 120, division = quarter
- WHEN converted
- THEN the result equals 0.5 seconds

#### Scenario: Dotted-eighth at 90 BPM
- GIVEN BPM = 90, division = dotted-eighth
- WHEN converted
- THEN the result equals 0.5 seconds

#### Scenario: Eighth-triplet is exactly one third of a quarter note
- GIVEN any BPM
- WHEN quarter-note seconds and eighth-triplet seconds are compared
- THEN eighth-triplet seconds equals exactly 1/3 of quarter-note seconds

### Requirement: Sync/Free Toggle for Delay Time

The system MUST provide a Sync/Free toggle governing delay time. While Sync is active, delay time MUST track the division-derived seconds value (clamped per `internal-synth-output`) and the manual delay-time control MUST be disabled for direct entry. While Free is active, the user MAY set delay time manually within `internal-synth-output`'s existing bounds, and the last manual value MUST be retained across a Sync-Free-Sync round trip.

#### Scenario: Enabling Sync disables manual entry
- GIVEN Free mode with a manually set delay time
- WHEN the user enables Sync
- THEN the delay-time control becomes non-editable and shows the division-derived value

#### Scenario: Disabling Sync restores manual control
- GIVEN Sync mode is active
- WHEN the user switches to Free
- THEN the delay-time control becomes editable again and shows the last manual value, not the division-derived value
