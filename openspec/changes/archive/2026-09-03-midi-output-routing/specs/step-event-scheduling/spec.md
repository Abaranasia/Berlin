# Delta for Step Event Scheduling

## ADDED Requirements

### Requirement: Flush Pending Note-Off Before Shutdown

The system MUST provide `SequencePlayer::flushPendingNoteOff(StepEventBuffer&)`, which clears the given buffer and pushes at most one note-off event for any currently-sounding note (tracked via the player's pending-note state), then clears that pending-note state. It MUST be idempotent: a second call with no newly-sounding note in between emits nothing. It MUST be called before `reset()`, since `reset()` discards the pending-note state; `stop()` does not clear it, so flushing after `stop()` also emits correctly.

The only production call site MUST be `MainComponent::releaseResources()`, on the shutdown/device-restart path. Neither `stop()` nor `reset()` gains a sink parameter, and neither emits anything itself — both signatures stay unchanged.

#### Scenario: Flush emits a note-off for a sounding note

- GIVEN a `SequencePlayer` currently sounding a note
- WHEN `flushPendingNoteOff` is called with an empty `StepEventBuffer`
- THEN exactly one note-off event for that note is pushed at sample offset 0, and the pending-note state is cleared

#### Scenario: Flush is a no-op when nothing is sounding

- GIVEN a `SequencePlayer` with no currently-sounding note
- WHEN `flushPendingNoteOff` is called
- THEN no event is pushed and the buffer ends up cleared

#### Scenario: Flush is idempotent

- GIVEN `flushPendingNoteOff` was just called and emitted a note-off
- WHEN it is called again with no further processing in between
- THEN the second call emits nothing

#### Scenario: Ordering against reset determines whether the note-off survives

- GIVEN a sounding note
- WHEN `flushPendingNoteOff` is called before `reset()`
- THEN the note-off is emitted
- AND WHEN, in a separate case, `reset()` is called first and `flushPendingNoteOff` is called after
- THEN no note-off is emitted, because `reset()` already discarded the pending-note state

#### Scenario: No unmatched note-on across repeated start/stop cycles

- GIVEN repeated cycles of starting playback, processing several blocks, and stopping — including a stop that lands mid-step and a stop with nothing sounding
- WHEN `flushPendingNoteOff` is called at the end of each cycle before any `reset()`
- THEN every note-on emitted during that cycle has a matching note-off, with none left hanging
