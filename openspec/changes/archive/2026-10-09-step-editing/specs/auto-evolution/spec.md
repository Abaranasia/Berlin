# Delta for Auto-Evolution

## ADDED Requirements

### Requirement: Evolve Runs On Edited Patterns

Auto-Evolve MUST keep running when the pattern has been step-edited. It MUST mutate the current, edited sequence and MUST NOT clear `edited`. A step edit publish pending at a due tick MUST be handled by the existing "Busy-Collision Check Precedes the Mutate Call" retry rule, with no status label. The reset-to-step-0 behavior of automatic mutations (see "Inherits Phase-Reset-On-Publish") is unchanged.

#### Scenario: Evolve mutates the edited sequence [T]

- GIVEN auto-evolve is on and a step was muted by an edit
- WHEN a due automatic mutation succeeds
- THEN it applies to the edited sequence and `edited` stays true

#### Scenario: Edit publish pending at a due tick [T]

- GIVEN a step edit publish is pending and a mutation is due
- WHEN the timer runs
- THEN `mutate()` is not called and no status label shows
- AND the mutation is retried on a later tick

#### Scenario: Edit while evolve is on (manual) [M]

- GIVEN auto-evolve is on
- WHEN steps are edited during playback
- THEN edits land, and the next mutation builds on them
