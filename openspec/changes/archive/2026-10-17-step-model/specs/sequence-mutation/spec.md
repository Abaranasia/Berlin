# Delta for Sequence Mutation

## ADDED Requirements

### Requirement: Mutations Respect Skipped Steps

`addNote` and `removeNote` MUST NOT select or modify a skipped step: skipped steps are neither candidates for activation/deactivation nor sources for pitch copying. Copy-based transforms (reverse, rotate, stretch, compress, palindrome, add-step, remove-step and any other transform that moves or duplicates steps) MUST carry the `skipped` flag with the step they copy or move. Pitch-only transforms (transpose, invert, scaleIntervals, change-note) MUST leave every `skipped` flag unchanged. Transforms MUST remain total (no crash or UB) for sequences with skipped steps, including all-skipped sequences, where `hasActive`-style guards make them no-ops. Mutation outputs MAY exceed 16 steps up to 64 and MUST NOT lose skip flags.

#### Scenario: addNote ignores skipped steps

- GIVEN a sequence where all inactive steps are skipped
- WHEN addNote is applied
- THEN the result is an unmodified copy of the input

#### Scenario: removeNote ignores skipped steps

- GIVEN a sequence where the only active-looking candidates are skipped steps
- WHEN removeNote is applied
- THEN no skipped step is modified

#### Scenario: Skipped steps are not pitch sources

- GIVEN a skipped step carrying a distinct note
- WHEN addNote or change-note is applied
- THEN the note of the skipped step never appears as a newly assigned pitch unless it also exists on a non-skipped active step

#### Scenario: Reverse carries the skip flag with the step

- GIVEN a 4-step sequence with step 0 skipped
- WHEN reverse is applied
- THEN the skipped flag is on step 3 of the result and no other step is skipped

#### Scenario: Palindrome and stretch carry skip flags

- GIVEN a sequence with step 1 skipped
- WHEN palindrome or stretch is applied
- THEN every output step copied from step 1 has `skipped == true` and all others `false`

#### Scenario: Pitch transforms keep skip flags

- GIVEN a sequence with skipped steps
- WHEN transpose, invert or scaleIntervals is applied
- THEN every step's `skipped` flag is unchanged

#### Scenario: All-skipped sequence is total

- GIVEN a sequence where every step is skipped
- WHEN any transform is applied
- THEN a valid Sequence is returned with no crash

## MODIFIED Requirements

### Requirement: Change-Note and Add-Note Stay In-Pattern

`change-note` and `add-note` MUST derive the pitch they assign from another existing active, non-skipped step in the same Sequence (never from a fresh scale/range draw, and never fabricating `note = 0`). If the Sequence has no active non-skipped step (or, for `change-note`, fewer than two), the transform MUST be a no-op returning a copy of the input. `add-note` MUST only activate non-skipped inactive steps; `change-note` MUST only re-voice non-skipped active steps.
(Previously: no skip concept; "active step" had no skip qualifier.)

#### Scenario: change-note re-voices using an existing pitch

- GIVEN a Sequence with at least two active non-skipped steps carrying different notes
- WHEN change-note is applied
- THEN one active step's note is replaced with the note copied from a different active non-skipped step
- AND no note value appears in the result that was not already present among the input's active non-skipped steps

#### Scenario: change-note is a no-op below the minimum active-step count

- GIVEN a Sequence with fewer than two active non-skipped steps
- WHEN change-note is applied
- THEN the result is unchanged from the input
