# Delta for Web UI Layout

Built on top of the pending `step-velocity` delta. The Step View requirement is not modified by this change; the step-controls requirement below is purely additive.

## ADDED Requirements

### Requirement: Step Controls Hold Three Buttons

The `step-controls` area under each step cell MUST hold exactly three buttons, in DOM and tab order: the velocity button (`.step-vel`), the gate-length button (`.step-gate`) and the skip toggle (`.step-skip`). At the 16-steps-per-row layout a cell is too narrow for three buttons side by side, so `step-controls` MUST lay out as two columns: the velocity and gate-length buttons share the first row, and the skip toggle spans both columns in the second row. The buttons MUST NOT clip, overlap or overflow the cell horizontally. Each button MUST keep the same minimum-target treatment the existing step controls use. The area MUST NOT change the cell grid's wrapping into rows of 16.

#### Scenario: Three buttons per cell [V]
- GIVEN a rendered grid
- WHEN each `step-controls` element is inspected
- THEN it contains the velocity, gate-length and skip buttons in that DOM order

#### Scenario: Controls fit at 16 steps [M]
- GIVEN 16 steps in one row at the minimum supported window width
- WHEN rendered
- THEN velocity and gate-length sit side by side, skip spans the row below, and nothing overlaps or overflows the cell

#### Scenario: Wrapping unchanged [V]
- GIVEN a snapshot with 40 steps
- WHEN rendered
- THEN cells are shown in rows of 16, 16 and 8 as before

#### Scenario: Gate level is visually distinct [M]
- GIVEN steps at staccato, short, long and full
- WHEN viewed
- THEN the four gate levels are distinguishable by the button's bar width and the active-step visuals are unaffected
