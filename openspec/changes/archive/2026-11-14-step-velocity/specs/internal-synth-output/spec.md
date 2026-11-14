# Delta for Internal Synth Output

## MODIFIED Requirements

### Requirement: StepEventBuffer Consumption Alongside MIDI Dispatch

The system MUST consume the same per-block `StepEventBuffer` stream (`sampleOffset`, `stepIndex`, `note`, `isNoteOn`, `velocity`) that MIDI dispatch already consumes, as an additive parallel path inside the audio callback. The synth MUST pass each note-on's `velocity` to the voice. This MUST NOT alter the events delivered to, or the behavior of, the existing MIDI dispatch path.
(Previously: the stream had no `velocity` field and the synth ignored velocity.)

#### Scenario: Same events reach both the synth and MIDI dispatch
- GIVEN a block containing note-on and note-off `StepEvent`s
- WHEN the block is processed
- THEN both the synth path and the MIDI dispatch path observe the identical set of events with identical field values

#### Scenario: MIDI dispatch is unaffected by synth consumption
- GIVEN the synth reads the `StepEventBuffer`
- WHEN MIDI dispatch also reads it in the same block
- THEN MIDI dispatch's messages, timestamps, and behavior are identical to Phase 7 behavior

### Requirement: Mixing Without Clipping

The system MUST mix the synth's rendered signal (including any enabled effects) into the audio output buffer with sufficient headroom that a sustained, single-voice note at full level does not clip the output. This MUST also hold at velocity 127 with `outputLevel` at its default 0.8 and effects bypassed (the default): no output sample may exceed `1.0` in magnitude. With effects enabled, velocity MUST NOT add more than the velocity gain ceiling to the peak that velocity 100 already produces. If the bypassed case cannot hold with gain `127/100`, a ceiling MAY lower the gain above velocity 100, while velocity 100 MUST stay exactly 1.0.
(Previously: covered only a sustained full-level note, with no accent velocity case.)
(Note: absolute no-clip with effects enabled is out of scope. The effects path already peaks above 1.0 at velocity 100; that pre-existing gain staging belongs to the standalone clipping follow-up.)

#### Scenario: Sustained full-level note does not clip
- GIVEN the voice sounds a note at full envelope level for a sustained duration, with effects enabled
- WHEN the output is inspected
- THEN no sample exceeds the valid output range

#### Scenario: Accent at the default level does not clip
- GIVEN a note at velocity 127, default `outputLevel` 0.8 and effects bypassed
- WHEN a sustained note is rendered
- THEN every output sample satisfies `|sample| <= 1.0`

#### Scenario: Accent with effects enabled stays within the velocity ceiling
- GIVEN the same note rendered at velocity 100 and at velocity 127, with effects enabled
- WHEN both peaks are measured
- THEN the velocity-127 peak is at most the velocity-100 peak times `kMaxVelocityGain`
