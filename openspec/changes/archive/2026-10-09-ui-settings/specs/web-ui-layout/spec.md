# Delta for Web UI Layout

## MODIFIED Requirements

### Requirement: Settings Button

TopBar MUST show a button with aria-label "Settings" only when the host is standalone (capability detection per `ui-bridge`). In standalone it MUST be enabled and pressing it MUST open the Settings dialog (see `standalone-settings`) without dispatching any command. In the plugin the button MUST NOT be rendered. The shell region list MUST read "settings button" where it previously read "settings placeholder".
(Previously: "Settings Placeholder" — a disabled button that dispatched nothing, always present.)

#### Scenario: Enabled in standalone [V]
- GIVEN `__juce__functions` includes `getAudioSettings`
- WHEN the "Settings" button is queried
- THEN it is enabled and pressing it opens the dialog with no `dispatch` call

#### Scenario: Hidden in plugin [V]
- GIVEN `__juce__functions` lacks `getAudioSettings`
- WHEN the app is rendered
- THEN no element with aria-label "Settings" exists

#### Scenario: Dialog keyboard access [V]
- GIVEN the dialog is open
- WHEN Escape is pressed
- THEN the dialog closes as Cancel and focus returns to the Settings button
