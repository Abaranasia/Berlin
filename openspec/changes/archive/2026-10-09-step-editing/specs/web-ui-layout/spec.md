# Delta for Web UI Layout

## MODIFIED Requirements

### Requirement: Step View

MainArea MUST show 16 steps, each with step number, gate LED, and note name. The playhead step MUST be highlighted and gated steps MUST be visually distinct from rests. Display MUST be driven only by the snapshot, optimistic `step.<i>` values and `playhead` events. Cells MUST be interactive as defined in capability `step-editing` (click mute, drag and wheel note change, roving-tabindex keyboard, aria-label and `aria-pressed`). Muted steps MUST show their note dimmed. A small "edited" indicator MUST show while `snapshot.edited` is true. Interactive targets keep the 44 px minimum.
(Previously: read-only, driven only by the snapshot and `playhead` events; no handlers, dimming or indicator.)

#### Scenario: Gate distinction [V]
- GIVEN a snapshot with step 1 gated (MIDI 48; naming convention MIDI 60 = C4) and step 2 rest
- WHEN rendered
- THEN step 1 shows its gate LED on and "C3"; step 2 shows the LED off

#### Scenario: Playhead [V]
- GIVEN a `playhead {step:4}` event
- WHEN received
- THEN only step 4 is highlighted

#### Scenario: Muted note dimmed [V]
- GIVEN step 2 is a rest with note 50
- WHEN rendered
- THEN its note text "D3" is shown with the dimmed style

#### Scenario: Edited indicator follows snapshot [V]
- GIVEN a snapshot with `edited:true`, then a snapshot event with `edited:false`
- WHEN rendered
- THEN the indicator is shown, then removed

#### Scenario: Focus ring on cells [M]
- GIVEN keyboard focus on a step cell
- WHEN focused
- THEN the accent focus ring is visible
