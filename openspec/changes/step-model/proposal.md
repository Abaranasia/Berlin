# Proposal: Step Model (Variable Length + True Skip)

## Intent

Berlin is meant to generate Berlin School sequences, but every generated pattern is fixed at 16 steps. A step is only `{note, active}`, so a muted step still takes up time. Two of the best-sourced techniques can't be expressed today:
- **Patterns of different lengths** (8/16-step patterns per Franke in E&MM 1983 and SOS 1994; polymetric loops per Wikipedia "Analog sequencer").
- **Per-step play/skip switches that shorten the loop** (Moog 960, via Wikipedia and ucapps).

This change is Slice 1 of `step-model`: variable length plus true skip. Velocity/accent and gate length are single-source techniques and come in a follow-up change.

## Scope

### In Scope
- Append `bool skipped = false` to `Step`. The struct stays an aggregate, so `Step{note, active}` still compiles. A skipped step leaves the cycle; `active=false` stays a timed rest.
- Add `GenerationParams.length` (default 16, range 1..64) for all three rhythm modes:
  - random mode scales its active count as `(length*11 + 8) / 16`, which gives 11 at length 16;
  - euclidean pulses and rotation are clamped to the length.
- Make the `SequencePlayer` cursor skip-aware on the audio thread. Wrapping past the end increments `loopCount`. This handles a skipped step 0, and an all-skipped sequence plays silence.
- `MidiExportTimeline` mirrors the skip rule.
- Add an optional `skipped` argument to `setStep` and `StepEdit` instead of a new command, so `kCommands` and its static_assert don't change. Editable steps go from 16 to the sequence size.
- `addNote` and `removeNote` ignore skipped steps.
- React `StepView` shows the real length (wrapping into rows of 16), with a per-step skip toggle and a length control.

### Out of Scope
- Velocity/accent and gate length, in the follow-up change `step-dynamics`.
- Persisting length and step edits. This matches today's rule that mode, pulses, rotation and edits are unpersisted. Follow-up: a schema v4 pattern block.
- Generators producing skips, reset steps, and direction modes (the last is folklore).

## Capabilities

### New Capabilities
- None.

### Modified Capabilities
- `sequencing-core`: `Step` gains `skipped`.
- `deterministic-generation`: the length parameter; output at length 16 is byte-identical.
- `euclidean-rhythm`: steps = length instead of a fixed 16.
- `step-event-scheduling`: skip-aware cursor and wrap-driven loop.
- `auto-evolution`: "loop" now means one pass over the non-skipped steps.
- `midi-export-timeline`: export follows the skip rule.
- `sequence-mutation`: addNote/removeNote ignore skipped steps.
- `step-editing`: skip edits; editable range = sequence size.
- `generation-live-control`: length control.
- `ui-bridge`: `setStep` and step encoding carry `skipped`; the params carry `length`.
- `web-ui-controls`: variable-length grid, skip toggle, length control.

## Approach

| # | Decision |
|---|---|
| D1 | Skip is a separate `bool`, not an enum, to keep the blast radius small. |
| D2 | The maximum length is 64 because mutations (stretch, palindrome) already produce up to 64 steps that today's 16-cell UI can't show or edit. One `kMaxSteps = 64` constant covers both. |
| D3 | Generated sequences contain no skips. Skip is an editing/performance tool, like the 960's switch. Adding skips to generation would mean extra RNG draws and would break the goldens. |
| D4 | The cursor scans forward with a bound of `size` and no allocation. If every step is skipped, there is no event and no wrap. |

## Affected Areas

| Area | Impact |
|------|--------|
| `Source/core/Step.h`, `Sequence.cpp` | Modified |
| `Source/generation/{GenerationParams.h,SequenceBuilder.cpp,StepEdit.*,MutationEngine.cpp}` | Modified |
| `Source/playback/SequencePlayer.cpp` | Modified (audio thread) |
| `Source/export/MidiExportTimeline.*` | Modified |
| `Source/plugin/BerlinAudioProcessor.cpp`, `Source/bridge/UiBridge.cpp` | Modified |
| `ui/src/{bridge/protocol.ts,state/steps.ts,components/StepView.tsx,store/actions.ts}` | Modified |
| `Tests/Source/*`, `ui` Vitest | New and modified tests |

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| Goldens drift | Med | Length 16 = today's draw count; ReproducibilityTests stay unchanged |
| Player and export diverge | Med | Shared skip-rule test cases on both |
| Auto-evolve stalls when everything is skipped | Low | Documented behavior; no wrap, no mutation |
| `compress` grows lengths 1-7 up to its floor of 4 | Low | Existing clamp; documented, not changed |
| Edits lost on reload | High | Explicit non-goal; schema v4 follow-up |
| Budget overrun | Med | Velocity/gate already split out |

## Rollback Plan

Single PR: revert the commit. There is no schema change, so presets and plugin state are unaffected.

## Dependencies

- None.

## Success Criteria

- [ ] Default params (length 16) reproduce the pre-change output byte-identically.
- [ ] Skipped steps take no time in playback or export, and the loop count follows the wrap, including when step 0 is skipped.
- [ ] An all-skipped sequence is silent and stable.
- [ ] Lengths 1..64 generate and edit correctly. Mutated sequences longer than 16 are visible and editable.
- [ ] No allocation or locking is added on the audio thread.

## Review-size forecast

About 650-780 changed lines (production ~400, tests ~300). This fits the 800 budget.

## Proposal question round (auto mode — assumptions for user review)

1. Is a maximum length of 64 acceptable, or should the first slice cap it at 32?
2. Is it acceptable that skip and length edits are lost when a preset is reloaded?
3. Should generators stay skip-free?
