# Delta for Internal Synth Voice

## ADDED Requirements

### Requirement: Velocity-Scaled Note Gain

A note-on MUST carry the step velocity, and the voice MUST latch a linear gain of `velocity / 100` for that note, applied per voice at note-on. Velocity 100 MUST give a gain of exactly 1.0, so output is bit-identical to behavior before this change. The target gain MUST be latched at note-on only. When the note-on retriggers a voice that is still sounding (its previous note in release), the applied gain MAY ramp linearly from the previous gain to the new target over at most 2 ms to avoid a click. An idle voice MUST start at the target gain immediately. After that ramp the gain MUST NOT change for the rest of the note. Applying it MUST allocate nothing, take no lock and call no logging function. A ceiling MAY reduce the gain above velocity 100 only if needed to satisfy the no-clip requirement in `internal-synth-output`, and velocity 100 MUST remain exactly 1.0.

#### Scenario: Velocity 100 is bit-identical

- GIVEN the same patch, note and block layout
- WHEN rendered with velocity 100 and with the pre-change code path
- THEN every sample is identical

#### Scenario: Gain is linear in velocity

- GIVEN a sustained note rendered at velocities 50 and 100
- WHEN the peak levels are compared
- THEN the velocity-50 level is half the velocity-100 level within tolerance

#### Scenario: Gain latches at note-on

- GIVEN a note sounding at velocity 40
- WHEN a following note-on arrives at velocity 127
- THEN the new note reaches the velocity-127 gain within 2 ms, and the old gain no longer applies after that

#### Scenario: Idle voice starts at the target gain

- GIVEN an idle voice
- WHEN a note-on arrives at velocity 127
- THEN the first rendered sample already uses the velocity-127 gain (exact `velocityToGain(127) / velocityToGain(100)` ratio, which is the ceiling `kMaxVelocityGain` = 1.12)
