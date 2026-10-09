# Standalone Settings Specification

## Purpose

A Settings dialog for the standalone app only: audio device, sample rate, buffer size, MIDI output port (including None), MIDI channel and output level. Apply is transactional, and the chosen setup persists in one versioned file and is restored at launch with safe fallbacks. The plugin has no Settings surface.

Verification legend: [V] Vitest/jsdom, [T] `BerlinTests` (headless, pure logic), [M] manual (standalone with real devices).

## ADDED Requirements

### Requirement: Standalone Capability Detection

The UI MUST treat the host as standalone if and only if `__juce__functions` includes `getAudioSettings`. The Settings button and dialog MUST be available only when standalone. The native functions `getAudioSettings` and `applyAudioSettings` MUST be registered only when the editor is created with a non-null standalone host.

#### Scenario: Standalone detected [V]
- GIVEN `__juce__functions` includes `getAudioSettings`
- WHEN the app mounts
- THEN `isStandalone` is true and the Settings button is enabled

#### Scenario: Plugin not standalone [V]
- GIVEN `__juce__functions` does not include `getAudioSettings`
- WHEN the app mounts
- THEN `isStandalone` is false and no Settings button or dialog exists

#### Scenario: Plugin registers no settings functions [M]
- GIVEN the plugin (VST3/AU) editor
- WHEN its registered native functions are listed
- THEN neither `getAudioSettings` nor `applyAudioSettings` is present

### Requirement: Dialog Contents And Lifecycle

Opening the dialog MUST call `getAudioSettings` and show: audio device type, audio device, sample rate, buffer size, MIDI output port (including a `None` entry), MIDI channel 1-16, and output level with a dB readout. The device list MUST be re-enumerated on every open; there MUST be no hot-plug refresh while open. Sample rate and buffer size options MUST be those of the currently selected device. Cancel (and closing) MUST discard all edits and call nothing. The dialog MUST NOT be offered when `getAudioSettings` itself fails; the failure MUST show an error status.

#### Scenario: Fields populated from current state [V]
- GIVEN `getAudioSettings` returns a setup with device, rate 48000, buffer 256, port "Synth A", channel 3
- WHEN the dialog opens
- THEN each field shows those values and the channel list has 16 entries

#### Scenario: None port is selectable [V]
- GIVEN the returned port list includes None
- WHEN the port control is opened
- THEN a `None` option is present and selectable

#### Scenario: Re-listed on every open [V]
- GIVEN the dialog was opened and closed once
- WHEN it is opened again
- THEN `getAudioSettings` is called again and the new lists are shown

#### Scenario: Cancel discards [V]
- GIVEN the user changed the sample rate and channel
- WHEN Cancel is pressed
- THEN `applyAudioSettings` is not called and reopening shows the original values
- AND the output level is not reverted, because it applies live (see Output Level Reuses Master Level)

#### Scenario: Dependent options follow device [V]
- GIVEN the user selects a different audio device in the dialog
- WHEN the selection changes
- THEN sample rate and buffer size options are those offered for that device

### Requirement: Output Level Reuses Master Level

The dialog's output level MUST be the existing `masterLevel`, shown from the snapshot and changed only through the existing `setMasterLevel` dispatch. It MUST NOT be part of the `applyAudioSettings` request or response, and `getAudioSettings` MUST NOT return a volume. The readout MUST be `20*log10(level)` with one decimal and a unit, and `−∞ dB` when level is 0.

#### Scenario: dB readout [V]
- GIVEN masterLevel 0.5
- WHEN the dialog renders
- THEN the readout is `-6.0 dB`

#### Scenario: Zero level [V]
- GIVEN masterLevel 0
- WHEN the dialog renders
- THEN the readout is `−∞ dB`

#### Scenario: Level change uses dispatch [V]
- GIVEN the dialog is open
- WHEN the level slider is moved
- THEN `dispatch("setMasterLevel", {level})` is called and `applyAudioSettings` is not

#### Scenario: Shared with TopBar [V]
- GIVEN the TopBar master level slider and the dialog slider
- WHEN either is moved
- THEN both reflect the same snapshot `masterLevel`

### Requirement: Transactional Apply

`applyAudioSettings(request)` MUST validate the whole request first and then apply it as a unit. It MUST return `{ ok: true }` on success or `{ ok: false, error: <token> }` on failure, where `<token>` is one of `invalid-request`, `device-unavailable`, `sample-rate-unsupported`, `buffer-size-unsupported`, `midi-port-unavailable`, `busy`. On any `ok:false`, no audio, MIDI port, channel, or persisted state MUST change. Tokens: malformed or out-of-range request fields (including channel outside 1-16) are `invalid-request`; unknown or missing audio device is `device-unavailable`; unsupported rate or buffer for the device are `sample-rate-unsupported` and `buffer-size-unsupported`; unknown MIDI port is `midi-port-unavailable`; an apply already in progress is `busy`. If a device or port fails to open after validation passed, the system MUST attempt a best-effort rollback to the previous device setup, MIDI port and channel, MUST NOT write the settings file, and MUST return `device-unavailable` or `midi-port-unavailable`. After a successful apply the dialog MUST close; after a failure it MUST stay open, show the error, and keep the user's edits.

#### Scenario: Valid apply [V]
- GIVEN valid edits
- WHEN Apply is pressed and the host returns `{ ok: true }`
- THEN the dialog closes

#### Scenario: Rejected apply changes nothing [T]
- GIVEN a request with a valid port but an unsupported buffer size
- WHEN validated
- THEN the result is `buffer-size-unsupported` and device, port, channel and the settings file are unchanged

#### Scenario: Each token is reported [T]
- GIVEN requests that violate exactly one rule each (bad shape, missing device, bad rate, bad buffer, missing port, apply in progress)
- WHEN validated
- THEN the results are `invalid-request`, `device-unavailable`, `sample-rate-unsupported`, `buffer-size-unsupported`, `midi-port-unavailable`, `busy` respectively

#### Scenario: Channel out of range [T]
- GIVEN a request with channel 0 or 17
- WHEN validated
- THEN the result is `invalid-request`

#### Scenario: Error shown, edits kept [V]
- GIVEN the host returns `{ ok: false, error: "device-unavailable" }`
- WHEN Apply is pressed
- THEN the dialog stays open, an error is shown, and the edited values remain

#### Scenario: Hardware apply [M]
- GIVEN a standalone app with two audio devices and two MIDI ports
- WHEN a different device, rate, buffer, port and channel are applied
- THEN audio and MIDI continue on the new setup without a crash

### Requirement: Persistence File

The system MUST persist the standalone setup in one XML file `settings.xml` under `userApplicationDataDirectory/Berlin`, with root element `BerlinSettings` and attribute `version="1"`. It MUST contain the `AudioDeviceManager` state XML, the MIDI port identifier and name (or `none="1"` for an explicit None), and the MIDI channel. It MUST be written after each successful apply. A write failure MUST NOT fail the apply or crash.

#### Scenario: Round trip [T]
- GIVEN a setup with audio state, port id "p1" name "Synth A", channel 5
- WHEN serialized and parsed
- THEN all fields are equal and the root is `BerlinSettings` with `version="1"`

#### Scenario: None serialized explicitly [T]
- GIVEN the port choice is None
- WHEN serialized
- THEN the port element has `none="1"` and no identifier

#### Scenario: Write failure tolerated [T]
- GIVEN the settings directory is not writable
- WHEN a successful apply persists
- THEN the apply still returns `{ ok: true }` and no crash occurs

### Requirement: Launch Restore And Fallback

At launch the system MUST restore the saved setup. A missing, corrupt, wrong-root, or unknown-version file MUST yield defaults without crash. If the saved MIDI port is not available, the system MUST open the first available port, or None if there are none. A saved `None` MUST stay None even if ports exist. A channel outside 1-16 MUST fall back to 1. A saved audio device that cannot be opened MUST fall back to the default audio setup.

#### Scenario: Missing file [T]
- GIVEN no `settings.xml`
- WHEN loaded
- THEN defaults are returned (default audio state, first available port, channel 1)

#### Scenario: Corrupt or unknown version [T]
- GIVEN a file that is not valid XML, has another root, or has `version="2"`
- WHEN loaded
- THEN defaults are returned and no exception escapes

#### Scenario: Saved port present [T]
- GIVEN a saved port identifier that is in the current list
- WHEN resolved
- THEN that port is chosen

#### Scenario: Saved port missing [T]
- GIVEN a saved port not in the current list and other ports exist
- WHEN resolved
- THEN the first available port is chosen; with zero ports the result is None

#### Scenario: Saved None stays None [T]
- GIVEN `none="1"` and ports available
- WHEN resolved
- THEN the result is None

#### Scenario: Saved channel invalid [T]
- GIVEN a saved channel of 99
- WHEN loaded
- THEN the channel is 1

#### Scenario: Relaunch survives [M]
- GIVEN a device, port and channel applied
- WHEN the app is closed and relaunched
- THEN the same setup is active

#### Scenario: Missing device at relaunch [M]
- GIVEN the saved audio device or port is unplugged
- WHEN the app launches
- THEN it starts with the fallback setup and does not crash
