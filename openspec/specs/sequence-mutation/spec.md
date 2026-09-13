# Sequence Mutation Specification

## Purpose

Manual, one-click mutation of the current sequence: a pure, deterministic transform library (`berlin::MutationEngine`) plus a `MainComponent::mutate()` trigger that publishes the result through the existing playback pipeline. Gives users a middle ground between "keep exactly" and "regenerate from scratch."

## Requirements

### Requirement: Pure Mutation Transforms

`MutationEngine` MUST expose stateless free functions, each `(const Sequence&, DeterministicRandom&) -> Sequence`, for: transpose, reverse, rotate, change-note, add-step, remove-step, stretch, compress. Each transform MUST be total — no crash or UB for any valid `Sequence`, including size 0, size 1, or zero active steps.

#### Scenario: Transpose shifts active note pitches only

- GIVEN a Sequence with active and inactive steps
- WHEN transpose is applied
- THEN every active step's `note` shifts by the same signed offset
- AND inactive steps remain unchanged (note value untouched)

#### Scenario: Degenerate input never crashes

- GIVEN a Sequence of size 0, size 1, or with zero active steps
- WHEN any transform is applied
- THEN a valid Sequence is returned with no crash or undefined behavior

### Requirement: Transform Bounds

Transpose MUST clamp the requested offset — once, against the minimum and maximum note among active steps — so that applying the (possibly reduced) offset uniformly to every active step never pushes any of them outside the valid MIDI note range (0-127). Individual notes MUST NOT be clamped independently of one another, since that would compress or expand the intervals between notes. Stretch and compress MUST clamp resulting sequence length to [4, 64] steps.

#### Scenario: Transpose clamps the offset, preserving intervals

- GIVEN active steps at notes 100 and 125, and a requested transpose offset of +12
- WHEN transpose is applied
- THEN the offset actually used is reduced to +2 (the largest offset that keeps the highest active note, 125, at or below 127)
- AND the resulting notes are 102 and 127 — the original 25-semitone interval between them is preserved

#### Scenario: Stretch respects the upper bound

- GIVEN a Sequence of 60 steps
- WHEN stretch is applied with a factor that would exceed 64 steps
- THEN the result is clamped to 64 steps

#### Scenario: Compress respects the lower bound

- GIVEN a Sequence of 6 steps
- WHEN compress is applied with a factor that would go below 4 steps
- THEN the result is clamped to 4 steps

### Requirement: Change-Note and Add-Note Stay In-Pattern

`change-note` and `add-note` MUST derive the pitch they assign from another existing active step in the same Sequence (never from a fresh scale/range draw, and never fabricating `note = 0`). If the Sequence has no active step (or, for `change-note`, fewer than two), the transform MUST be a no-op returning a copy of the input.

#### Scenario: change-note re-voices using an existing pitch

- GIVEN a Sequence with at least two active steps carrying different notes
- WHEN change-note is applied
- THEN one active step's note is replaced with the note copied from a different active step
- AND no note value appears in the result that was not already present among the input's active steps

#### Scenario: change-note is a no-op below the minimum active-step count

- GIVEN a Sequence with fewer than two active steps
- WHEN change-note is applied
- THEN the result is unchanged from the input

### Requirement: Single Random Transform Per Mutate Click

Each Mutate click MUST apply exactly one transform, chosen at random from the full transform set. Transform selection is not user-configurable in this change.

#### Scenario: One transform per click

- GIVEN the user clicks Mutate once
- WHEN `mutate()` runs
- THEN exactly one transform from the set is applied to produce the new sequence

### Requirement: Deterministic Mutation Reproducibility

Mutation randomness MUST derive from a base seed tied to the sequence actually in play — NOT the raw `currentSeed` UI field, since that field can be edited without regenerating and would otherwise desynchronize from the live sequence — plus a per-click generation counter, so that "seed X + N mutation clicks" always reproduces the same resulting Sequence and the same transform choice sequence. The base seed MUST be captured only at the moment a sequence is actually (re)generated, not read live from the seed field at mutate time.

#### Scenario: Same seed and click count reproduce identically

- GIVEN two runs starting from the same generated sequence (same base seed)
- WHEN Mutate is clicked the same number of times (N) in both runs, with no intervening Generate
- THEN both runs produce byte-identical resulting Sequences

#### Scenario: Editing the seed field without regenerating does not affect in-flight mutation

- GIVEN a sequence has already been generated from seed X and mutated once
- WHEN the user edits the seed field to a different value Y without clicking Generate
- THEN a subsequent Mutate click continues the mutation chain based on X, unaffected by the edited (but not yet applied) field value Y

#### Scenario: Generate restores the seeded original (undo story)

- GIVEN a sequence that has been mutated one or more times
- WHEN the user clicks Generate with the seed field unchanged
- THEN the original seeded sequence is restored exactly, discarding all mutations, and the mutation chain resets (the next Mutate click starts again from generation 1)

### Requirement: Manual Mutate Trigger

`MainComponent` MUST expose a `mutate()` method and a Mutate button that: builds a mutated copy of `currentSequence`, publishes a copy via `player.publishSequence()` (mirroring `regenerate()`'s copy-then-publish pattern), and on success sets `currentSequence` to the mutated result so MIDI export and preview reflect it.

#### Scenario: Successful mutate updates playback and export state

- GIVEN a valid `currentSequence` and no pending unadopted publish
- WHEN the user clicks Mutate
- THEN the mutated sequence is published to the player
- AND `currentSequence` becomes the mutated sequence
- AND the mutation generation counter advances by one

#### Scenario: Busy publish leaves state unchanged

- GIVEN a publish is already pending and unadopted
- WHEN the user clicks Mutate
- THEN the publish is rejected, `currentSequence` is left unchanged, the mutation generation counter is left unchanged, and the status label shows "Busy, try again" (same contract as `regenerate()`)
- AND a subsequent Mutate click retries from the same generation, reproducing the same candidate mutation

### Requirement: Publish Always Resets Playback Phase

Publishing a mutated sequence MUST reset the transport and playhead to step 0, identically to Generate/Randomize today. No phase-preserving publish path exists in this change.

#### Scenario: Mutate restarts the loop at step 0

- GIVEN playback is running mid-loop at any step
- WHEN a mutation is successfully published
- THEN playback resumes from step 0 on the next process block
