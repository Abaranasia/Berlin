# Delay-Time Recommendation Specification

## Purpose

Read-only, at-a-glance display of the delay time every `SyncDivision` would
produce at the current BPM, for users of external delay/reverb units who
cannot read that value off Berlin's own synced-division picker. Consumes
`tempo-control`'s BPM and `(bpm, division) -> seconds` conversion unchanged;
owns only the ms-rounding/formatting rule and the display's refresh and
visibility contract.

## Requirements

### Requirement: Multi-Division Delay-Time Display

The system MUST display the delay time for all 6 `SyncDivision` values
simultaneously, in enum order (half, quarter, dottedEighth, eighth,
eighthTriplet, sixteenth - labels "1/2", "1/4", "1/8.", "1/8", "1/8T",
"1/16"). Each division MUST be presented with equal visual weight; no
division MAY be marked, highlighted, or labelled as the "correct" or
"recommended" one.

#### Scenario: All divisions visible at once
- GIVEN the editor is open at any BPM
- WHEN the delay-time recommendation display is inspected
- THEN all 6 divisions are shown together, in enum order, each with its own
  computed time

#### Scenario: No division is singled out
- GIVEN the delay-time recommendation display
- WHEN its presentation is inspected
- THEN no division carries a "recommended"/"correct" marker and all 6 use
  the same visual treatment

### Requirement: Millisecond Rounding and Formatting

The system MUST format each division's delay time as a whole-millisecond
integer computed as `std::lround(delaySecondsFor(bpm, division) * 1000.0)`
(half away from zero). A value of 1000 ms or greater MUST remain expressed
in milliseconds (e.g. "3000 ms"), never converted to seconds.

#### Scenario: Exact values at 160 BPM
- GIVEN BPM = 160
- WHEN the recommendation display is computed
- THEN the 6 divisions read 750, 375, 281, 188, 125, 94 ms respectively

#### Scenario: Exact values at 120 BPM
- GIVEN BPM = 120
- WHEN the recommendation display is computed
- THEN the 6 divisions read 1000, 500, 375, 250, 167, 125 ms respectively

#### Scenario: Values at or above 1000 ms stay in milliseconds
- GIVEN BPM = 40 and division = half
- WHEN the recommendation display is computed
- THEN that division reads "3000 ms", not a seconds value

#### Scenario: Upper BPM bound produces valid sub-second values
- GIVEN BPM = 240 (the maximum allowed)
- WHEN the recommendation display is computed
- THEN the 6 divisions read 500, 250, 188, 125, 83, 63 ms respectively, with
  no negative or overflowing value

#### Scenario: Non-positive BPM guard yields zero
- GIVEN `delaySecondsFor` is invoked with bpm <= 0 (its documented guard
  case, returning 0 seconds)
- WHEN that result is formatted
- THEN every affected division's displayed time is 0 ms

### Requirement: Event-Driven Refresh on the Message Thread

The system MUST refresh the recommendation display on every event that can
change the displayed BPM: at editor construction (using the initial BPM),
on each BPM change delivered through the tempo-slider path
(`pushTempoFromWidgets()`), and whenever the editor re-syncs from the
processor after a BPM restore (preset load or `setStateInformation`, i.e.
`refreshFromProcessor()`). BPM MUST be read on the message thread only. The
system MUST NOT use a `juce::Timer`, polling, or any background thread, and
MUST NOT introduce an audio-thread change or a preset/schema change.

#### Scenario: Initial display reflects the starting BPM
- GIVEN the editor has just been constructed with the processor's current
  BPM
- WHEN the recommendation display is first shown
- THEN its values match that starting BPM, with no user action taken

#### Scenario: Display refreshes after a BPM change
- GIVEN the editor is open and showing recommendations for the prior BPM
- WHEN the user changes BPM via the tempo slider
- THEN the display recomputes and shows the new BPM's values with no
  additional user action

#### Scenario: Display refreshes after a preset or state restores BPM
- GIVEN the editor is open showing recommendations for 120 BPM
- WHEN a preset (or saved plugin state) with 160 BPM is loaded and the
  editor re-syncs from the processor
- THEN the display shows the 160 BPM values (750/375/281/188/125/94 ms),
  even though the tempo slider was updated without sending a notification

#### Scenario: No timer or audio-thread involvement
- GIVEN the refresh path for the recommendation display
- WHEN it is inspected during code review
- THEN it contains no `juce::Timer`, no polling loop, no audio-thread
  change, and no preset/schema change

### Requirement: Visibility Independent of FX Toggle and Sync/Free Mode

The system MUST keep the recommendation display visible and current
regardless of the internal-FX toggle's state and regardless of whether the
delay Sync/Free toggle is set to Sync or Free.

#### Scenario: Display stays visible with internal FX off
- GIVEN the internal-FX toggle is switched off
- WHEN the recommendation display is inspected
- THEN it remains visible and shows correct values for the current BPM

#### Scenario: Display stays visible in Free mode
- GIVEN the delay Sync/Free toggle is set to Free
- WHEN the recommendation display is inspected
- THEN it remains visible and shows correct values for the current BPM,
  independent of the manually-set delay time
