# MIDI Device Selection Specification

## Purpose

Message-thread MIDI output device enumeration and auto-open, a fixed output channel constant with no user-facing selection this phase, an ordered close sequence that keeps the panic guard provably last on the wire, and graceful degradation when no device is available or a device disappears mid-playback.

## Requirements

### Requirement: Auto-Open First Available Device At Startup

The system MUST enumerate available MIDI output devices and, if at least one exists, open the first one and start its background dispatch thread, before audio starts. No device-selection UI exists this phase.

#### Scenario: A device is available at startup

- GIVEN at least one MIDI output device is available
- WHEN the application starts
- THEN the first available device is opened and its background thread is started before audio begins

#### Scenario: No device is available at startup

- GIVEN zero MIDI output devices are available
- WHEN the application starts
- THEN the device stays closed, and the application continues starting without crash

### Requirement: Fixed Output Channel Constant

The system MUST dispatch all MIDI events on a single fixed channel constant. The channel is not user-selectable and not persisted between launches this phase.

#### Scenario: All dispatched events use the fixed channel

- GIVEN the application is running
- WHEN any MIDI event is dispatched
- THEN its channel equals the fixed channel constant, unconditionally

### Requirement: Ordered Close Sequence

The system MUST close a MIDI output device using the exact sequence: discard pending queued messages, then send All Notes Off immediately, then stop the background thread, then release the device — in that order, so a queued future note-on cannot be delivered after the panic guard. Close MUST be idempotent when already closed.

#### Scenario: Panic guard is provably last on the wire

- GIVEN a device with future note-on messages still queued in its background thread
- WHEN the device is closed
- THEN the queued messages are discarded before All Notes Off is sent, so All Notes Off is the last message able to reach the device

#### Scenario: Repeated close calls are no-ops

- GIVEN a device that has already been closed
- WHEN close is called again
- THEN no message is sent, no thread operation repeats, and no crash occurs

### Requirement: Graceful Degradation On No Device, Failed Open, or Removal

The system MUST NOT crash, assert, or hang when zero devices are available, when opening the first available device fails, or when the open device disappears mid-playback. In every such case the system MUST continue running silently, with dispatch to a null or closed device treated as a safe no-op.

#### Scenario: Zero-device launch runs to completion

- GIVEN zero MIDI output devices are available
- WHEN the application runs a full session
- THEN it launches, runs, and exits cleanly with no MIDI output and no crash

#### Scenario: Open failure degrades to silent operation

- GIVEN opening the first available device fails
- WHEN playback proceeds
- THEN dispatch produces no MIDI output and no crash

#### Scenario: Device disappears mid-playback

- GIVEN an open device is removed while playback is running
- WHEN the next block is dispatched
- THEN the dispatch call is a safe no-op with no crash, assert, or hang

### Requirement: Device Lifecycle Confined to the Message Thread

The system MUST perform device open and close only from the message thread, never from the audio callback. The audio thread's only interaction with device state MUST be a null/closed check inside dispatch.

#### Scenario: Audio callback never opens or closes a device

- GIVEN the audio callback's implementation
- WHEN it is inspected during code review
- THEN it contains no device-open or device-close call; only a null/closed check gating dispatch is present
