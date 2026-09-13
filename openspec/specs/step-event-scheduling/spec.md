# Step Event Scheduling Specification

## Purpose

A project-local, non-`juce::MidiMessage` event type and a fixed-capacity, non-allocating sink for it, plus the rules `SequencePlayer` uses to turn `Transport` boundaries and a `Sequence` into ordered, correctly-timed note-off/note-on events — including full-step gating, inactive/empty-sequence suppression, and continuity across the loop wrap.

## Requirements

### Requirement: Project-Local StepEvent Type

The system MUST provide a `StepEvent` value type with fields `{ sampleOffset, stepIndex, note, isNoteOn }`. The system MUST NOT use `juce::MidiMessage`, `juce::MidiBuffer`, or any type from `juce_audio_basics`.

#### Scenario: Construct a StepEvent

- GIVEN values for `sampleOffset`, `stepIndex`, `note`, and `isNoteOn`
- WHEN a `StepEvent` is constructed with them
- THEN each field reads back the given value

#### Scenario: No MIDI-library dependency

- GIVEN the `StepEvent` type definition and its translation unit's includes
- WHEN inspecting its dependencies
- THEN no `juce::MidiMessage`, `juce::MidiBuffer`, or `juce_audio_basics` symbol is referenced

### Requirement: Fixed-Capacity, Non-Allocating Event Sink

The system MUST provide a `StepEventBuffer` with fixed capacity fixed at construction/compile time. Pushing an event MUST NOT allocate heap memory, lock, or block. On overflow, the system MUST drop the excess event and record it via a flag/counter rather than growing, blocking, or crashing.

#### Scenario: Push within capacity retains order

- GIVEN a `StepEventBuffer` with unused capacity
- WHEN events are pushed within that capacity
- THEN all pushed events are retained in the order they were pushed

#### Scenario: Push beyond capacity drops and flags, never allocates

- GIVEN a `StepEventBuffer` already at full capacity
- WHEN one more event is pushed
- THEN the buffer's stored events are unchanged, the overflow flag/counter increments, and no heap allocation occurs

### Requirement: Note-Off Before Note-On At a Shared Boundary

Because `Step` has no gate/duration field, a note MUST sound for its full step: note-off MUST be emitted at the next step boundary. When that note-off and the following step's note-on fall at the same sample offset, the system MUST order the note-off before the note-on.

#### Scenario: Coincident boundary orders note-off first

- GIVEN an active step followed immediately by another active step
- WHEN `SequencePlayer` processes across their shared boundary
- THEN the emitted note-off for the ending step appears before the note-on for the starting step at that sample offset

### Requirement: Inactive Steps and Empty Sequence Emit Nothing

The system MUST emit no note-on/note-off events for a step whose `active` flag is false. An empty `Sequence` (zero steps), or a `Sequence` where every step is inactive, MUST produce no events at all.

#### Scenario: Inactive step is silent

- GIVEN a step with `active = false`
- WHEN `SequencePlayer` processes the boundary containing that step
- THEN no note-on or note-off event is emitted for it

#### Scenario: Empty or fully-inactive sequence emits nothing

- GIVEN a `Sequence` with zero steps, or with every step inactive
- WHEN `SequencePlayer` processes any number of blocks
- THEN no events are emitted

### Requirement: Continuous Emission Across the Loop Wrap

The system MUST continue emitting correctly-timed events across the loop boundary: the last step's note-off (if any) and the wrap to step 0 MUST occur with no timing gap and no duplicated or dropped event.

#### Scenario: Wrap boundary has no gap or duplicate

- GIVEN a `Sequence` of N steps being processed past its final step
- WHEN `SequencePlayer` processes the boundary between step N-1 and step 0
- THEN any pending note-off from step N-1 and step 0's note-on (if active) are emitted at their correct, contiguous sample offsets with no gap and no duplicate event

### Requirement: Step Position Wraps Indefinitely

The system MUST expose, via `SequencePlayer`, a step position (playhead) that increases with each `Transport` boundary and wraps back to 0 upon completing `Sequence::size()` steps, continuing indefinitely rather than stopping after one pass. `SequencePlayer` combines `Transport`'s boundary count with the driving `Sequence`'s length to compute this wrap; `Transport` itself has no loop-length concept (see `playback-transport`'s "Unbounded Boundary Counting" requirement).

#### Scenario: Step position wraps at loop end

- GIVEN a `Sequence` of N steps and a running `SequencePlayer`
- WHEN the playhead reaches step N-1 and one more boundary is crossed
- THEN the reported next step position is 0, with no gap or skipped boundary

### Requirement: Emitted Notes Match the Seeded Sequence

The system MUST emit note values, in order, matching the `note` field of each active step in the driving `Sequence`, verified by simulated processing over many blocks including irregular block sizes and blocks shorter than one step.

#### Scenario: Emitted notes match across irregular block sizes

- GIVEN a seeded `Sequence` and a series of `SequencePlayer::process` calls using irregular, varying block sizes (including some shorter than one step)
- WHEN all emitted `StepEvent`s are collected in order
- THEN each note-on's `note` value equals the corresponding active step's `note` field, in sequence order

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

### Requirement: Loop Completion Counter

The system MUST expose, via `SequencePlayer`, an RT-safe `std::atomic<int> loopCount` and `getLoopCount() const noexcept`, mirroring the existing `playhead`/`getPlayheadStep()` cross-thread pattern exactly (audio thread writes via a relaxed atomic store inside `process()`; message thread reads via the getter — no lock, no allocation). `loopCount` MUST increment exactly once per genuine loop completion, defined as the step position wrapping from a non-zero step back to 0 during normal playback advancement. `loopCount` MUST NOT increment when the step position is set to 0 by `reset()` (e.g., on publish-adopt), since that is not a loop completion. `loopCount` MUST remain monotonic — it is never reset or decremented by any operation in this change, including publish-adopt.

#### Scenario: Loop completion increments the counter

- GIVEN a running `SequencePlayer` processing a `Sequence` of N steps
- WHEN the step position wraps from step N-1 back to step 0 during normal advancement
- THEN `getLoopCount()` reflects exactly one additional completed loop

#### Scenario: Publish-triggered reset does not count as a loop completion

- GIVEN a running `SequencePlayer` with `loopCount` at value V, mid-loop at a non-zero step
- WHEN a new sequence is published and adopted, calling `reset()` and setting the step position to 0
- THEN `getLoopCount()` remains at V, unchanged by the reset

#### Scenario: Counter stays exact across many wraps, including around publishes

- GIVEN a `SequencePlayer` processed across many loop wraps interspersed with occasional publish-adopts
- WHEN `getLoopCount()` is inspected after each wrap and after each adopt
- THEN it increases by exactly one per genuine loop completion only, with no missed or double counts

#### Scenario: Device restart does not fabricate a loop completion

- GIVEN a running `SequencePlayer` mid-loop at a non-zero step, with `loopCount` at value V
- WHEN `prepare()` is called (e.g. on audio device restart) and playback then advances past the first step-0 boundary
- THEN `getLoopCount()` is still exactly V + 1 for that one genuine boundary — not V + 2 — i.e. `prepare()` itself must not leave stale wrap-detection state that fabricates an extra completion

### Requirement: Publish-Pending Observability

The system MUST expose, via `SequencePlayer`, an RT-safe `bool isPublishPending() const noexcept` that reports whether a published sequence is currently awaiting adoption by the audio thread, readable from the message thread with no lock or allocation. This lets a caller check busy state before attempting a publish-dependent action, rather than only discovering busy state from that action's own rejection.

#### Scenario: Reflects pending state accurately across the handoff

- GIVEN a `SequencePlayer` with no pending publish
- WHEN a sequence is published via `publishSequence()`
- THEN `isPublishPending()` returns true until the audio thread adopts it, and false once adopted
