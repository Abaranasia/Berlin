# Proposal: Delay-Time Recommendation Display

Slice 3 of 5, "Berlin School authenticity" (Slice 2 `tempo-delay-ui` archived as `3860e87`). Builds on `explore.md` (Engram #313).

## Intent

A delay time picked relative to tempo is the best-sourced Berlin School convention. However, Berlin shows that time for only one division at a time, and only for its own internal delay. Users with external delay units have to flip `delayDivisionBox` and read the result off a seconds slider. This slice shows every division's time at once, as whole milliseconds, without any user action.

## Scope

### In Scope
- One read-only display that lists all 6 `SyncDivision` values with their delay time at the current BPM.
- Format is whole milliseconds, rounded with `std::lround` (half away from zero, which equals round-half-up for positive values). Values at or above 1000 ms stay in ms (e.g. `3000 ms`).
- A pure, JUCE-free formatting helper, unit-tested directly.
- Updates fire on events only: at construction and from `pushTempoFromWidgets()`.
- The display is not gated by the internal-FX toggle.

### Out of Scope
- A per-division label grid, or highlighting the currently synced division (Approach 2, future polish).
- Any change to sync math, the audio thread, `Transport`, or the preset schema.
- Host-tempo sync. Copy-to-clipboard. Fractional-ms precision.
- Labelling any division as the historically "correct" one.

## Capabilities

### New Capabilities
- `delay-time-recommendation`: read-only list of the delay time for every division at the current BPM, the ms rounding rule, event-driven refresh, and independence from the internal-FX toggle.

### Modified Capabilities
- None. `tempo-control` is consumed and stays unchanged.

## Approach

Exploration Approach 1:
1. A free function in `Source/core/TempoSync.h/.cpp` builds the ms values or string from `delaySecondsFor`. It is pure and JUCE-free.
2. The editor gets one `juce::Label` in a new row placed after DELAY. It is refreshed by `updateDelayRecommendations()`, which is called next to `recomputeSyncedDelayTime()` at the BPM call sites.
3. Bump the `setSize` height formula by one row and add a `resized()` block.

Rough size: about 150–250 changed lines including tests, well under the 800-line budget.

## Affected Areas

| Area | Impact | Description |
|---|---|---|
| `Source/core/TempoSync.h/.cpp` | Modified | Additive ms/format helper |
| `Source/plugin/BerlinAudioProcessorEditor.h/.cpp` | Modified | Label, refresh helper, layout row, height bump |
| `Tests/Source/TempoSyncTests.cpp` | Modified | Rounding and format tests (written first, TDD) |

## Risks

| Risk | Likelihood | Mitigation |
|---|---|---|
| New row is clipped because the `setSize` bump is missed | Med | Verification item; manual layout check |
| String overflows the fixed 800px width | Low | Compact separators; design sets the width |
| Readers take a listed division as an authority claim | Low | Neutral label text, all divisions shown equally |
| BPM is read off the message thread | Low | Only existing message-thread call sites; no Timer |

## Rollback Plan

Revert the slice commit. All changes are additive: one helper, one label, one layout row. There is no preset, schema, or audio-path change, so no data compatibility concern.

## Dependencies

- Slice 2 (`TempoSync`, `pushTempoFromWidgets`), already landed.

## Success Criteria

- [ ] At 160 BPM the formatter yields 750/375/281/188/125/94 ms (test).
- [ ] At 120 BPM it yields 1000/500/375/250/167/125 ms (test); at 40 BPM, 1/2 yields 3000 ms.
- [ ] The display updates when BPM changes and stays visible with internal FX off.
- [ ] No Timer and no audio-thread or preset change (review gate).
- [ ] `BerlinTests.exe --category=Berlin` passes.

## Proposal question round

Auto mode, so these were resolved by assumption. Flag any to change:
1. **Rounding**: whole ms via `std::lround`. Some units accept 0.1 ms.
2. **Placement**: a new row directly under DELAY, rather than a separate section.
3. **Free mode**: the display stays visible even when the internal delay is in Free mode.
