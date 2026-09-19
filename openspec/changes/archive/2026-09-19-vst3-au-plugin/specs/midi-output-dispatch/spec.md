# Delta for MIDI Output Dispatch

## MODIFIED Requirements

### Requirement: Named, Bounded Lock Exception in Dispatch

The system's use of `juce::MidiOutput::sendBlockOfMessages` from the audio thread MAY acquire that call's short internal lock, on the standalone path only, where `MidiOutputSink` dispatches to an OS MIDI device. This MUST be the only lock acquisition permitted anywhere in that callback's path, and MUST remain bounded to list-append work with no I/O or unbounded-size allocation. The plugin path MUST NOT use `sendBlockOfMessages` or any OS `MidiOutput` device at all; it writes translated events directly into the host's `MidiBuffer&` with no lock (see `plugin-host-integration`).

(Previously: described the lock exception without scoping it to standalone-only; the plugin path did not exist.)

#### Scenario: Exactly one documented lock site exists on the standalone path

- GIVEN the standalone audio callback's full call path for one block
- WHEN it is inspected during code review
- THEN the only lock acquisition found is the documented `sendBlockOfMessages` call, and no other lock exists anywhere in the path

#### Scenario: Plugin path has no MIDI-dispatch lock

- GIVEN the plugin `processBlock`'s full call path for one block
- WHEN it is inspected during code review
- THEN no `sendBlockOfMessages` call or OS `MidiOutput` lock exists, because MIDI is written directly into the host buffer

### Requirement: All-Notes-Off Panic Guard On Close

The system MUST send an All Notes Off (CC 123) message on the configured channel when closing an open `MidiOutputSink` OS device, independent of and in addition to any application-tracked note-off. Closing an already-closed device MUST be a no-op. This requirement applies to the standalone `MidiOutputSink` path only; the plugin path has no OS device to close and MUST NOT attempt to send this guard message (the host owns note-off responsibility for its own MIDI routing).

(Previously: unscoped; now explicit that this applies to the standalone `MidiOutputSink` device path only, since the plugin never opens a device.)

#### Scenario: Closing an open device sends the panic guard

- GIVEN an open MIDI output device on the standalone path
- WHEN the device is closed
- THEN a CC 123 All Notes Off message on the configured channel is sent before the close completes

#### Scenario: Closing an already-closed device is idempotent

- GIVEN a device that is already closed
- WHEN close is invoked again
- THEN no message is sent and no crash occurs
