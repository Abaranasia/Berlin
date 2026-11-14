# Step Editing Specification

## Purpose

Per-step mute, note, skip and velocity editing of the live sequence (up to 64 steps), with scale-aware note stepping, an `edited` flag, and keep-position playback. Verification legend: [V] Vitest, [T] BerlinTests, [M] manual, [B] build.

## Requirements

### Requirement: Step Index Limit

The engine MUST accept an edit only for an index in `[0, size)`, where `size` is the live sequence size (at most `kMaxSteps` = 64; the former fixed 16-step cap is removed). Otherwise it MUST reject with `step-out-of-range` and leave all state unchanged.
(Previously: `[0, min(size, 16))`.)

#### Scenario: Valid index edits the step [T]
- GIVEN a 16-step sequence
- WHEN step 15 is edited with an in-scale note
- THEN the edit succeeds and only step 15 differs

#### Scenario: Index beyond 16 is editable on a longer sequence [T]
- GIVEN a 32-step sequence
- WHEN step 20 is edited with an in-scale note
- THEN the edit succeeds and only step 20 differs

#### Scenario: Index beyond size is rejected [T]
- GIVEN an 8-step sequence, and separately a 32-step sequence
- WHEN index 8 is edited on the first, and index 32 on the second
- THEN both fail with `step-out-of-range` and the sequence is unchanged

#### Scenario: Negative index is rejected [T]
- GIVEN any sequence
- WHEN index -1 is edited
- THEN the edit fails with `step-out-of-range`

### Requirement: Absolute Note and Gate Assignment

A step edit MUST set the step's note and gate to the supplied absolute values. When the optional `skipped` value is supplied it MUST set the flag; when absent the flag MUST be left unchanged. When the optional `velocity` is supplied it MUST set the step's velocity; when absent the step's current velocity MUST be kept. A `velocity` outside `[1, 127]` MUST be rejected with `velocity-out-of-range` (never clamped) and leave all state unchanged. Validation order is index, note, velocity, then busy. A note equal to the step's current note MUST always be accepted, even if no longer in `scaleNotes`. A changed note MUST be a member of the current `scaleNotes`, else the edit MUST be rejected with `note-out-of-scale`. A muted step (`active=false`) MUST keep its note and velocity, and a skipped step MUST keep its note, `active` value and velocity. Skipping or un-skipping MUST be independent of the gate and of velocity. Skipping every step is permitted.

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
- THEN step 4 is not skipped and its note and gate are unchanged

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

### Requirement: Scale-Aware Stepping Notes

The snapshot `scaleNotes` MUST list, ascending, every in-scale MIDI note in `[rangeLow, rangeHigh]` for the current scale and root. Stepping up from a note MUST select the smallest `scaleNote` greater than it; stepping down MUST select the largest smaller one, so an out-of-scale current note snaps in the chosen direction. Stepping MUST stop at the ends (no wrap). The UI MUST step through `scaleNotes` and MUST NOT duplicate scale tables.

#### Scenario: Up and down within scale [V]
- GIVEN `scaleNotes` [48, 50, 51, 53] and a step at 50
- WHEN stepped up, then down twice
- THEN the notes are 51, then 50, then 48

#### Scenario: Out-of-scale note snaps by direction [V]
- GIVEN `scaleNotes` [48, 50, 51] and a step at 49
- WHEN stepped up, and separately down
- THEN up gives 50 and down gives 48

#### Scenario: Ends are clamped [V]
- GIVEN a step at the highest scale note
- WHEN stepped up
- THEN the note is unchanged

#### Scenario: scaleNotes follows scale, root and range [T]
- GIVEN D Dorian, range [40, 64]
- WHEN a snapshot is taken
- THEN `scaleNotes` is ascending, in D Dorian and within [40, 64]

### Requirement: Edited Flag Lifecycle

`edited` MUST become true after a successful `setStep`. Regenerate, randomize and loadPreset MUST clear it. Mutate and auto-evolve MUST keep it. A rejected edit MUST NOT set it.

#### Scenario: Edit sets, regenerate clears [T]
- GIVEN `edited` is false
- WHEN a step edit succeeds, then regenerate runs
- THEN `edited` is true, then false

#### Scenario: Randomize and loadPreset clear it [T]
- GIVEN `edited` is true
- WHEN randomize runs, and separately a preset is loaded
- THEN `edited` is false after each

#### Scenario: Mutate keeps it [T]
- GIVEN `edited` is true
- WHEN mutate or an auto-evolve mutation succeeds
- THEN `edited` remains true

#### Scenario: Rejected edit does not set it [T]
- GIVEN `edited` is false
- WHEN an edit fails with `note-out-of-scale`
- THEN `edited` is false

### Requirement: Edits Are Session State

Edits MUST NOT be stored in presets. MIDI export MUST export the current, edited sequence. Regenerate, randomize and loadPreset MUST discard edits. There is no undo.

#### Scenario: Export reflects edits [T]
- GIVEN a step was muted
- WHEN MIDI is exported
- THEN the muted step produces no note

#### Scenario: Regenerate discards edits [T]
- GIVEN an edited sequence
- WHEN regenerate runs with the same seed
- THEN the sequence equals the seeded original

### Requirement: Keep-Position Edit Playback

A step edit MUST NOT reset the transport or playhead. If the edited step is the one currently sounding and its note, gate or skipped flag changed, the sounding note MUST receive a note-off at adoption. If the edit changes only the velocity of the currently sounding step, it MUST emit no note-off and no retrigger, and the new velocity applies from the next pass. Otherwise no note-off MUST be sent. After adoption the cursor continues from its current index; if that index now lands on a skipped step it advances to the next non-skipped step (wrapping without counting a loop), per `step-event-scheduling`. An edit MUST be rejected with `busy` while a publish is pending or while a state restore is pending; in both cases the busy check comes after index, note and velocity validation.

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

#### Scenario: Velocity plus note change still invalidates [T]
- GIVEN step 5 is sounding
- WHEN its note and velocity are both changed and adopted
- THEN a note-off for the old note is emitted

#### Scenario: Editing another step sends no note-off [T]
- GIVEN step 5 is sounding
- WHEN step 3 is edited (including skipped or velocity) and adopted
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
- WHEN an edit with an out-of-range index, an out-of-scale note or an out-of-range velocity is made
- THEN it fails with `step-out-of-range`, `note-out-of-scale` or `velocity-out-of-range` (checked in that order), not `busy`

### Requirement: Interactive Step Grid

The grid MUST render one cell per step of the current sequence (`steps.length`, up to 64), wrapping into rows of 16. Each cell MUST toggle mute on a click without movement; a vertical drag MUST change the note through `scaleNotes` at a constant pixels-per-note; the wheel MUST step one scale note per notch with a non-passive listener calling `preventDefault`. Each cell MUST also provide a skip toggle that is a distinct control from the active/mute toggle and dispatches `setStep` with `skipped`. Each cell MUST also provide a velocity button, a sibling of the cell (not nested in it), that is part of the roving tabindex, with an aria-label that includes the displayed level name. The displayed level is the nearest of the levels ghost 40, soft 70, normal 100, accent 127 to the step's velocity (ties to the higher level). Activating the button MUST advance to the next level strictly above the displayed level, wrapping accent to ghost, and dispatch `setStep` with the absolute velocity of that level. The cell MUST expose `data-velocity` with the displayed level name. Keyboard MUST use a roving tabindex across all cells: Space toggles mute, Up/Down change the note, Left/Right/Home/End move focus. Each cell MUST have an aria-label like "Step 3, C3, on" (adding ", skipped" when skipped) and `aria-pressed` for the gate; the skip toggle MUST have its own accessible name and `aria-pressed`. Muted steps MUST show their note dimmed; skipped steps MUST be visibly distinct from muted steps. An "edited" indicator MUST show while `edited` is true.

#### Scenario: Click toggles mute [V]
- GIVEN an active step
- WHEN it is clicked without movement
- THEN `setStep` is dispatched with its note, `active:false` and its current velocity

#### Scenario: Skip toggle is distinct from mute [V]
- GIVEN a non-skipped step
- WHEN its skip toggle is activated
- THEN `setStep` is dispatched with the step's note, its current `active`, `skipped:true` and its current velocity, and the mute state does not change

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

#### Scenario: WebView2 input [M]
- GIVEN standalone and plugin in WebView2 with auto-evolve on
- WHEN wheel, drag, click and keyboard are used
- THEN the page does not scroll and the loop keeps its position

### Requirement: Velocity Is Session State

Velocity edits MUST NOT be stored in presets and the preset schema (v3) MUST be unchanged. MIDI export MUST export the current, edited velocities. Regenerate, randomize and loadPreset MUST discard velocity edits (generated steps have velocity 100). There is no undo.

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
