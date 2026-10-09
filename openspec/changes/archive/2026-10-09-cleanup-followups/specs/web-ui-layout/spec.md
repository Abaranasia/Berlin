# Delta for Web UI Layout

Verification legend: [V] Vitest/jsdom, [M] manual (layout, resize, clipboard in standalone and Cakewalk Sonar), [B] build.

## MODIFIED Requirements

### Requirement: Knob Control

Synth panel range controls MUST render as `Knob`, wrapping a visually hidden native range input. A Knob MUST keep the aria-label, min/max/step, disabled state and dispatch of the Slider it replaces. Skewed controls MUST use the existing `SKEWS` mapping. Pointer drag MUST be vertical, 200 px of travel being the full range. Keyboard MUST work through the native input. Wheel and double-click reset MUST NOT be supported. A pure helper `ui/src/lib/knob.ts` MUST implement the drag math. A pointer move with no pressed button (`buttons === 0`) or on a disabled Knob MUST end any drag in progress and dispatch nothing. Losing pointer capture (`lostpointercapture`) MUST end the drag.
(Previously: a move was not checked for pressed buttons or disabled state, and lost pointer capture did not end the drag, so a missed pointer-up could leave the Knob tracking a button-less pointer.)

#### Scenario: Drag math [V]
- GIVEN range 0..1 at 0.5
- WHEN dragged up 100 px
- THEN the helper returns 1.0, clamped to the max, and 200 px up from min gives max

#### Scenario: Skewed knob [V]
- GIVEN the cutoff Knob at midpoint
- WHEN read
- THEN it maps to 1000 Hz via `SKEWS`

#### Scenario: Keyboard [V]
- GIVEN a focused Knob
- WHEN ArrowRight is pressed
- THEN `setPatch` is dispatched with the next step value

#### Scenario: Disabled [V]
- GIVEN FX off
- WHEN a delay Knob is rendered
- THEN its input is disabled and drag dispatches nothing

#### Scenario: No wheel or reset [V]
- GIVEN a Knob
- WHEN a wheel event or double-click occurs
- THEN the value does not change

#### Scenario: Pointer feel [M]
- GIVEN the editor in the host
- WHEN a Knob is dragged
- THEN it tracks the pointer smoothly

#### Scenario: Button-less move ends the drag [V]
- GIVEN a drag in progress on an enabled Knob
- WHEN a pointer move arrives with `buttons === 0`
- THEN nothing is dispatched and the drag has ended, so a following move with `buttons === 1` and no new pointer-down also dispatches nothing

#### Scenario: Move on a disabled Knob dispatches nothing [V]
- GIVEN a Knob that is disabled while pointer events are fired at it
- WHEN a pointer-down and a pointer move with `buttons === 1` occur
- THEN nothing is dispatched and no drag is active

#### Scenario: Lost pointer capture ends the drag [V]
- GIVEN a drag in progress
- WHEN `lostpointercapture` fires
- THEN the drag ends and a following move with `buttons === 1` dispatches nothing

#### Scenario: Normal drag is unaffected [V]
- GIVEN an enabled Knob
- WHEN pointer-down, then moves with `buttons === 1`, then pointer-up occur
- THEN the drag dispatches values as before
