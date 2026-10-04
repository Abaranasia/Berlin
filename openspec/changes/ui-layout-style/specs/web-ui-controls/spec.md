# Delta for Web UI Controls

## MODIFIED Requirements

### Requirement: Skewed Sliders

Cutoff, Resonance, Attack, Decay, Release and LFO Rate controls (rendered as sliders or as Knobs) MUST use skewed mappings whose midpoint maps to the legacy values: cutoff 1000, resonance 2, attack 0.2, decay 0.3, release 0.5, LFO rate 2. Endpoints MUST map to the protocol limits and the mapping MUST round-trip.
(Previously: applied to sliders only.)

#### Scenario: Midpoint
- GIVEN the cutoff control position 0.5
- WHEN mapped to a value
- THEN it is 1000 (likewise resonance 2, attack 0.2, decay 0.3, release 0.5, LFO rate 2)

#### Scenario: Round trip and endpoints
- GIVEN any value within limits
- WHEN mapped to position and back
- THEN it equals the original within tolerance, with positions 0 and 1 giving min and max

### Requirement: Delay Recommendations

The UI MUST compute delay recommendations in TS from BPM: beat ms = 60000/bpm; factors 2, 1, 0.75, 0.5, 1/3, 0.25 with labels `1/2, 1/4, 1/8., 1/8, 1/8T, 1/16`; each value rounded to whole ms. Each recommendation MUST be shown as a clickable button labelled `<label> <ms> ms` that copies its ms value, as specified by capability `web-ui-layout` (Delay-Times Card).
(Previously: shown as small text under `data-role="delay-recommendations"`.)

#### Scenario: At 120 BPM
- GIVEN BPM 120
- WHEN recommendations are computed
- THEN they are 1/2=1000, 1/4=500, 1/8.=375, 1/8=250, 1/8T=167, 1/16=125 ms

#### Scenario: Updates with BPM
- GIVEN BPM changes to 150
- WHEN recomputed
- THEN 1/4 shows 400 ms

#### Scenario: Rendered as buttons [V]
- GIVEN BPM 120
- WHEN the card renders
- THEN six buttons exist, including "1/4 500 ms", and no `data-role="delay-recommendations"` text remains
