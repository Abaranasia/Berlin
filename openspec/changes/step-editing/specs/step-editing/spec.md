# Step Editing Specification

## Purpose

Per-step mute and note editing of the live sequence, limited to the visible 16 steps, with scale-aware note stepping, an `edited` flag, and keep-position playback. Verification legend: [V] Vitest, [T] BerlinTests, [M] manual, [B] build.

## Requirements

### Requirement: Step Index Limit

The engine MUST accept an edit only for an index in `[0, min(size, 16))`, where `size` is the live sequence size. Otherwise it MUST reject with `step-out-of-range` and leave all state unchanged.

#### Scenario: Valid index edits the step [T]
- GIVEN a 16-step sequence
- WHEN step 15 is edited with an in-scale note
- THEN the edit succeeds and only step 15 differs

#### Scenario: Index beyond 16 or size is rejected [T]
- GIVEN a 32-step sequence, and separately an 8-step sequence
- WHEN index 16 is edited on the first, and index 8 on the second
- THEN both fail with `step-out-of-range` and the sequence is unchanged

#### Scenario: Negative index is rejected [T]
- GIVEN any sequence
- WHEN index -1 is edited
- THEN the edit fails with `step-out-of-range`

### Requirement: Absolute Note and Gate Assignment

A step edit MUST set the step's note and gate to the supplied absolute values. The note MUST be a member of the current `scaleNotes`, else the edit MUST be rejected with `note-out-of-scale`. A muted step (`active=false`) MUST keep its note.

#### Scenario: Mute keeps the note [T]
- GIVEN step 3 is active with note 48
- WHEN it is edited with note 48, active false
- THEN the step is inactive and its note remains 48

#### Scenario: Out-of-scale note is rejected [T]
- GIVEN C minor and a note not in `scaleNotes`
- WHEN a step is edited with that note
- THEN the edit fails with `note-out-of-scale` and the step is unchanged

#### Scenario: Idempotent re-send [T]
- GIVEN an edit has succeeded
- WHEN the same absolute values are sent again
- THEN the sequence is identical

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

A step edit MUST NOT reset the transport or playhead. If the edited step is the one currently sounding and its note or gate changed, the sounding note MUST receive a note-off at adoption. Otherwise no note-off MUST be sent.

#### Scenario: Playhead continues after edit [T]
- GIVEN playback is at step 9
- WHEN step 3 is edited and adopted
- THEN the playhead continues from step 9 and loop count is unchanged

#### Scenario: Editing the sounding step sends note-off [T]
- GIVEN step 5 is sounding
- WHEN step 5 is muted or its note changed, then adopted
- THEN a note-off for the old note is emitted

#### Scenario: Editing another step sends no note-off [T]
- GIVEN step 5 is sounding
- WHEN step 3 is edited and adopted
- THEN no note-off is emitted for the sounding note

#### Scenario: Busy rejects the edit [T]
- GIVEN a publish is pending
- WHEN a valid step edit is made
- THEN it fails with `busy` and state is unchanged

#### Scenario: Validation precedes busy [T]
- GIVEN a publish is pending
- WHEN an edit with an out-of-range index or an out-of-scale note is made
- THEN it fails with `step-out-of-range` or `note-out-of-scale` (checked in that order), not `busy`, so the UI only retries edits that can succeed

### Requirement: Interactive Step Grid

Each cell MUST toggle mute on a click without movement; a vertical drag MUST change the note through `scaleNotes` at a constant pixels-per-note; the wheel MUST step one scale note per notch with a non-passive listener calling `preventDefault`. Keyboard MUST use a roving tabindex: Space toggles mute, Up/Down change the note, Left/Right/Home/End move focus. Each cell MUST have an aria-label like "Step 3, C3, on" and `aria-pressed` for the gate. Muted steps MUST show their note dimmed. An "edited" indicator MUST show while `edited` is true.

#### Scenario: Click toggles mute [V]
- GIVEN an active step
- WHEN it is clicked without movement
- THEN `setStep` is dispatched with its note and `active:false`

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

#### Scenario: Edited indicator [V]
- GIVEN `edited` true, then false
- WHEN rendered
- THEN the indicator is shown, then hidden

#### Scenario: WebView2 input [M]
- GIVEN standalone and plugin in WebView2 with auto-evolve on
- WHEN wheel, drag, click and keyboard are used
- THEN the page does not scroll and the loop keeps its position
