# Delta for Playback Transport

## ADDED Requirements

### Requirement: Message-Thread-Requested Play/Stop Adopted On The Audio Thread

The system MUST expose a message-thread-safe `setPlaying(bool)` request, mirroring the existing `pendingBpm` atomic handoff, adopted at the start of the next audio-thread process block. `isPlaying()` MUST report the most recently REQUESTED state, not the audio-thread-adopted state, so a UI reflects user intent even when no audio callback is currently running. When an adopted transition stops playback, the system MUST emit any pending note-off through the normal event path on the audio thread before silencing. A request MUST be adopted only on a transition (a stop only while the transport is running, a start only while it is stopped). When an adopted transition resumes playback, it MUST NOT reset to step 1 and MUST NOT replay the interrupted step: playback resumes at the next step boundary after the interrupted step, at that boundary's original grid position (step counter and sample origin unchanged).

#### Scenario: setPlaying(false) silences without a stuck note
- GIVEN a running transport with a note currently sounding
- WHEN `setPlaying(false)` is called on the message thread and the next audio block processes
- THEN the adopted stop emits a note-off through the normal event path before the transport reports stopped

#### Scenario: isPlaying reports the requested state before any block runs
- GIVEN a stopped transport
- WHEN `setPlaying(true)` is called on the message thread and no audio callback has run yet
- THEN `isPlaying()` returns true immediately

#### Scenario: Restart resumes at the next step boundary
- GIVEN a sequence whose steps 5 and 6 are active, and a running transport stopped partway through step 5 (step 5's note-on already emitted, its note-off emitted by the adopted stop)
- WHEN `setPlaying(true)` is called and playback resumes
- THEN the first event emitted is step 6's note-on at step 6's original grid position (offset equal to the remainder of step 5)
- AND the remainder of step 5 is not replayed, and playback does not restart at step 1

#### Scenario: setPlaying introduces no allocation or lock
- GIVEN `setPlaying`'s implementation
- WHEN it is inspected during code review
- THEN it contains no heap allocation, no lock acquisition, and no logging call
