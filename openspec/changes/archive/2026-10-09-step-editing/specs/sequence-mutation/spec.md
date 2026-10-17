# Delta for Sequence Mutation

## MODIFIED Requirements

### Requirement: Publish Always Resets Playback Phase

Publishing a mutated or generated sequence (mutate, auto-evolve, regenerate, randomize, loadPreset) MUST reset the transport and playhead to step 0. Step edits are the only publishes excluded: they use the keep-position handoff defined in `generation-live-control`. A mutation MUST operate on the current sequence including any step edits, so edits are the base for the next mutation.
(Previously: every publish reset to step 0; no phase-preserving path existed.)

#### Scenario: Mutate restarts the loop at step 0

- GIVEN playback is running mid-loop at any step
- WHEN a mutation is successfully published
- THEN playback resumes from step 0 on the next process block

#### Scenario: Mutation builds on edits [T]

- GIVEN step 2 was muted by an edit
- WHEN a mutation that preserves step 2's gate (e.g. transpose) succeeds
- THEN step 2 is still muted in the result

#### Scenario: Edit does not restart the loop [T]

- GIVEN playback is running mid-loop
- WHEN a step edit is adopted
- THEN playback does not restart from step 0
