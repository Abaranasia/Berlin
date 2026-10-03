# Delta for Internal Synth Output

## ADDED Requirements

### Requirement: Live-Adjustable Master Output Level, Smoothed And Applied On Every Patch Path

The system MUST expose a single master output level control in `[0, 1]`, persisted as `SynthPatch::outputLevel`, applied live at the terminal mix with smoothing so changes never click or drop out. Values passed to the setter MUST be clamped to `[0, 1]`. The level MUST be applied live whenever it changes via any path: the dedicated setter, `setPatch`, loading a preset, or restoring plugin state (`setStateInformation`). The default value MUST be 0.8. Master level changes MUST NOT affect MIDI output.

#### Scenario: Out-of-range level clamps and applies without a click
- GIVEN the synth is sounding at the default level
- WHEN `setMasterLevel(1.5)` is called
- THEN the effective level clamps to 1.0 and the new gain is audible within one block with no click or dropout

#### Scenario: Loading a preset applies outputLevel live
- GIVEN a preset whose `outputLevel` differs from the current live level
- WHEN the preset is loaded
- THEN the output gain changes to the preset's `outputLevel` immediately, with no further action required

#### Scenario: Restoring plugin state applies outputLevel live
- GIVEN `setStateInformation` restores a patch with an `outputLevel` different from the current value
- WHEN state restoration completes
- THEN the live gain reflects the restored `outputLevel`

#### Scenario: Master level change does not affect MIDI output
- GIVEN MIDI dispatch is active
- WHEN `setMasterLevel` changes the audio gain
- THEN MIDI message content, timing, and velocity are unaffected

#### Scenario: Default level is 0.8
- GIVEN a freshly constructed patch with no explicit `outputLevel` set
- WHEN the default is inspected
- THEN `outputLevel` equals 0.8

#### Scenario: Master level change introduces no allocation, lock, or logging call
- GIVEN `setMasterLevel`'s audio-thread-side gain application
- WHEN it is inspected during code review
- THEN no heap allocation, lock acquisition, or logging/formatting call is present
