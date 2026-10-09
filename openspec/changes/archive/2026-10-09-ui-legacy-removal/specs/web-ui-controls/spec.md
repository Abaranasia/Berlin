# Delta for web-ui-controls

Purpose line change (non-requirement text): replace "gives full legacy-editor parity through `UiBridge` only" with "provides all engine controls through `UiBridge` only".

Scenario tags: [V] Vitest, [T] BerlinTests, [M] manual, [B] build. Existing scenarios not restated here are unchanged.

## MODIFIED Requirements

### Requirement: Status Line And Error Messages

A status line MUST show the latest outcome. Error tokens MUST map to human-readable messages exactly as in the design's "Status Messages" table (including `exists`, `busy`, `path not absolute`, every `PresetResult` and `MidiFileWriteResult` name). Because `writeFailed` is both a `PresetResult` and a `MidiFileWriteResult` name, the mapping MUST take the command into account (`exportMidi` uses the export write-failure wording, `savePreset`/`loadPreset` the preset wording). Unmapped tokens MUST show the raw token. Error outcomes, including the invalid-seed message, MUST be shown in an error style.

(Previously: "with preset and write failures worded as the legacy `describePresetFailure` / `describeWriteFailure`". The wording itself is unchanged; the reference to the deleted legacy functions is dropped.)

#### Scenario: Token mapping [V]
- GIVEN `loadPreset` returns `fileNotFound`
- WHEN the store handles the failure
- THEN the status shows the mapped message, not the raw token

#### Scenario: Unmapped token [V]
- GIVEN an unknown token `weird`
- WHEN handled
- THEN the status shows `weird`

#### Scenario: Busy [V]
- GIVEN `regenerate` returns `busy`
- WHEN handled
- THEN the status shows `Busy, try again`

#### Scenario: Command-dependent writeFailed [V]
- GIVEN `writeFailed` is returned
- WHEN it comes from `exportMidi`, and separately from `savePreset`
- THEN the status shows "Export failed: could not write the file." and "Could not write the preset file." respectively

### Requirement: Skewed Sliders

Cutoff, Resonance, Attack, Decay, Release and LFO Rate controls (rendered as sliders or as Knobs) MUST use skewed mappings whose midpoint maps to these values: cutoff 1000, resonance 2, attack 0.2, decay 0.3, release 0.5, LFO rate 2. Endpoints MUST map to the protocol limits and the mapping MUST round-trip.

(Previously: "midpoint maps to the legacy values".)

#### Scenario: Midpoint [V]
- GIVEN the cutoff control position 0.5
- WHEN mapped to a value
- THEN it is 1000 (likewise resonance 2, attack 0.2, decay 0.3, release 0.5, LFO rate 2)

#### Scenario: Round trip and endpoints [V]
- GIVEN any value within limits
- WHEN mapped to position and back
- THEN it equals the original within tolerance, with positions 0 and 1 giving min and max

## REMOVED Requirements

### Requirement: Manual Parity Checks (MANUAL)

(Reason: the legacy editor is deleted, so there is nothing to compare against. Parity was verified in Sonar and the standalone before removal. Removes the scenarios "Parity checklist (MANUAL)" and "Dialogs (MANUAL)". Export and overwrite-confirm dialog behavior, including closing the editor mid-dialog, remains covered by the web-editor requirements "Export File Chooser Native Function", "Confirm Native Function" and "Dialog Lifetime Safety".)
