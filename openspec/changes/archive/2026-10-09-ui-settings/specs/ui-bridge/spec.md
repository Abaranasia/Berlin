# Delta for UI Bridge

## ADDED Requirements

### Requirement: Standalone-Only Settings Functions

When the editor is created with a non-null standalone host, the bridge MUST register two additional native functions, `getAudioSettings` and `applyAudioSettings`; otherwise it MUST NOT register either. They are not `dispatch` commands: the 15-command `dispatch` surface, its result shape and its tokens are unchanged. Both MUST run on the message thread only and MUST NOT call back into JS after the editor is destroyed.

`getAudioSettings` MUST return the current setup and lists: audio device types and devices, sample rates and buffer sizes for the selected device, MIDI output ports (including a None entry), the selected port and the channel 1-16. It MUST NOT return output volume. It MAY take an optional `{ deviceType, device }` query; when given, the sample rates and buffer sizes MUST be those offered by that device, so the dialog can show options for a device that is selected but not yet applied. The None port MUST be represented by the id `""` and listed first as `{ id: "", name: "None" }`.

`applyAudioSettings(request)` MUST return `{ ok: true }` or `{ ok: false, error: <token> }` with the token one of `invalid-request`, `device-unavailable`, `sample-rate-unsupported`, `buffer-size-unsupported`, `midi-port-unavailable`, `busy`, applying transactionally as defined in `standalone-settings`. The request MUST NOT carry output level; level changes use `setMasterLevel`.

#### Scenario: Registered only with a host [T]
- GIVEN an editor built with a non-null standalone host, and another built with null
- WHEN registered functions are listed
- THEN the first includes `getAudioSettings` and `applyAudioSettings`; the second includes neither

#### Scenario: Get returns lists and no volume [V]
- GIVEN the mocked host returns a settings payload
- WHEN `getAudioSettings` is called
- THEN the payload has devices, rates, buffer sizes, ports with None, selected port and channel, and no volume field

#### Scenario: Apply success shape [V]
- GIVEN a valid request
- WHEN `applyAudioSettings` resolves
- THEN the result is `{ ok: true }` with no `snapshot` key

#### Scenario: Apply failure shape [V]
- GIVEN an invalid request
- WHEN `applyAudioSettings` resolves with `{ ok: false, error: "invalid-request" }`
- THEN the JS wrapper exposes that token unchanged

#### Scenario: Dispatch surface unchanged [V]
- GIVEN the bridge command list
- WHEN inspected
- THEN it still contains exactly the 15 existing commands and no settings command

#### Scenario: Callback after teardown [M]
- GIVEN an apply is in flight
- WHEN the editor is destroyed
- THEN no JS callback is made and no crash occurs

### Requirement: Capability-Based Standalone Detection

The JS bridge MUST expose `isStandalone`, true if and only if the host's `__juce__functions` includes `getAudioSettings`. It MUST NOT use user-agent, build flags or any other signal. When the JS bridge is absent (non-JUCE) it MUST be false.

#### Scenario: Detection true [V]
- GIVEN a mocked host whose function list includes `getAudioSettings`
- WHEN `isStandalone` is read
- THEN it is true

#### Scenario: Detection false [V]
- GIVEN a mocked host whose list has `dispatch` but not `getAudioSettings`
- WHEN `isStandalone` is read
- THEN it is false

#### Scenario: No host [V]
- GIVEN no `__juce__functions`
- WHEN `isStandalone` is read
- THEN it is false and no error is thrown
