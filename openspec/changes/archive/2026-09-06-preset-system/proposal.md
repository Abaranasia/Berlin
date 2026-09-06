# Proposal: Preset System (roadmap Phase 11)

## Intent

Every sound the user builds is session-volatile. The 11 live synth parameters live only inside JUCE widgets and voice atomics; `currentSeed` lives only in `MainComponent`. Quitting the app destroys both. A musician cannot keep a patch, A/B two sounds, or return to yesterday's work. This codebase has **zero persistence of any kind** — this phase introduces the first one.

## Scope

### In Scope

- A named, browsable preset store bundling **the 11 live synth parameters + the generation seed** in one unit.
- New JUCE-aware persistence layer (`Source/preset/`) owning serialization, disk I/O, and preset enumeration; `SynthPatch.h` stays JUCE-free per its documented exception.
- PRESETS UI row (name entry, Save, preset selector, Load) following Berlin UI Pattern v1.
- One new capability spec + spec deltas.

### Out of Scope

The 8 dead `SynthPatch` effects fields (delay/reverb/outputLevel) — stay pinned to `kDefaultPatch` · `numSteps`/`activeSteps` · Mutation Engine (§14) · scale/root/tempo/MIDI-device in presets (proposal §25's wider list) · factory preset bank · preset rename/delete/reorder · import/export via OS file dialog.

## Approach

**1. Data unit.** A `Preset` aggregate = `SynthPatch` (11 live fields only) + `juce::int64 seed` + name + `schemaVersion` int.

**2. Format — ValueTree serialized as XML text (recommended).** Satisfies both the `juce-app-dev` gate ("one versioned `ValueTree`, with a schema version int") and proposal §25's "presets should be human-readable". §25 literally suggests JSON, but JUCE's `var`/JSON path has no `int64` fidelity guarantee and no versioning idiom. Design decides; the ValueTree-vs-JSON tradeoff must be argued, not assumed.

**3. Storage — one file per preset** in `userApplicationDataDirectory/Berlin/Presets/*.xml`, enumerated at startup into a `ComboBox`. Gives named + browsable without an OS dialog per action, and limits corruption blast radius to one preset. Alternative (single `PropertiesFile` holding a list) is the design-phase counter-option.

**4. Load path reuses existing plumbing, zero audio-thread work.** Inverse of `pushAllParametersToSynth()`: set the 11 widgets with `dontSendNotification`, call the 11 existing `SynthEngine` setters, set `currentSeed` + `seedEditor`, call `regenerate(false)`. First phase in this project with no RT-safety surface at all.

**5. Layout.** A full-width preset row above the two columns costs 34px, leaving the tight right column ~44px slack — no window growth. Placing it inside the right column instead consumes 34 of its 78px. Design picks; growing past 800x600 should be a last resort.

## Capabilities

### New Capabilities

- `preset-persistence`: preset data unit, schema version, save/load/overwrite semantics, disk layout, malformed-file handling, enumeration.

### Modified Capabilities

- `internal-synth-voice`: its Purpose states "no preset save/load yet" — now false.
- `generation-live-control`: loading a preset becomes a **third** seed-mutating path alongside Generate and Randomize. Its Lock Seed requirement ("MUST NOT draw or apply a new seed value") does not currently anticipate this; the interaction needs an explicit requirement.

## Affected Areas

| Area | Impact | Description |
|---|---|---|
| `Source/preset/` | New | `PresetManager` + `Preset` data unit; serialization + file I/O |
| `Source/synth/SynthPatch.h` | Unchanged | Reused as-is; 11 of 19 fields participate |
| `Source/synth/SynthEngine.*`, `SynthVoice.*` | Unchanged | Existing 11 setters already sufficient |
| `Source/MainComponent.h/.cpp` | Modified | PRESETS row, save/load wiring, widget-value restore |
| `openspec/specs/preset-persistence/` | New | New capability spec |
| `openspec/specs/{internal-synth-voice,generation-live-control}/` | Modified | Delta specs |
| `Tests/Source/` | New | Round-trip fidelity, schema version, malformed-file rejection |

## Settled Decisions (user-confirmed — do not revisit)

| Question | Decision |
|---|---|
| Preset contents | Combined: 11 synth params **and** seed, one action |
| 8 effects fields | Excluded; stay on `kDefaultPatch` |
| `numSteps`/`activeSteps` | Out of scope, not UI-exposed |
| Mutation Engine | Separate future phase |

## Open Decisions (for sdd-design)

1. **Name collision on save**: silent overwrite, confirm dialog, or reject? Recommend async confirm (`AlertWindow::showOkCancelBox`) — silent overwrite destroys work, reject is hostile.
2. ValueTree+XML vs. JSON vs. `PropertiesFile` (see Approach 2/3).
3. **Lock Seed vs. preset load**: does Lock Seed block a preset's seed, or does an explicit load always win? Recommend load wins (explicit user intent) — must be spec'd either way.
4. Preset row placement: full-width header row vs. right column.
5. Empty state: what the selector shows when no presets exist, and whether Load is disabled.
6. Malformed/out-of-range preset values: reject the file, or clamp to each parameter's documented range?

## Risks

| Risk | Likelihood | Mitigation |
|---|---|---|
| No persistence precedent in this codebase; the skill's ValueTree rule is guidance, never exercised here | High | Isolate all I/O in `PresetManager`; round-trip test as first task |
| Human-readable format invites hand-editing → out-of-range values pushed into voice atomics | Medium | Validate/clamp on load against existing documented ranges; explicit spec requirement |
| Loading while playing restarts the transport from step 1 (existing `generation-live-control` contract) — audible and possibly surprising | Medium | Spec it as intended behaviour, not a defect |
| Layout: right column has only ~78px verified slack | Medium | Full-width row (34px) budgeted before wiring |
| Coupling two independently-evolved subsystems into one file format | Medium | Two named sections inside one versioned tree, not a flat blob |
| Size: forecast ~650–800 lines vs. Phase 9's ~680, Phase 10's ~700–800 | Medium | `single-pr` strategy resolved; re-forecast at sdd-tasks |

## Rollback Plan

Single-commit revert: delete `Source/preset/`, the PRESETS row, and the new specs. No in-app state depends on preset files and nothing else reads them, so orphaned files on disk are inert. No migration, no schema already in the wild.

## Dependencies

None external. Depends on existing `SynthEngine` setters, `SynthPatch`, `regenerate(bool)`, and `pushAllParametersToSynth()` — all unchanged.

## Success Criteria

- [ ] Saving a named preset, quitting, relaunching, and loading it restores all 11 synth parameters and the seed to their saved values.
- [ ] A loaded preset reproduces the same `Sequence` the seed produced when saved.
- [ ] Loading while audio runs causes no hung note, dropout, assert, or crash.
- [ ] Widget displayed values match the loaded values (no stale UI vs. live audio drift).
- [ ] Saving over an existing name follows the decided collision policy, deterministically.
- [ ] A malformed, truncated, or wrong-version preset file is handled without crashing and without pushing invalid values into the synth.
- [ ] The 8 effects fields are provably unaffected by any preset operation.
- [ ] All three spec artifacts (1 new + 2 deltas) exist; no spec text still claims there is no preset save/load.
- [ ] Full test suite green.

## Proposal question round

This proposal was produced without a live user question round (delegated phase, no direct user channel). The scope decisions above are user-confirmed; the following product questions would sharpen it before design. Answering is optional — design can proceed on the stated recommendations.

1. **Collision**: when you save under an existing preset name, should it overwrite silently, ask, or refuse? (assumed: ask)
2. **Lock Seed**: with Lock Seed on, should loading a preset still change the seed? (assumed: yes — an explicit load beats a lock)
3. **Load while playing**: is restarting the sequence from step 1 on load what you want, or should the synth parameters load without touching the running sequence? (assumed: full load, restart — matches Generate)
4. **First run**: with no presets saved yet, is an empty selector acceptable, or do you want a small factory bank shipped? (assumed: empty; factory bank out of scope)
5. **Preset files**: is a hidden app-data folder right, or do you expect to see, share, and hand-edit these files somewhere obvious like Documents? (assumed: app-data folder, human-readable XML)
