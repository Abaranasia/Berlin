# Delta for Step Editing

Built on top of the pending `step-velocity` delta. Archive `step-velocity` first, then this change. In this capability "gate" in older text means the `active` on/off flag; the new per-step length is called gate length (`gatePercent`).

Archive note: the capability Purpose line (after the velocity archive) MUST read "Per-step mute, note, skip, velocity and gate-length editing of the live sequence (up to 64 steps), ...".

## MODIFIED Requirements

### Requirement: Absolute Note and Gate Assignment

A step edit MUST set the step's note and `active` flag to the supplied absolute values. When the optional `skipped` value is supplied it MUST set the flag; when absent the flag MUST be left unchanged. When the optional `velocity` is supplied it MUST set the step's velocity; when absent the current velocity MUST be kept. When the optional `gatePercent` is supplied it MUST set the step's gate length; when absent the current gate length MUST be kept. A `velocity` outside `[1, 127]` MUST be rejected with `velocity-out-of-range` and a `gatePercent` outside `[1, 100]` MUST be rejected with `gate-out-of-range` (never clamped), leaving all state unchanged. Validation order is index, note, velocity, gate, then busy. A note equal to the step's current note MUST always be accepted, even if no longer in `scaleNotes`. A changed note MUST be a member of the current `scaleNotes`, else the edit MUST be rejected with `note-out-of-scale`. A muted step (`active=false`) MUST keep its note, velocity and gate length, and a skipped step MUST keep its note, `active` value, velocity and gate length. Skipping or un-skipping MUST be independent of velocity and gate length. Skipping every step is permitted.
(Previously: no `gatePercent` value and no `gate-out-of-range` rejection; validation order was index, note, velocity, then busy.)

#### Scenario: Mute keeps the note [T]
- GIVEN step 3 is active with note 48
- WHEN it is edited with note 48, active false
- THEN the step is inactive and its note remains 48

#### Scenario: Out-of-scale note is rejected [T]
- GIVEN C minor and a note not in `scaleNotes`
- WHEN a step is edited with that note
- THEN the edit fails with `note-out-of-scale` and the step is unchanged

#### Scenario: Mute allowed on an out-of-scale note [T]
- GIVEN a step whose note is no longer in `scaleNotes` after a scale change
- WHEN it is edited with its current note and active false
- THEN the edit succeeds, the step is inactive and its note is unchanged
- AND an edit that changes it to another out-of-scale note still fails with `note-out-of-scale`

#### Scenario: Idempotent re-send [T]
- GIVEN an edit has succeeded
- WHEN the same absolute values are sent again
- THEN the sequence is identical

#### Scenario: Skip a step [T]
- GIVEN step 4 is active and not skipped
- WHEN it is edited with its note, active true and skipped true
- THEN step 4 has `skipped == true`, `active == true` and the same note

#### Scenario: Omitted skipped leaves the flag unchanged [T]
- GIVEN step 4 is skipped
- WHEN it is edited with a new in-scale note and no `skipped` value
- THEN the note changes and step 4 is still skipped

#### Scenario: Un-skip a step [T]
- GIVEN step 4 is skipped
- WHEN it is edited with skipped false
- THEN step 4 is not skipped and its note and gate length are unchanged

#### Scenario: Skip every step [T]
- GIVEN a sequence of any length
- WHEN every step is edited with skipped true
- THEN all edits succeed and the sequence is all-skipped

#### Scenario: Velocity edit sets the value [T]
- GIVEN step 2 has velocity 100
- WHEN it is edited with its note, active and velocity 127
- THEN step 2 has velocity 127 and only step 2 differs

#### Scenario: Omitted velocity keeps the current value [T]
- GIVEN step 2 has velocity 40
- WHEN it is edited with a new in-scale note and no `velocity`
- THEN the note changes and velocity is still 40

#### Scenario: Out-of-range velocity is rejected, not clamped [T]
- GIVEN step 2 has velocity 100
- WHEN it is edited with velocity 0, separately 128, separately -5
- THEN each fails with `velocity-out-of-range` and the step is unchanged

#### Scenario: Velocity bounds are accepted [T]
- GIVEN any step
- WHEN it is edited with velocity 1, then 127
- THEN both succeed

#### Scenario: Gate edit sets the value [T]
- GIVEN step 2 has `gatePercent` 100
- WHEN it is edited with its note, active and `gatePercent` 50
- THEN step 2 has `gatePercent` 50, its velocity is unchanged and only step 2 differs

#### Scenario: Omitted gate keeps the current value [T]
- GIVEN step 2 has `gatePercent` 25
- WHEN it is edited with a new in-scale note and no `gatePercent`
- THEN the note changes and `gatePercent` is still 25

#### Scenario: Out-of-range gate is rejected, not clamped [T]
- GIVEN step 2 has `gatePercent` 100
- WHEN it is edited with `gatePercent` 0, separately 101, separately -5
- THEN each fails with `gate-out-of-range` and the step is unchanged

#### Scenario: Gate bounds are accepted [T]
- GIVEN any step
- WHEN it is edited with `gatePercent` 1, then 100
- THEN both succeed

#### Scenario: Muted and skipped steps keep their gate length [T]
- GIVEN a step with `gatePercent` 25
- WHEN it is muted, then un-muted, then skipped, then un-skipped
- THEN its `gatePercent` is still 25

### Requirement: Keep-Position Edit Playback

A step edit MUST NOT reset the transport or playhead. If the edited step is the one currently sounding and its note, `active` flag or skipped flag changed, the sounding note MUST receive a note-off at adoption. If the edit changes only the velocity and/or the gate length of the currently sounding step, it MUST emit no note-off and no retrigger: the sounding note keeps its existing note-off time, and the new velocity and gate length apply from the next pass. Otherwise no note-off MUST be sent. After adoption the cursor continues from its current index; if that index now lands on a skipped step it advances to the next non-skipped step (wrapping without counting a loop), per `step-event-scheduling`. An edit MUST be rejected with `busy` while a publish is pending or while a state restore is pending; in both cases the busy check comes after index, note, velocity and gate validation.
(Previously: only a velocity-only edit was exempt from the note-off; busy followed index, note and velocity validation.)

#### Scenario: Playhead continues after edit [T]
- GIVEN playback is at step 9
- WHEN step 3 is edited and adopted
- THEN the playhead continues from step 9 and loop count is unchanged

#### Scenario: Editing the sounding step sends note-off [T]
- GIVEN step 5 is sounding
- WHEN step 5 is muted, skipped, or its note changed, then adopted
- THEN a note-off for the old note is emitted

#### Scenario: Velocity-only edit of the sounding step does not interrupt it [T]
- GIVEN step 5 is sounding
- WHEN only its velocity is changed and adopted
- THEN no note-off and no new note-on are emitted
- AND the next pass note-on carries the new velocity

#### Scenario: Gate-only edit of the sounding step does not interrupt it [T]
- GIVEN step 5 is sounding with `gatePercent` 100
- WHEN only its `gatePercent` is changed to 25 and adopted
- THEN no note-off and no new note-on are emitted at adoption
- AND the sounding note keeps its existing note-off time
- AND on the next pass the note-off is emitted at the 25 percent point of the step

#### Scenario: Velocity plus note change still invalidates [T]
- GIVEN step 5 is sounding
- WHEN its note and velocity are both changed and adopted
- THEN a note-off for the old note is emitted

#### Scenario: Gate plus note change still invalidates [T]
- GIVEN step 5 is sounding
- WHEN its note and gate length are both changed and adopted
- THEN a note-off for the old note is emitted

#### Scenario: Editing another step sends no note-off [T]
- GIVEN step 5 is sounding
- WHEN step 3 is edited (including skipped, velocity or gate length) and adopted
- THEN no note-off is emitted for the sounding note

#### Scenario: Busy rejects the edit [T]
- GIVEN a publish is pending
- WHEN a valid step edit is made
- THEN it fails with `busy` and state is unchanged

#### Scenario: Pending restore rejects a valid edit [T]
- GIVEN a state restore is pending and no publish is pending
- WHEN a valid step edit is made
- THEN it fails with `busy`, the sequence and `edited` are unchanged
- AND once the restore has published, the same edit succeeds

#### Scenario: Validation precedes busy [T]
- GIVEN a publish is pending, or a state restore is pending
- WHEN an edit with an out-of-range index, an out-of-scale note, an out-of-range velocity or an out-of-range gate is made
- THEN it fails with `step-out-of-range`, `note-out-of-scale`, `velocity-out-of-range` or `gate-out-of-range` (checked in that order), not `busy`

### Requirement: Interactive Step Grid

The grid MUST render one cell per step of the current sequence (`steps.length`, up to 64), wrapping into rows of 16. Each cell MUST toggle mute on a click without movement; a vertical drag MUST change the note through `scaleNotes` at a constant pixels-per-note; the wheel MUST step one scale note per notch with a non-passive listener calling `preventDefault`. Each cell MUST also provide a skip toggle that is a distinct control from the active/mute toggle and dispatches `setStep` with `skipped`. Each cell MUST also provide a velocity button and a gate-length button, each a sibling of the cell (not nested in it), each part of the roving tabindex, each with an aria-label that includes its displayed level name. The displayed velocity level is the nearest of ghost 40, soft 70, normal 100, accent 127 to the step's velocity (ties to the higher level); the displayed gate level is the nearest of staccato 25, short 50, long 75, full 100 to the step's `gatePercent` (ties to the higher level). Activating the velocity button MUST advance to the next velocity level strictly above the displayed level, wrapping accent to ghost, and dispatch `setStep` with the absolute velocity of that level. Activating the gate button MUST advance to the next gate level strictly above the displayed level, wrapping full to staccato, and dispatch `setStep` with the absolute `gatePercent` of that level. The cell MUST expose `data-velocity` with the displayed velocity level name and `data-gate-length` with the displayed gate level name (`data-gate` keeps meaning the active flag). Keyboard MUST use a roving tabindex across all cells: Space toggles mute, Up/Down change the note, Left/Right/Home/End move focus. Each cell MUST have an aria-label like "Step 3, C3, on" (adding ", skipped" when skipped) and `aria-pressed` for the gate flag; the skip toggle MUST have its own accessible name and `aria-pressed`. Muted steps MUST show their note dimmed; skipped steps MUST be visibly distinct from muted steps. An "edited" indicator MUST show while `edited` is true.
(Previously: no gate-length button or `data-gate-length`.)

#### Scenario: Click toggles mute [V]
- GIVEN an active step
- WHEN it is clicked without movement
- THEN `setStep` is dispatched with its note, `active:false`, its current velocity and its current `gatePercent`

#### Scenario: Skip toggle is distinct from mute [V]
- GIVEN a non-skipped step
- WHEN its skip toggle is activated
- THEN `setStep` is dispatched with the step's note, its current `active`, `skipped:true`, its current velocity and its current `gatePercent`, and the mute state does not change

#### Scenario: Drag does not toggle [V]
- GIVEN an active step
- WHEN a vertical drag past one note threshold ends
- THEN `setStep` carries the new scale note and no mute toggle occurs

#### Scenario: Wheel steps and prevents scroll [V]
- GIVEN a cell
- WHEN one wheel notch up is dispatched
- THEN the note moves one scale note up and `preventDefault` was called

#### Scenario: Keyboard [V]
- GIVEN a focused cell
- WHEN Space, ArrowUp, ArrowRight and End are pressed
- THEN mute toggles, note steps up, focus moves right, then to the last cell, with only one cell tabbable

#### Scenario: Accessibility and dimming [V]
- GIVEN step 3 at C3 muted
- WHEN rendered
- THEN aria-label is "Step 3, C3, off", `aria-pressed` is false and the note is dimmed

#### Scenario: Skipped rendering [V]
- GIVEN step 3 skipped
- WHEN rendered
- THEN its skip toggle has `aria-pressed` true and the cell has the skipped style, distinct from the muted style

#### Scenario: Cell count follows the sequence length [V]
- GIVEN a snapshot with 40 steps
- WHEN rendered
- THEN 40 cells are shown in rows of 16, 16 and 8

#### Scenario: Edited indicator [V]
- GIVEN `edited` true, then false
- WHEN rendered
- THEN the indicator is shown, then hidden

#### Scenario: Velocity button cycles upward and wraps [V]
- GIVEN a step with velocity 100 (displayed normal)
- WHEN the velocity button is activated three times, each response applied
- THEN `setStep` carries velocity 127, then 40, then 70

#### Scenario: Off-level velocity snaps for display and cycling [V]
- GIVEN velocities 85, 84 and 20
- WHEN rendered and the button is activated once on each
- THEN `data-velocity` is normal, soft and ghost, and the dispatched velocities are 127, 100 and 70

#### Scenario: Velocity button is a sibling in the tab order [V]
- GIVEN a rendered grid
- WHEN the DOM and tabindex are inspected
- THEN each velocity button is a sibling of its cell, its aria-label contains the level name, and only one control in the grid is tabbable

#### Scenario: Gate button cycles upward and wraps [V]
- GIVEN a step with `gatePercent` 100 (displayed full)
- WHEN the gate button is activated three times, each response applied
- THEN `setStep` carries `gatePercent` 25, then 50, then 75

#### Scenario: Off-level gate snaps for display and cycling [V]
- GIVEN `gatePercent` values 90, 38 and 10
- WHEN rendered and the gate button is activated once on each
- THEN `data-gate-length` is full, short and staccato (38 is nearer 50 than 25), and the dispatched values are 25, 75 and 50

#### Scenario: Gate button is a sibling in the tab order [V]
- GIVEN a rendered grid
- WHEN the DOM and tabindex are inspected
- THEN each gate button is a sibling of its cell, its aria-label contains the gate level name, and only one control in the grid is tabbable

#### Scenario: Gate cycle keeps other fields [V]
- GIVEN a step with note 48, active, not skipped, velocity 70
- WHEN its gate button is activated
- THEN `setStep` carries note 48, `active:true`, `skipped:false`, velocity 70 and the next gate level

#### Scenario: WebView2 input [M]
- GIVEN standalone and plugin in WebView2 with auto-evolve on
- WHEN wheel, drag, click and keyboard are used
- THEN the page does not scroll and the loop keeps its position

### Requirement: Velocity Is Session State

Velocity and gate-length edits MUST NOT be stored in presets and the preset schema (v3) MUST be unchanged. MIDI export MUST export the current, edited velocities and gate lengths. Regenerate, randomize and loadPreset MUST discard velocity and gate-length edits (generated steps have velocity 100 and `gatePercent` 100). There is no undo.
(Previously: covered velocity only.)

#### Scenario: Export reflects velocity edits [T]
- GIVEN a step's velocity was edited to 127
- WHEN MIDI is exported
- THEN that step's note-on carries velocity 127

#### Scenario: Regenerate discards velocity edits [T]
- GIVEN a velocity-edited sequence
- WHEN regenerate runs with the same seed
- THEN every step has velocity 100 and the sequence equals the seeded original

#### Scenario: Preset round trip does not store velocity [T]
- GIVEN a preset saved after a velocity edit
- WHEN it is loaded
- THEN the preset XML has no velocity data and the loaded sequence has velocity 100 everywhere

#### Scenario: Export reflects gate edits [T]
- GIVEN a step's `gatePercent` was edited to 50
- WHEN MIDI is exported
- THEN that step's note-off is at the half-step tick

#### Scenario: Regenerate discards gate edits [T]
- GIVEN a gate-edited sequence
- WHEN regenerate runs with the same seed
- THEN every step has `gatePercent` 100 and the sequence equals the seeded original

#### Scenario: Preset round trip does not store gate length [T]
- GIVEN a preset saved after a gate edit
- WHEN it is loaded
- THEN the preset XML has no gate-length data and the loaded sequence has `gatePercent` 100 everywhere
