# Delta for Generation Live Control

## ADDED Requirements

### Requirement: Sequence Length Control

The UI MUST expose a length control (integer `[1, 64]`, default 16) in the generation controls, backed by `GenerationParams.length`. The bridge MUST clamp finite out-of-range values to `[1, 64]` like other generation params. Changing the length MUST NOT affect the currently playing `Sequence` until Generate (or Randomize/New Seed, or a preset load) regenerates from the current seed, matching how other generation parameters apply. Randomize/New Seed MUST NOT alter the staged `length`. Loading a preset MUST NOT change the live `length`; the preset's seed is applied with the live length. `length` is not persisted.

#### Scenario: Length applies on Generate

- GIVEN `length` is set to 8 while a 16-step Sequence plays
- WHEN Generate is pressed
- THEN the playing Sequence is unaffected until then, and the rebuilt Sequence has 8 steps from the current seed

#### Scenario: Randomize keeps length

- GIVEN `length = 24`
- WHEN Randomize/New Seed is pressed
- THEN the new Sequence has 24 steps and `length` remains 24

#### Scenario: Out-of-range length is clamped

- GIVEN a finite length of 100 (or 0) is set
- WHEN it is applied
- THEN it becomes 64 (or 1) and the command succeeds

#### Scenario: Default length reproduces pre-change output

- GIVEN `length` is left at 16
- WHEN Generate is pressed with an unchanged seed
- THEN the Sequence is byte-identical to the pre-change output

#### Scenario: Preset load uses the live length

- GIVEN `length = 12`
- WHEN a preset is loaded
- THEN the Sequence built from the preset's seed has 12 steps and `length` is still 12

## MODIFIED Requirements

### Requirement: Generate Rebuilds the Sequence From the Current Seed and Restarts Playback

Pressing Generate MUST rebuild a `Sequence` of `GenerationParams.length` steps by composing the currently selected rhythm source (`SkipMaskGenerator` in Random mode, `EuclideanRhythmGenerator` in Euclidean mode, `RhythmGenerator` in Probability mode) and `PitchGenerator` against a `DeterministicRandom` constructed from the current seed, publish it through the handoff, and restart the transport from the first non-skipped step (step 1 of a freshly generated sequence, which has no skips) — not a mid-loop swap that keeps the previous playhead running. `PitchGenerator`'s scale, root, and range MUST come from the current `GenerationParams` values. Any whole-sequence publish discards step edits, including skips.
(Previously: the sequence length was the fixed 16 and not part of the contract.)

#### Scenario: Generate with an unchanged seed, mode and length reproduces an identical Sequence

- GIVEN the current seed, rhythm mode, length and scale/root/range are unchanged since the last Generate
- WHEN Generate is pressed again
- THEN the resulting `Sequence` is byte-identical to the previous one

#### Scenario: Generate mid-playback restarts from step 1

- GIVEN audio is playing partway through the current `Sequence`
- WHEN Generate is pressed
- THEN playback restarts from step 1 of the newly adopted `Sequence`, not from the prior playhead position

#### Scenario: Generate discards skips

- GIVEN steps were skipped by editing
- WHEN Generate is pressed with the same seed
- THEN the new Sequence has no skipped steps

### Requirement: Audio-Thread-Safe Regeneration Handoff

Regeneration MUST use a message-thread-to-audio-thread publish/adopt handoff: the message thread builds a complete `Sequence` and publishes it into a staging slot (`SequencePlayer::publishSequence`); the audio thread, at the top of its own `process()`, adopts it only as a whole — emitting a note-off for any currently-sounding note first, swapping the sequence in without allocation (`Sequence::swap`), and resetting its playhead to step 1. Adoption on the audio thread MUST NOT allocate, MUST NOT use a blocking lock, and MUST NOT introduce logging. A published sequence MAY have any length in `[1, 64]` and MAY contain skipped steps; the handoff MUST NOT assume a fixed length.

The handoff MUST also offer a keep-position publish variant, used only by step edits. Adopting it MUST swap the sequence without resetting the transport or playhead, and MUST emit a note-off only when the sounding step is the edited step and its note, gate or skipped flag changed. If the retained cursor index is `>=` the new length or lands on a skipped step, it advances to the next non-skipped step (wrapping without counting a loop). It MUST satisfy the same no-allocation, no-lock, no-logging, no-torn-read rules. Whole-sequence publishes (regenerate, randomize, loadPreset, mutate) MUST keep the reset behavior above.
(Previously: no length or skip considerations.)

#### Scenario: Regeneration while audio is running causes no hung note, dropout, assert, or crash

- GIVEN audio is running and a note may be sounding
- WHEN Generate or Randomize is triggered
- THEN the previously sounding note (if any) receives a note-off, the new `Sequence` is adopted, and no hung note, dropout, assert, or crash occurs

#### Scenario: Length change across the handoff is safe

- GIVEN playback is running on a 16-step Sequence
- WHEN a 64-step Sequence, then a 1-step Sequence, are published and adopted
- THEN each is adopted whole with no out-of-range cursor, allocation or crash

#### Scenario: Audio thread never observes a partially-built or torn Sequence during adoption

- GIVEN the message thread is mid-build of a new `Sequence`
- WHEN the audio thread is simultaneously rendering
- THEN the audio thread reads either the complete old `Sequence` or the complete new one, never a torn mixture

#### Scenario: Keep-position adoption does not reset [T]

- GIVEN a keep-position publish is pending and playback is mid-loop
- WHEN the audio thread adopts it
- THEN the new sequence is in place and the playhead and loop count are unchanged

#### Scenario: Keep-position adoption never counts a false wrap [T]

- GIVEN a keep-position publish makes the cursor index invalid or skipped
- WHEN the audio thread adopts it and re-positions the cursor, wrapping to the start
- THEN `loopCount` is unchanged and playback does not reset to step 0

#### Scenario: Whole-sequence publish still resets [T]

- GIVEN a regenerate publish is pending mid-loop
- WHEN it is adopted
- THEN playback restarts from step 1
