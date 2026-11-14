# Delta for MIDI Export Timeline

Built on top of the pending `step-velocity` delta (per-step velocity on note-on). `MidiExportEvent` gets no new field and `MidiFileWriter` is unchanged.

## MODIFIED Requirements

### Requirement: Ordered Events With Note-Off Before Note-On

The system MUST emit, for each active step, a note-off event no later than the tick where that step ends and the next step begins. A step with `gatePercent == 100` MUST emit its note-off at that boundary tick, immediately followed (at the same tick) by the next active step's note-on, exactly as before this change. A step with `gatePercent < 100` MUST emit its note-off at the gate-scaled tick defined in "Gate-Scaled Note-Off Ticks". The full emitted event list MUST be non-decreasing in tick position, and at any shared tick a note-off MUST precede a note-on.
(Previously: every note-off was at the tick where the step ends.)

#### Scenario: Consecutive active steps share a boundary tick

- GIVEN two consecutive active steps at gate 100
- WHEN their events are emitted
- THEN the first step's note-off and the second step's note-on both appear at the shared boundary tick, with the note-off ordered first

#### Scenario: Full timeline is tick-ordered

- GIVEN a sequence with multiple active steps, mixed gate lengths, across multiple repeats
- WHEN the complete event list is produced
- THEN every event's tick is greater than or equal to the previous event's tick

### Requirement: Terminal Note-Off Correctness

Because live playback loops forever but an exported file must terminate cleanly, the system MUST emit a note-off for the last sounding note at the final tick of the last repeat (`repeats * nonSkippedCount * ticksPerStep`) when the last non-skipped step is active with `gatePercent == 100`. When that step has `gatePercent < 100` its note-off is already at its gate-scaled tick inside the step and no further terminal note-off MUST be emitted. In every case no note-on in the exported timeline MUST be left without a matching note-off, and the total grid length MUST be unchanged by gate lengths.
(Previously: a terminal note-off was always emitted at the final tick.)

#### Scenario: Final active step gets a closing note-off

- GIVEN a sequence whose last non-skipped step is active at gate 100, exported with repeat count *N*
- WHEN the timeline is built
- THEN a note-off for that step's note appears at the exact final tick of the grid, and no note-on anywhere in the timeline lacks a matching note-off

#### Scenario: Final short-gate step has no extra note-off

- GIVEN a sequence whose last non-skipped step is active with `gatePercent == 50`
- WHEN the timeline is built
- THEN its single note-off is inside that step and no other note-off for it exists at the final tick
- AND every note-on still has exactly one matching note-off

## ADDED Requirements

### Requirement: Gate-Scaled Note-Off Ticks

For an active step with `gatePercent < 100`, the timeline MUST place the note-off at `stepStartTick + clamp((gatePercent * ticksPerStep + 50) / 100, 1, ticksPerStep)` using integer division. A step with `gatePercent == 100` MUST use the legacy boundary placement, so a sequence whose steps all have gate 100 produces a timeline, and exported file bytes, identical to those before this change. Skipped and inactive steps contribute no events regardless of their gate. The same gate that yields N samples in live playback MUST yield the corresponding tick count in export when the ratio is exact.

#### Scenario: Gate tick placement

- GIVEN `ticksPerStep == 240` and active steps with `gatePercent` 25, 50, 75 and 100
- WHEN the timeline is built
- THEN the note-offs are 60, 120, 180 and 240 ticks after their note-ons

#### Scenario: Gate rounding to one tick

- GIVEN a step whose scaled length rounds to 0 ticks (for example `gatePercent == 1` with `ticksPerStep == 10`)
- WHEN the timeline is built
- THEN the note-off is exactly 1 tick after the note-on

#### Scenario: Gate 100 is byte-identical

- GIVEN a sequence of gate-100 steps
- WHEN the timeline is built and written
- THEN the timeline and file bytes equal those produced before this change

#### Scenario: Sample and tick parity at an exact ratio

- GIVEN 24000 samples per step in playback and 240 ticks per step in export, with `gatePercent` 25, 50 and 75
- WHEN the live note length and the exported note length are compared as fractions of the step
- THEN both equal the gate fraction exactly (0.25, 0.5 and 0.75)

#### Scenario: Skipped and inactive steps ignore gate

- GIVEN a skipped step and an inactive step, each with `gatePercent == 25`
- WHEN the timeline is built
- THEN no event is emitted for them and the ticks of other steps are unchanged

#### Scenario: Export reflects gate edits

- GIVEN a step's `gatePercent` was edited to 50
- WHEN MIDI is exported
- THEN that step's note-off is at the half-step tick in every repeat
