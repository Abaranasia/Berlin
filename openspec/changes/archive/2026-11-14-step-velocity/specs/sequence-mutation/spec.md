# Delta for Sequence Mutation

## ADDED Requirements

### Requirement: Mutations Preserve Step Velocity

Copy-based transforms (reverse, rotate, stretch, compress, palindrome, add-step, remove-step and any transform that moves or duplicates steps) MUST carry each step's `velocity` with the step. Pitch-only transforms (transpose, invert, scaleIntervals, change-note) MUST leave every `velocity` unchanged. `addNote` MUST activate the target step copying only the note and MUST keep that target step's own stored velocity. `removeNote` MUST leave the step's velocity untouched. Transforms MUST NOT draw randomness for velocity.

#### Scenario: Reverse carries velocity with the step

- GIVEN a 4-step sequence with velocities 40, 70, 100, 127
- WHEN reverse is applied
- THEN the velocities are 127, 100, 70, 40

#### Scenario: Palindrome and stretch carry velocity

- GIVEN a sequence where step 1 has velocity 127
- WHEN palindrome or stretch is applied
- THEN every output step copied from step 1 has velocity 127

#### Scenario: Pitch transforms keep velocity

- GIVEN steps with distinct velocities
- WHEN transpose, invert, scaleIntervals or change-note is applied
- THEN every step's velocity is unchanged

#### Scenario: addNote keeps the target's own velocity

- GIVEN an inactive step with velocity 40 and an active step with velocity 127
- WHEN addNote activates the inactive step
- THEN it takes the copied note and keeps velocity 40

#### Scenario: removeNote leaves velocity

- GIVEN an active step with velocity 127
- WHEN removeNote deactivates it
- THEN its velocity is still 127
