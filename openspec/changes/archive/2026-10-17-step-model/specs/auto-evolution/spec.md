# Delta for Auto-Evolution

## MODIFIED Requirements

### Requirement: Mutation Scheduled by Loop Completions

While Auto-Evolve is enabled, `MainComponent` (a `juce::Timer`) MUST poll `SequencePlayer::getLoopCount()` and, when it has advanced by the selected rate N since the last successfully-triggered mutation, mark that threshold crossing as due and call the existing `mutate()` unchanged as soon as the system is not busy (see the Busy-Collision requirement for the busy case). A "loop" is one pass of the playback cursor over the non-skipped steps (last non-skipped step back to first non-skipped step, per `step-event-scheduling`); skipped steps shorten the loop and a skipped step 0 does not affect counting. When every step is skipped, `loopCount` does not advance, so no automatic mutation becomes due. No new publish path or duplicated mutation logic MUST be introduced.
(Previously: "loop" implicitly meant a pass over all steps; skipped steps did not exist.)

#### Scenario: Automatic mutation fires once per N loops

- GIVEN Auto-Evolve enabled at rate N with no pending busy publish
- WHEN `getLoopCount()` advances by N since the last triggered mutation
- THEN `mutate()` is called exactly once for that threshold crossing

#### Scenario: Skipped steps shorten the loop

- GIVEN Auto-Evolve at rate 2 and a sequence where half the steps are skipped
- WHEN playback runs
- THEN a mutation becomes due after 2 passes over the non-skipped steps, i.e. in half the wall-clock time of the unskipped pattern

#### Scenario: Skipped step 0 does not stall evolution

- GIVEN Auto-Evolve enabled and step 0 skipped
- WHEN playback completes N passes over the non-skipped steps
- THEN a mutation becomes due

#### Scenario: All-skipped sequence never evolves

- GIVEN Auto-Evolve enabled and every step skipped
- WHEN the timer runs for any duration
- THEN `mutate()` is never called automatically and nothing crashes

#### Scenario: Disabling stops further scheduling

- GIVEN Auto-Evolve is enabled and then toggled off before the next threshold is reached
- WHEN additional loops complete after the toggle-off
- THEN no further automatic `mutate()` calls occur, even though `getLoopCount()` keeps advancing
