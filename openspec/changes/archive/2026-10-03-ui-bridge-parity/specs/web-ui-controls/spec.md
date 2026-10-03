# Web UI Controls Specification

## Purpose

The React UI (`ui/src`) gives full legacy-editor parity through `UiBridge` only: typed protocol, bridge abstraction, store, and plain unstyled controls. Styling is out of scope.

## Requirements

### Requirement: Typed Protocol

`protocol.ts` MUST type the 15 commands with their args, the snapshot, the events (`playhead`, `snapshot`), the error tokens, the enum lists (waveform, lfoDestination, scaleType, delayDivision, mode) and hand-written parameter limits. Seed MUST be a string.

#### Scenario: Enum lists match the bridge encoding
- GIVEN the protocol enum lists
- WHEN compared with the `ui-bridge` encoding conventions
- THEN they are identical (e.g. waveform `saw|square|pulse|triangle`)

#### Scenario: Unknown command is a type error
- GIVEN a TS call with a command name outside the 15
- WHEN type-checked
- THEN compilation fails

### Requirement: Bridge Interface And Implementations

The UI MUST talk to the host only through a `Bridge` interface (`dispatch`, `getSnapshot`, `onPlayhead`, `onSnapshot`, `chooseExportFile`, `confirm`) with a `nativeBridge` and a `mockBridge`. Components MUST NOT call the native layer directly.

#### Scenario: Native bridge forwards
- GIVEN a host exposing the native functions
- WHEN the store dispatches `setBpm`
- THEN `nativeBridge` calls native `dispatch` with that command and args

#### Scenario: Events subscribed and unsubscribed
- GIVEN `onSnapshot` and `onPlayhead` subscriptions
- WHEN the returned unsubscribe functions are called
- THEN later events are not delivered

### Requirement: Mock Bridge Is Dev-Only

The mock MUST be selected only when no host is present AND `import.meta.env.DEV` is true, via a dynamic import that is lexically guarded by `import.meta.env.DEV` (so a production build emits no mock chunk). It MUST NOT appear in embedded release assets: the embed step (`ui/scripts/embed-lib.mjs` `assertNoMockChunk`) MUST fail if any built asset file name matches `/mock/`. It MUST use the same fixtures and error tokens as the protocol.

#### Scenario: No host in dev uses mock
- GIVEN no native host and DEV true
- WHEN the app starts
- THEN the mock bridge is used

#### Scenario: Release has no mock
- GIVEN a production build
- WHEN the embed step lists the built asset file names
- THEN none matches `/mock/`, and `assertNoMockChunk` throws when given a list that contains one

#### Scenario: Host present uses native
- GIVEN a native host
- WHEN the app starts in any mode
- THEN `nativeBridge` is used

#### Scenario: Mock reproduces error tokens
- GIVEN the mock bridge
- WHEN `setBpm` is dispatched without `bpm`
- THEN it returns `{ok:false, error:"missing arg: bpm"}`

### Requirement: External Store With Slices

State MUST live in a dependency-free store consumed via `useSyncExternalStore` with selectors. Playhead, status and draft MUST be separate slices from the engine snapshot, so a playhead event does not re-render snapshot-only components.

#### Scenario: Selector isolation
- GIVEN a component selecting only `bpm`
- WHEN a `playhead` event arrives
- THEN that component does not re-render

#### Scenario: Snapshot event updates store
- GIVEN the store holds a snapshot
- WHEN a `snapshot` event arrives with no in-flight command
- THEN selectors reflect the new snapshot

### Requirement: Optimistic Updates And Latest-Wins Coalescing

A control change MUST update the store immediately (optimistic). Per command key, at most one request MUST be in flight and at most one pending; a newer change MUST replace the pending one (latest wins). A key settles when its in-flight request has completed and no value is pending for it; when it settles, its optimistic value is dropped and the engine snapshot value is shown. On success the response snapshot reconciles state; on `ok:false` with no newer pending value for that key, the store MUST drop the optimistic value, so the display reverts to the last confirmed engine snapshot value (a newer pending value for the key is still sent and stays displayed).

#### Scenario: Rapid drag coalesces
- GIVEN a slider changes 10 times while the first request is in flight
- WHEN the first response arrives
- THEN exactly one further request is sent with the latest value

#### Scenario: Optimistic value shown instantly
- GIVEN a pending response
- WHEN the user changes a control
- THEN the displayed value is the new one before any response

#### Scenario: Failure reverts
- GIVEN an optimistic change
- WHEN the command returns `ok:false`
- THEN the value reverts to the confirmed value and the status line shows the error

### Requirement: Stale Responses Are Dropped

A single global, monotonically increasing sequence counter MUST tag each request. A response snapshot MUST be applied only if its sequence is greater than the last applied sequence; otherwise it MUST be ignored (the response still settles its key). `snapshot` events carry no sequence: they MUST update the engine snapshot, but MUST NOT replace the displayed value of a key with an optimistic value until that key settles.

#### Scenario: Out-of-order responses
- GIVEN requests 1 and 2 for different keys, and response 2 applied
- WHEN response 1 arrives
- THEN its snapshot is not applied and the engine state is unchanged

#### Scenario: Stale snapshot event during optimistic edit
- GIVEN an optimistic `cutoffHz` edit in flight
- WHEN a snapshot event with the old `cutoffHz` arrives
- THEN the displayed `cutoffHz` keeps the optimistic value

### Requirement: Command Failures Never Reject Unhandled

Any dispatch or dialog failure (`ok:false`, thrown error, rejected promise) MUST surface in the status line and MUST NOT produce an unhandled promise rejection.

#### Scenario: Native throws
- GIVEN the native function rejects
- WHEN a control triggers it
- THEN the status line shows an error and no `unhandledrejection` occurs

### Requirement: Status Line And Error Messages

A status line MUST show the latest outcome. Error tokens MUST map to human-readable messages exactly as in the design's "Status Messages" table (including `exists`, `busy`, `path not absolute`, every `PresetResult` and `MidiFileWriteResult` name, with preset and write failures worded as the legacy `describePresetFailure` / `describeWriteFailure`). Because `writeFailed` is both a `PresetResult` and a `MidiFileWriteResult` name, the mapping MUST take the command into account (`exportMidi` uses the write-failure wording, `savePreset`/`loadPreset` the preset wording). Unmapped tokens MUST show the raw token. Error outcomes, including the invalid-seed message, MUST be shown in an error style.

#### Scenario: Token mapping
- GIVEN `loadPreset` returns `fileNotFound`
- WHEN the store handles the failure
- THEN the status shows the mapped message, not the raw token

#### Scenario: Unmapped token
- GIVEN an unknown token `weird`
- WHEN handled
- THEN the status shows `weird`

#### Scenario: Busy
- GIVEN `regenerate` returns `busy`
- WHEN handled
- THEN the status shows `Busy, try again` (the legacy wording)

#### Scenario: Command-dependent writeFailed
- GIVEN `writeFailed` is returned
- WHEN it comes from `exportMidi`, and separately from `savePreset`
- THEN the status shows "Export failed: could not write the file." and "Could not write the preset file." respectively

### Requirement: Transport, Tempo And Level Controls

The UI MUST provide Play/Stop (`setPlaying`), BPM (−/+ and entry, `setBpm`), master level (`setMasterLevel`), and a 16-step display with the playhead highlight driven by `playhead` events and the initial snapshot. Consecutive BPM presses MUST accumulate (no stale-closure loss).

#### Scenario: Play/Stop
- GIVEN stopped
- WHEN Play is pressed
- THEN `setPlaying {playing:true}` is dispatched and the control shows playing after the response

#### Scenario: Rapid BPM presses
- GIVEN BPM 120
- WHEN + is pressed three times quickly
- THEN the final dispatched BPM equals 120 plus three increments and none are lost

#### Scenario: Master level
- GIVEN the level slider
- WHEN moved
- THEN `setMasterLevel {level}` is dispatched with the slider value

#### Scenario: Playhead follows (MANUAL)
- GIVEN playback is running
- WHEN steps advance
- THEN the highlighted step follows, in standalone and Cakewalk Sonar

### Requirement: Synth And FX Toggles

The UI MUST provide Synth and FX toggles (`setSynthEnabled`, `setEffectsEnabled`) reflecting the snapshot.

#### Scenario: Toggle FX
- GIVEN FX on
- WHEN toggled
- THEN `setEffectsEnabled {enabled:false}` is dispatched

### Requirement: Live Generation Params

Generation param edits (mode, pulses, rotation, chance, lock seed, scale, root, range) MUST be sent immediately via `setGenerationParams` (not staged until Generate). `rangeLow` and `rangeHigh` MUST always be sent together in one command.

#### Scenario: Live send
- GIVEN the mode is euclidean
- WHEN pulses changes to 5
- THEN `setGenerationParams {pulses:5}` is dispatched without pressing Generate

#### Scenario: Range sent together
- GIVEN range 36..72
- WHEN Range Low is changed to 40
- THEN the command contains both `rangeLow:40` and `rangeHigh:72`

### Requirement: Generation, Seed And Rhythm Controls

The UI MUST provide: Seed field (`setSeed`), Generate (`regenerate {randomize:false}`), Randomize (`regenerate {randomize:true}`), Mutate (`mutate`), Lock Seed, Rhythm mode (random|euclidean|probability), Pulses, Rotation and Chance %.

#### Scenario: Generate and Randomize
- GIVEN Lock Seed off
- WHEN Generate then Randomize are pressed
- THEN `regenerate` is dispatched with `randomize:false` then `randomize:true`

#### Scenario: Mutate
- GIVEN a sequence
- WHEN Mutate is pressed
- THEN `mutate` is dispatched

#### Scenario: Rhythm fields reflect snapshot
- GIVEN a snapshot with mode `probability` and chance 0.5
- WHEN rendered
- THEN mode and chance controls show those values

### Requirement: Pitch Controls

The UI MUST provide Scale, Root, Range Low and Range High, covering all scale and root values and the limits in the protocol.

#### Scenario: Scale choices
- GIVEN the Scale control
- WHEN opened
- THEN it offers exactly `minor|major|dorian|phrygian|mixolydian|harmonicMinor`

### Requirement: Evolution Controls

The UI MUST provide Auto-Evolve (`setAutoEvolveEnabled`) and Rate (`setAutoEvolveRate`).

#### Scenario: Enable evolve
- GIVEN auto-evolve off
- WHEN toggled on
- THEN `setAutoEvolveEnabled {enabled:true}` is dispatched

#### Scenario: Evolve updates UI via snapshot event
- GIVEN auto-evolve is running
- WHEN a `snapshot` event arrives with a new `mutationCount` and steps
- THEN the step display updates

### Requirement: Synth Panel Controls

The UI MUST provide via `setPatch`: Waveform, Pulse Width, Cutoff, Resonance, Attack, Decay, Sustain, Release, LFO Destination, LFO Rate and LFO Depth.

#### Scenario: Waveform choices
- GIVEN the Waveform control
- WHEN opened
- THEN it offers `saw|square|pulse|triangle` and selecting one dispatches `setPatch {waveform}`

#### Scenario: Cutoff change
- GIVEN the cutoff slider
- WHEN moved
- THEN `setPatch {cutoffHz}` carries a value within the protocol limits

### Requirement: Skewed Sliders

Cutoff, Resonance, Attack, Decay, Release and LFO Rate sliders MUST use skewed mappings whose slider midpoint maps to the legacy values: cutoff 1000, resonance 2, attack 0.2, decay 0.3, release 0.5, LFO rate 2. Endpoints MUST map to the protocol limits and the mapping MUST round-trip.

#### Scenario: Midpoint
- GIVEN the cutoff slider position 0.5
- WHEN mapped to a value
- THEN it is 1000 (likewise resonance 2, attack 0.2, decay 0.3, release 0.5, LFO rate 2)

#### Scenario: Round trip and endpoints
- GIVEN any value within limits
- WHEN mapped to position and back
- THEN it equals the original within tolerance, with positions 0 and 1 giving min and max

### Requirement: Delay Controls

The UI MUST provide Delay Sync, Division, Time, Feedback and Mix (`setPatch`). Delay Time MUST be disabled while Sync is on. Switching Sync from on to off MUST restore the last manual delay time (the last Time set while Sync was off, or the snapshot value when none).

#### Scenario: Sync memory
- GIVEN Sync off with Time 0.30 s, then Sync on, then BPM changes the synced time
- WHEN Sync is turned off
- THEN `setPatch` restores `delayTimeSeconds: 0.30`

#### Scenario: Time disabled in Sync
- GIVEN Sync on
- WHEN rendered
- THEN the Time control is disabled

### Requirement: Delay Recommendations

The UI MUST compute delay recommendations in TS from BPM: beat ms = 60000/bpm; factors 2, 1, 0.75, 0.5, 1/3, 0.25 with labels `1/2, 1/4, 1/8., 1/8, 1/8T, 1/16`; each value rounded to whole ms.

#### Scenario: At 120 BPM
- GIVEN BPM 120
- WHEN recommendations are computed
- THEN they are 1/2=1000, 1/4=500, 1/8.=375, 1/8=250, 1/8T=167, 1/16=125 ms

#### Scenario: Updates with BPM
- GIVEN BPM changes to 150
- WHEN recomputed
- THEN 1/4 shows 400 ms

### Requirement: Reverb Controls

The UI MUST provide Room, Damping, Wet and Dry (`setPatch`).

#### Scenario: Wet change
- GIVEN the Wet slider
- WHEN moved
- THEN `setPatch {reverbWetLevel}` is dispatched

### Requirement: Disabled States

Delay and Reverb controls MUST be disabled while FX is off; Randomize MUST be disabled while Lock Seed is on; Save MUST be disabled while the name is empty after trimming whitespace (and Save sends the trimmed name); Load MUST be disabled with no selection.

#### Scenario: FX off
- GIVEN FX off
- WHEN rendered
- THEN all delay and reverb controls are disabled

#### Scenario: Lock seed
- GIVEN Lock Seed on
- WHEN rendered
- THEN Randomize is disabled

#### Scenario: Preset buttons
- GIVEN an empty name and no selection
- WHEN rendered
- THEN Save and Load are disabled

### Requirement: Seed Validation

The seed field MUST accept only `-?digits`. Invalid input MUST show the error message "Seed must be a whole number." and MUST NOT dispatch `setSeed`. Valid input MUST dispatch the seed as a string.

#### Scenario: Invalid seed
- GIVEN the user enters `12a`
- WHEN committed
- THEN the message "Seed must be a whole number." is shown in error style and nothing is dispatched

#### Scenario: Large seed
- GIVEN `9223372036854770000`
- WHEN committed
- THEN `setSeed {seed:"9223372036854770000"}` is dispatched as a string

### Requirement: Presets UI

The UI MUST provide a preset name field with Save, and a preset list (from `snapshot.presetNames`) with Load. After a successful load the name field MUST show the loaded name. The list MUST refresh from snapshots.

#### Scenario: Load fills name
- GIVEN "Lead A" is selected
- WHEN Load succeeds
- THEN the name field shows "Lead A"

#### Scenario: Load busy
- GIVEN `loadPreset` returns `busy`
- WHEN handled
- THEN the status shows the busy message and the name field is unchanged

### Requirement: Save Flow

Save MUST dispatch `savePreset {name, overwrite:false}`. On `exists` it MUST call `confirm`; only if true MUST it dispatch `savePreset {name, overwrite:true}`. Declining MUST leave the stored preset unchanged.

#### Scenario: New name
- GIVEN no preset "New"
- WHEN Save is pressed
- THEN one `savePreset` with `overwrite:false` succeeds and no confirm is shown

#### Scenario: Confirm overwrite
- GIVEN "Lead" exists
- WHEN Save returns `exists` and `confirm` resolves true
- THEN `savePreset {overwrite:true}` is dispatched

#### Scenario: Decline overwrite
- GIVEN `exists` and `confirm` resolves false
- WHEN handled
- THEN no second dispatch occurs

### Requirement: Export Flow

Export MUST call `chooseExportFile`, then `exportMidi {path}` with the chosen path. A cancelled result without `error` MUST be a silent no-op (no dispatch, no status error). A result with `error:"busy"` MUST NOT dispatch and MUST show the busy message in the status line.

#### Scenario: Export chosen path
- GIVEN `chooseExportFile` resolves `{cancelled:false, path:"C:/a.mid"}`
- WHEN Export is pressed
- THEN `exportMidi {path:"C:/a.mid"}` is dispatched

#### Scenario: Cancel
- GIVEN `{cancelled:true, path:""}`
- WHEN handled
- THEN nothing is dispatched and no error is shown

#### Scenario: Chooser busy
- GIVEN `{cancelled:true, path:"", error:"busy"}`
- WHEN handled
- THEN nothing is dispatched and the status shows the busy message

#### Scenario: Write failure
- GIVEN `exportMidi` returns `pathUnavailable`
- WHEN handled
- THEN the status shows the mapped message

### Requirement: Dev Script

`ui/package.json` MUST define `"dev": "vite"` and `ui/vite.config.ts` MUST set `server: {port: 5173, strictPort: true}`, matching the conventional `BERLIN_WEB_UI_DEV_URL` value `http://localhost:5173`.

#### Scenario: Pinned port
- GIVEN the Vite config
- WHEN read
- THEN `server.port` is 5173 and `strictPort` is true

#### Scenario: Dev against mock
- GIVEN `pnpm --dir ui dev` in a plain browser
- WHEN the page loads
- THEN the mock bridge drives a working UI

### Requirement: Limits Drift Guard

A Vitest guard at `ui/scripts/limits-drift.test.mjs` MUST compare the TS limits in `ui/src/bridge/limits.ts` with the `kMin*/kMax*` values (including BPM) in `Source/synth/SynthPatch.h`, read by regex via a relative `fs` path. It needs no build plumbing (`scripts/**/*.test.mjs` is already in the Vitest `include`).

#### Scenario: Matching limits
- GIVEN the TS limits and `SynthPatch.h`
- WHEN the guard runs
- THEN it passes, and fails if any bound differs

### Requirement: Manual Parity Checks (MANUAL)

Every legacy-editor action MUST be verified manually with the flag on, in standalone and Cakewalk Sonar. A Sonar-only dialog failure blocks the slice.

#### Scenario: Parity checklist (MANUAL)
- GIVEN the flag-on build
- WHEN each control section (transport, toggles, tempo, delay, reverb, generation, rhythm, pitch, evolution, synth, presets, export) is exercised
- THEN each behaves like the legacy editor

#### Scenario: Dialogs (MANUAL)
- GIVEN Export and overwrite-confirm
- WHEN used, including closing the editor mid-dialog
- THEN they behave correctly and keyboard focus is not lost, in both hosts
