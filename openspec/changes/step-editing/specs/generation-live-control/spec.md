# Delta for Generation Live Control

## MODIFIED Requirements

### Requirement: Audio-Thread-Safe Regeneration Handoff

Regeneration MUST use a message-thread-to-audio-thread publish/adopt handoff: the message thread builds a complete `Sequence` and publishes it into a staging slot (`SequencePlayer::publishSequence`); the audio thread, at the top of its own `process()`, adopts it only as a whole — emitting a note-off for any currently-sounding note first (the same note-off contract `flushPendingNoteOff` provides, applied inline as part of adoption rather than via a second call to that method), swapping the sequence in without allocation (`Sequence::swap`), and resetting its playhead to step 1. Adoption on the audio thread MUST NOT allocate, MUST NOT use a blocking lock, and MUST NOT introduce logging.

The handoff MUST also offer a keep-position publish variant, used only by step edits. Adopting it MUST swap the sequence without resetting the transport or playhead, and MUST emit a note-off only when the sounding step is the edited step and its note or gate changed. It MUST satisfy the same no-allocation, no-lock, no-logging, no-torn-read rules. Whole-sequence publishes (regenerate, randomize, loadPreset, mutate) MUST keep the reset behavior above.
(Previously: only the reset-to-step-1 adoption existed.)

#### Scenario: Regeneration while audio is running causes no hung note, dropout, assert, or crash

- GIVEN audio is running and a note may be sounding
- WHEN Generate or Randomize is triggered
- THEN the previously sounding note (if any) receives a note-off, the new `Sequence` is adopted, and no hung note, dropout, assert, or crash occurs

#### Scenario: Audio thread never observes a partially-built or torn Sequence during adoption

- GIVEN the message thread is mid-build of a new `Sequence`
- WHEN the audio thread is simultaneously rendering
- THEN the audio thread reads either the complete old `Sequence` or the complete new one, never a torn mixture (the audio thread only ever reads `sequence`, which it alone replaces via `swap`; the message thread never touches `sequence` directly, only the `pendingSequence` staging slot behind the `sequencePending` atomic flag)

#### Scenario: Keep-position adoption does not reset [T]

- GIVEN a keep-position publish is pending and playback is mid-loop
- WHEN the audio thread adopts it
- THEN the new sequence is in place and the playhead and loop count are unchanged

#### Scenario: Whole-sequence publish still resets [T]

- GIVEN a regenerate publish is pending mid-loop
- WHEN it is adopted
- THEN playback restarts from step 1
