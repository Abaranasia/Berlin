# Web UI Layout Specification

## Purpose

Visual shell of the React UI: theme tokens, bundled fonts, regions, responsive rules, collapsible synth panel, Knob, step view, delay-times card and settings placeholder. No bridge protocol changes.

Verification legend: [V] Vitest/jsdom, [M] manual (layout, resize, clipboard in standalone and Cakewalk Sonar).

## Requirements

### Requirement: Theme Tokens

Styling MUST come from CSS custom properties on `:root`: ground, surfaces, borders, text, muted, accent `#F0A238`, accent-hover, on-accent, radii (6/8/12 px) and a focus ring. Interactive targets MUST be at least 44 px. The inline `<style>` in `index.html` MUST be removed.

#### Scenario: Tokens defined [V]
- GIVEN the built stylesheet
- WHEN `:root` is inspected
- THEN every listed token is defined and `--accent` equals `#F0A238`

#### Scenario: Focus ring visible [M]
- GIVEN keyboard focus on any control
- WHEN it is focused
- THEN an accent focus ring is visible

### Requirement: Bundled Offline Fonts

IBM Plex Sans Condensed and IBM Plex Mono (latin subset) MUST be bundled in the embedded assets with the OFL license text shipped. The UI MUST NOT perform any network font load.

#### Scenario: No network fonts [V]
- GIVEN the production build output
- WHEN stylesheets are scanned
- THEN every `@font-face` URL is a local asset and no `http(s)` font URL exists

#### Scenario: Offline rendering [M]
- GIVEN no network access
- WHEN the editor loads
- THEN text renders in IBM Plex

### Requirement: Shell Regions

`App` MUST render three regions. TopBar: wordmark, Play/Stop, BPM, preset select/name/Load/Save, Export MIDI, master level, synth-panel toggle, settings button. MainArea: step view, Generation, Rhythm, Pitch, Evolution and the delay-times card. SynthPanel: Sound/FX toggles, oscillator, filter and envelope, LFO, delay, reverb. All existing controls MUST keep their labels and dispatches.

#### Scenario: Controls in regions [V]
- GIVEN the app is rendered
- WHEN regions are queried
- THEN master level is in TopBar, step view in MainArea, and FX toggle and delay/reverb controls in SynthPanel

#### Scenario: Parity preserved [V]
- GIVEN the Slice 2 control tests
- WHEN run against the shell
- THEN they pass with unchanged labels and dispatches

### Requirement: Settings Button

TopBar MUST show a button with aria-label "Settings" only when the host is standalone (capability detection per `ui-bridge`). In standalone it MUST be enabled and pressing it MUST open the Settings dialog (see `standalone-settings`) without dispatching any command. In the plugin the button MUST NOT be rendered. The shell region list MUST read "settings button" where it previously read "settings placeholder".

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

### Requirement: Collapsible Synth Panel With Persistence

The synth panel MUST stay mounted when collapsed, hidden via the HTML `hidden` attribute on its container. Open state MUST persist in `localStorage` key `berlin.ui.synthPanelOpen` ("1" open, "0" closed), default closed. Storage read or write failure MUST fall back to closed silently, without a status error.

#### Scenario: Default closed [V]
- GIVEN no stored value
- WHEN the app mounts
- THEN the panel container has `hidden` and its labeled controls remain in the DOM

#### Scenario: Toggle persists [V]
- GIVEN the panel is closed
- WHEN the toggle is pressed and the app is remounted
- THEN the key is "1" and the panel is open

#### Scenario: Storage throws [V]
- GIVEN `localStorage` access throws
- WHEN the app mounts and the toggle is pressed
- THEN the panel starts closed and no error is shown or thrown

### Requirement: Responsive Layout

The layout MUST use one breakpoint at 1100 px viewport width. At or above it, an open synth panel MUST sit beside the main area. Below it, the open panel MUST overlay the main area. At the 720x520 minimum size all controls MUST remain reachable, with the panel open or closed.

#### Scenario: Wide viewport [M]
- GIVEN viewport 1200 px wide and the panel open
- WHEN rendered
- THEN the panel is beside the main area, not covering it

#### Scenario: Narrow viewport [M]
- GIVEN viewport 960 px wide and the panel open
- WHEN rendered
- THEN the panel overlays the main area

#### Scenario: Minimum size usable [M]
- GIVEN a 720x520 editor
- WHEN the panel is open, then closed
- THEN every control is reachable by scroll or focus

### Requirement: Knob Control

Synth panel range controls MUST render as `Knob`, wrapping a visually hidden native range input. A Knob MUST keep the aria-label, min/max/step, disabled state and dispatch of the Slider it replaces. Skewed controls MUST use the existing `SKEWS` mapping. Pointer drag MUST be vertical, 200 px of travel being the full range. Keyboard MUST work through the native input. Wheel and double-click reset MUST NOT be supported. A pure helper `ui/src/lib/knob.ts` MUST implement the drag math.

#### Scenario: Drag math [V]
- GIVEN range 0..1 at 0.5
- WHEN dragged up 100 px
- THEN the helper returns 1.0, clamped to the max, and 200 px up from min gives max

#### Scenario: Skewed knob [V]
- GIVEN the cutoff Knob at midpoint
- WHEN read
- THEN it maps to 1000 Hz via `SKEWS`

#### Scenario: Keyboard [V]
- GIVEN a focused Knob
- WHEN ArrowRight is pressed
- THEN `setPatch` is dispatched with the next step value

#### Scenario: Disabled [V]
- GIVEN FX off
- WHEN a delay Knob is rendered
- THEN its input is disabled and drag dispatches nothing

#### Scenario: No wheel or reset [V]
- GIVEN a Knob
- WHEN a wheel event or double-click occurs
- THEN the value does not change

#### Scenario: Pointer feel [M]
- GIVEN the editor in the host
- WHEN a Knob is dragged
- THEN it tracks the pointer smoothly

### Requirement: Step View

MainArea MUST show 16 steps, each with step number, gate LED, and note name. The playhead step MUST be highlighted and gated steps MUST be visually distinct from rests. It MUST be driven only by the snapshot and `playhead` events.

#### Scenario: Gate distinction [V]
- GIVEN a snapshot with step 1 gated (MIDI 48; naming convention MIDI 60 = C4) and step 2 rest
- WHEN rendered
- THEN step 1 shows its gate LED on and "C3"; step 2 shows the LED off

#### Scenario: Playhead [V]
- GIVEN a `playhead {step:4}` event
- WHEN received
- THEN only step 4 is highlighted

### Requirement: Delay-Times Card

The card MUST render each recommendation as a button labelled like "1/4 500 ms". Click MUST copy the integer ms as text (e.g. "500") via `navigator.clipboard` only. Success MUST show status "Copied 500 ms"; failure (rejection or no clipboard) MUST show "Copy failed" with no unhandled rejection. It replaces the `data-role="delay-recommendations"` text.

#### Scenario: Copy success [V]
- GIVEN BPM 120 and a working clipboard
- WHEN "1/4 500 ms" is clicked
- THEN `writeText("500")` is called and the status shows "Copied 500 ms"

#### Scenario: Copy failure [V]
- GIVEN `writeText` rejects
- WHEN a chip is clicked
- THEN the status shows "Copy failed" and no `unhandledrejection` occurs

#### Scenario: Clipboard in DAW [M]
- GIVEN the editor in Cakewalk Sonar
- WHEN a chip is clicked
- THEN the ms value pastes elsewhere, or "Copy failed" shows
