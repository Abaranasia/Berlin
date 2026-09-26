# Delta for MIDI File Output

## MODIFIED Requirements

### Requirement: Tempo Meta-Event From Live Transport BPM

The system MUST write exactly one tempo meta-event, at tick 0, derived from the live/current BPM at the time of export - not a fixed constant - reflecting whatever BPM the user has set via `tempo-control`, even when it differs from the application's default.
(Previously: "Tempo Meta-Event From Transport BPM" - derived from "the transport BPM used to build the timeline," which in practice was always the fixed `kBpm` constant since no runtime BPM mutation existed.)

#### Scenario: Tempo event reflects the transport BPM
- GIVEN a timeline built at a known BPM
- WHEN the file is written
- THEN exactly one tempo meta-event exists at tick 0, encoding that BPM

#### Scenario: Exported tempo reflects a changed BPM, not the default
- GIVEN the user has changed BPM away from the default 120 before exporting
- WHEN the file is written
- THEN the tempo meta-event encodes the current live BPM, not 120 or any other stale/default value
