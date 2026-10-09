# Proposal: Generation / Randomize (roadmap Phase 10)

## Intent

Generation is already live in `MainComponent::buildSeededSequence()` — but frozen: one hardcoded seed (`kSeed = 12345`), built once into a `const Sequence`, immutable by construction *and* by spec. Trying a different pattern requires a rebuild. This phase turns generation into a runtime action and gives the app its first live-regeneration capability.

## Scope

### In Scope

- GENERATION UI section following Berlin UI Pattern v1: `Generate`, `Randomize`, seed display, `Lock Seed`.
- Live regeneration: replace the playing `Sequence` at runtime, audio-thread-safe.
- Skip-mask rhythm generation replacing uniform per-step coin flips.
- Spec deltas + one new capability (below).

### Out of Scope

Polymetric / multi-length layering (needs polyphony — later phase) · velocity/groove layer (blocked: no velocity field on `Step`/`StepEvent`) · slow root transposition over time · root/scale/steps/density UI exposure · Mutation Engine (transpose/reverse/rotate/stretch).

## Settled Semantics

| Action | Behaviour |
|---|---|
| Generate | Regenerate using the **current** seed. Same seed → identical `Sequence` (existing reproducibility contract). |
| Randomize / New Seed | Draw a fresh seed value externally (system RNG), then generate. `DeterministicRandom` stays explicit-seed-only. |
| Lock Seed | Suppresses reseeding, so repeated Generate presses stay reproducible. |

## Approach

**1. Regeneration: atomic publish-and-adopt — NOT naive stop-then-swap.**

Evidence against exploration's default recommendation: `SequencePlayer::process` reads `sequence.size()` and `sequence[stepIndex]` *before and regardless of* the transport's running state, and `Transport::running` is a plain non-atomic `bool`. Calling `stop()` from the message thread and then reassigning the `Sequence` is therefore a real data race, not a safe pattern.

Instead: the message thread builds the new `Sequence` and publishes it to the player through an explicit cross-thread handoff; the audio thread adopts it at the top of `process()` (buffer swap only — no allocation, no free, no blocking lock), emitting any pending note-off first and resetting the playhead. `SequencePlayer`'s `const Sequence` and its documented "one owner, one immutable snapshot" invariant relax to "one owner, one atomically-published snapshot". Exact handoff shape is a design decision (see Open Decisions).

**2. Rhythm: skip-mask displacement** (research Finding 3, MIDIbox source).

Derive a skip mask over the step grid instead of independent per-step coin flips, so an even 16-step grid plays an odd effective count with deliberate rhythmic displacement. Chosen over "gradual density growth" because growth mutates the pattern across repeats — that is the deferred Mutation Engine, not a static `Sequence`. Recommended shape: an **additive** generator type, leaving `RhythmGenerator`'s existing probability requirement and tests intact.

## Capabilities

### New Capabilities

- `generation-live-control`: runtime regeneration trigger, seed selection, seed lock, and the audio-thread-safe sequence handoff contract. Chosen over overloading `realtime-audio-wiring` (a lifecycle spec) with runtime-mutation requirements.

### Modified Capabilities

- `realtime-audio-wiring`: "Sequence Built Before Audio Starts" currently states the Sequence "is never modified afterward" — must become "never modified concurrently; replaced only via the published handoff".
- `deterministic-generation`: add the skip-mask rhythm requirement; correct the stale design note claiming no production class composes `RhythmGenerator` + `PitchGenerator` (`MainComponent` does).
- `step-event-scheduling`: **missed by exploration** — spec line 101 mandates that `flushPendingNoteOff`'s "only production call site MUST be `MainComponent::releaseResources()`". Regeneration adds a second call site.

## Affected Areas

| Area | Impact | Description |
|---|---|---|
| `Source/playback/SequencePlayer.h/.cpp` | Modified | Relax `const Sequence`; add publish/adopt handoff; rewrite the header's immutability rationale |
| `Source/playback/Transport.h/.cpp` | Modified | `running` may need to become atomic if regeneration touches transport state |
| `Source/MainComponent.h/.cpp` | Modified | GENERATION UI, seed state, `buildSeededSequence` becomes seed-parameterised, regeneration wiring |
| `Source/generation/` | New | Skip-mask rhythm generator (additive) |
| `openspec/specs/generation-live-control/` | New | New capability spec |
| `openspec/specs/{realtime-audio-wiring,deterministic-generation,step-event-scheduling}/` | Modified | Delta specs |
| `Tests/Source/` | New/Modified | Handoff semantics, skip-mask determinism, seed-lock reproducibility |

## Open Decisions (for sdd-design)

1. Handoff mechanism: atomic-flag + buffer swap, double-buffer index, or `juce::SpinLock::tryEnter` with a one-block bail-out (skill-sanctioned for trivially short critical sections).
2. Skip-mask generator shape: new type vs. a style enum on `RhythmGenerator` (the latter breaks 2 existing spec scenarios + the reproducibility golden).
3. Whether regeneration resets the playhead to step 0 or adopts mid-loop.

## Risks

| Risk | Likelihood | Mitigation |
|---|---|---|
| Data race on the live `Sequence` (naive stop-then-swap) | High if unaddressed | Publish-and-adopt handoff; explicit design-phase decision; no message-thread write to a live-read `Sequence` |
| Hung/dropped note at the swap edge (MIDI device *and* internal synth) | Medium | Emit pending note-off before adopting; test both output paths |
| Reversing Phase 4's deliberate non-copyable/non-movable invariant | Medium | Documented, spec-backed relaxation — not a silent workaround |
| Changing production generation breaks `ReproducibilityTests` goldens | Medium | Additive generator keeps old goldens; update deliberately if goldens are seed-derived |
| Layout budget: ~426/588px used, ~160px spare | Medium | Budget the GENERATION section (~100px) before wiring; grow the window if it does not fit |
| Size: forecast ~550–750 lines vs. Phase 9's ~680 | Medium | `single-pr` strategy already resolved; re-check at sdd-tasks |

## Rollback Plan

Single-commit revert. The change is additive at the UI/generator level; the only invasive edit is `SequencePlayer`'s sequence ownership — reverting restores `const Sequence` and the initialiser-list construction path. No persisted state, no file format, no migration.

## Dependencies

None external. Depends on existing `PitchGenerator`, `Scale`, `DeterministicRandom` (all unchanged).

## Success Criteria

- [ ] Generate, Randomize, and Lock Seed are usable while audio is running, with no hung note, dropout, assert, or crash.
- [ ] With Lock Seed on, repeated Generate yields a byte-identical `Sequence` every time.
- [ ] Randomize yields a different seed and a different `Sequence`, with the seed visible in the UI.
- [ ] The audio callback path stays allocation-free, log-free, and free of blocking locks (code review vs. the RT-safety constitution rule).
- [ ] Generated rhythms show skip-mask displacement, not uniform per-step probability.
- [ ] All four spec artifacts (1 new + 3 deltas) exist and no spec text still claims the Sequence is never modified after construction.
- [ ] Full test suite green.
