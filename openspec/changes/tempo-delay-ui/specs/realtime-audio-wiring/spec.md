# Delta for Realtime Audio Wiring

## ADDED Requirements

### Requirement: Tempo And Effects Parameter Changes Reach The Audio Thread Without Allocation, Lock, Or Logging

The system MUST propagate BPM changes (`tempo-control`, `playback-transport`) and delay/reverb parameter changes (`internal-synth-output`) from the message thread into the audio callback (`getNextAudioBlock`/`processBlock`) using only atomic reads on the per-block path - no heap allocation, no lock acquisition beyond the already-documented `sendBlockOfMessages` exception (which this path does not use), and no logging/formatting call.

#### Scenario: BPM publish path is allocation-free and lock-free inside the callback
- GIVEN a BPM change published from the message thread
- WHEN the audio callback reads it on the next block
- THEN no heap allocation and no lock acquisition occurs in that read

#### Scenario: Delay/reverb parameter publish path is allocation-free and lock-free inside the callback
- GIVEN a delay or reverb parameter change published from the message thread
- WHEN the audio callback applies it at the start of the next block
- THEN no heap allocation and no lock acquisition occurs in that application
