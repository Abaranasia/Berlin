# Delta for MIDI Device Selection

## MODIFIED Requirements

### Requirement: Auto-Open First Available Device At Startup

The system MUST, at startup and before audio starts, enumerate MIDI output devices and open the saved port when it is available, else the first available device, and start its background dispatch thread. A saved explicit `None` MUST leave the device closed. With zero devices the device stays closed. The user may later choose a port or None in Settings (see `standalone-settings`).
(Previously: always opened the first available device; no device-selection UI.)

#### Scenario: Saved port available at startup
- GIVEN a saved port that is in the device list
- WHEN the application starts
- THEN that port is opened and its thread started before audio begins

#### Scenario: Saved port missing falls back to first
- GIVEN a saved port that is not available and at least one other device exists
- WHEN the application starts
- THEN the first available device is opened

#### Scenario: Saved None stays closed
- GIVEN a saved explicit None and devices available
- WHEN the application starts
- THEN no device is opened and startup continues

#### Scenario: No device is available at startup
- GIVEN zero MIDI output devices are available
- WHEN the application starts
- THEN the device stays closed, and the application continues starting without crash

### Requirement: Standalone Output Channel

In standalone, outgoing channel-voice MIDI MUST be dispatched on a user-selected channel 1-16, default 1, owned by `MidiOutputSink` as an atomic value that remaps channel-voice messages at dispatch. The channel MUST persist between launches (see `standalone-settings`). The processor's `MidiEventTranslator`, the plugin's MIDI output and MIDI file export MUST stay on channel 1. All Notes Off (CC123) MUST be sent on the current channel. Channel changes MUST be safe with respect to the audio thread (atomic read, no locks or allocation).
(Previously: "Fixed Output Channel Constant" — a single constant, not user-selectable, not persisted.)

#### Scenario: Events remapped to chosen channel [T]
- GIVEN the standalone channel is 5
- WHEN a note-on authored on channel 1 is dispatched
- THEN the message sent has channel 5

#### Scenario: Default channel [T]
- GIVEN no saved channel
- WHEN any channel-voice event is dispatched
- THEN its channel is 1

#### Scenario: Panic uses current channel [T]
- GIVEN the channel is 7
- WHEN the device is closed
- THEN All Notes Off is sent on channel 7

#### Scenario: Export and plugin unaffected [T]
- GIVEN the standalone channel is 9
- WHEN MIDI is exported or the plugin emits MIDI
- THEN all events are on channel 1

### Requirement: Ordered Close Sequence

The system MUST close a MIDI output device using the exact sequence: discard pending queued messages, then send All Notes Off immediately, then stop the background thread, then release the device — in that order, so a queued future note-on cannot be delivered after the panic guard. Close MUST be idempotent when already closed. The same sequence MUST run when a port is reopened or switched to None: the old device is fully closed in this order before any new device is opened.

#### Scenario: Panic guard is provably last on the wire
- GIVEN a device with future note-on messages still queued in its background thread
- WHEN the device is closed
- THEN the queued messages are discarded before All Notes Off is sent, so All Notes Off is the last message able to reach the device

#### Scenario: Repeated close calls are no-ops
- GIVEN a device that has already been closed
- WHEN close is called again
- THEN no message is sent, no thread operation repeats, and no crash occurs

#### Scenario: Port switch closes old before opening new
- GIVEN port A is open with queued notes
- WHEN Settings applies port B
- THEN A is closed via the ordered sequence, then B is opened, and no queued note from A reaches B

#### Scenario: Switch to None closes
- GIVEN port A is open
- WHEN Settings applies None
- THEN A is closed via the ordered sequence and no device remains open

### Requirement: Device Lifecycle Confined to the Message Thread

The system MUST perform device open, close and reopen only from the message thread, never from the audio callback. A port change MUST occur only while audio is provably stopped (the apply stops audio before and restarts it after). The audio thread's only interaction with device state MUST be a null/closed check inside dispatch plus the atomic channel read.

#### Scenario: Audio callback never opens or closes a device
- GIVEN the audio callback's implementation
- WHEN it is inspected during code review
- THEN it contains no device-open or device-close call; only a null/closed check gating dispatch and an atomic channel read are present

#### Scenario: Reopen bracketed by audio stop [M]
- GIVEN playback is running
- WHEN a new MIDI port is applied
- THEN audio is stopped, the swap occurs on the message thread, and audio restarts without a hung or stuck note

#### Scenario: Failed reopen degrades silently
- GIVEN opening the newly chosen port fails after the old one was closed
- WHEN playback proceeds
- THEN dispatch is a safe no-op with no crash
