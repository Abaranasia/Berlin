# Exploration: Generation/Randomize (roadmap Phase 10)

## Current State

**Generation IS already wired into the live app — it is NOT fixture/hardcoded data.**
`MainComponent::buildSeededSequence()` (Source/MainComponent.cpp:38-51) composes:
```cpp
berlin::DeterministicRandom rng (kSeed);                              // kSeed = 12345 (const int, MainComponent.cpp:13)
auto sequence = berlin::RhythmGenerator (kNumSteps, 0.5f).generate (rng);  // kNumSteps = 16
berlin::PitchGenerator pitch (berlin::Scale::minor (48), 36, 72);
for (...) if (sequence[i].active) sequence[i].note = pitch.generateNextNote (rng);
```
This is called exactly ONCE, in `MainComponent`'s member-initializer list: `sequence (buildSeededSequence())`. `sequence` is `const berlin::Sequence` (MainComponent.h:49, "MUST precede `player`" per Decision 2), and `player` (`SequencePlayer`) is constructed from a copy of it, also stored `const` internally.

**This immutability is a formal spec requirement, not an implementation accident.** `openspec/specs/realtime-audio-wiring/spec.md` — Requirement "Sequence Built Before Audio Starts": the Sequence MUST be built on the message thread before `setAudioChannels`, and the audio thread "never reads one being generated or mutated concurrently." `SequencePlayer` (Source/playback/SequencePlayer.h) stores `const Sequence sequence` and is documented as intentionally non-copyable/non-movable (its `std::atomic<int> playhead` member disables both) "to force in-place initialiser-list construction... enforces fully-built-before-audio rather than a comment." There is currently NO API anywhere to swap/replace the live Sequence.

**Reproducibility contract is real and test-covered**: `Tests/Source/ReproducibilityTests.cpp` confirms same-seed -> identical Sequence end-to-end (RhythmGenerator draws first, ascending order, then PitchGenerator per active step — fixed composition order is the contract). Interestingly, `deterministic-generation` spec's design note ("no production class composes [RhythmGenerator+PitchGenerator]; 16-step demo is test-local only") is now STALE — MainComponent does compose them in production. That composition was added later (likely internal-synth or parameter-controls phase) without an accompanying spec delta making it explicit.

**`DeterministicRandom`** (Source/generation/DeterministicRandom.h) exposes ONLY `nextInt`, `nextFloat`, `getSeed()`. No setter/reseed method, and by spec (`deterministic-generation` Requirement "DeterministicRandom Explicit-Seed Wrapper") MUST NOT gain a default or time-seeded constructor. A "New Seed" feature must generate a new int64 seed VALUE externally (e.g. system random / time-based) and construct a fresh `DeterministicRandom(newSeed)` — this does not violate the existing contract.

**Generation parameters are all hardcoded local constants** in MainComponent.cpp: `kNumSteps=16`, density `0.5f`, `Scale::minor(48)`, pitch range `[36,72]`. None are exposed to any UI. The task's own framing (and proposal doc excerpts at lines ~540-570/~1385-1395/~1510-1520) scopes THIS phase to Generate/Randomize buttons + seed controls (seed display, New Seed, Lock Seed) only — root/scale/steps/density parameter exposure and the Mutation Engine (proposal §14: transpose/reverse/rotate/stretch on an existing sequence) are explicitly out of scope here.

**MainComponent current state (post Phase 9)**: already has `exportButton`, `statusLabel`, `synthToggle`, `fxToggle`, and ~19 parameter-control widgets (waveform/filter/envelope/LFO sliders+combos) laid out per "Berlin UI Pattern v1" (openspec/changes/archive/2026-09-06-parameter-controls/design.md): GUI control's `onValueChange`/`onClick` calls a forwarder method on the target object, which clamps and stores into a `std::atomic<T>` member, read once per audio block with `memory_order_relaxed`. Window is 800x600, already budgeted to ~426px of 588 usable rows (~160px spare) — a new GENERATION section needs its own layout budget check.

## Affected Areas
- `Source/MainComponent.h` / `.cpp` — owns `buildSeededSequence()`, the `sequence`/`player` members, and all UI wiring; needs new Generate/Randomize/seed-display/New-Seed/Lock-Seed controls plus a live-regeneration path.
- `Source/playback/SequencePlayer.h` / `.cpp` — `sequence` is `const`, non-copyable/non-movable by design; needs a new swap mechanism (or an explicit stop-then-reconstruct pattern) to accept a regenerated Sequence at runtime — this is the single largest structural change this phase requires.
- `Source/generation/DeterministicRandom.h` — may need an externally-generated-seed helper (not a change to the class itself, which must stay explicit-seed-only per spec).
- `Source/generation/PitchGenerator.h/.cpp`, `RhythmGenerator.h/.cpp` — API already sufficient (Scale/range/density/numSteps + DeterministicRandom&); no signature changes anticipated.
- `openspec/specs/realtime-audio-wiring/spec.md` — "Sequence Built Before Audio Starts" requirement currently forbids post-construction mutation; needs an explicit delta defining the live-regeneration contract (still audio-thread-safe).
- `openspec/specs/deterministic-generation/spec.md` — likely needs new requirements for seed selection/reseed/lock semantics; its stale "no production class composes these" design note should be corrected.
- `openspec/specs/sequencing-core/spec.md` — Step/Sequence/Scale types unaffected, no changes anticipated.
- Possibly a NEW capability spec (e.g. `generation-live-control`) is cleaner than overloading `realtime-audio-wiring` (a lifecycle spec) with runtime-mutation requirements — a call for sdd-propose/sdd-design, not resolved here.

## Approaches

1. **Stop-then-swap** — On Generate/Randomize, `player.stop()`, flush any pending note-off (reuse the existing `flushPendingNoteOff` pattern), build the new `Sequence` on the message thread, replace `player` (wrap it in `std::optional<SequencePlayer>` and re-emplace, or add a guarded `setSequence()` only callable while stopped), then `prepare()`+`start()` again.
   - Pros: simplest; audio thread never observes a partially-built Sequence because playback is stopped first; smallest diff to `SequencePlayer`'s current design; consistent with the project's simplicity-first precedent (no prior phase did live cross-thread structural swaps).
   - Cons: brief audible/MIDI gap while stopped; requires relaxing `SequencePlayer`'s "non-copyable/non-movable by construction" invariant (an explicit, documented design decision from Phase 4) — needs a conscious, justified spec/design amendment, not a silent workaround.
   - Effort: Medium.

2. **RT-safe live double-buffer swap** — Two `Sequence` buffers + an atomic index/pointer; message thread builds into the spare buffer and atomically publishes; audio thread checks the atomic once per block and switches its working pointer at a step boundary (never mid-note), still routing the currently-sounding note's note-off through the OLD sequence before switching.
   - Pros: no audible/playback gap; matches the juce-app-dev skill's explicit guidance for "GUI needs data generated on audio thread -> atomic scalar or lock-free ring/double-buffer for block data."
   - Cons: materially more complex — must define exact swap-boundary semantics (step-aligned, not sample-aligned) and prove no note-off is dropped at the swap edge; directly reverses `SequencePlayer`'s existing "one owner, one immutable snapshot" rationale, which was an intentional Phase 4 decision defended in its own header comment — this is a real design collision, not a free extension.
   - Effort: High.

3. **Hybrid ("fast stop-then-swap")** — Same mechanics as Approach 1, but sized/optimized (e.g. always at a step boundary, minimal restart latency) so the gap is imperceptible in practice; documented explicitly as "stop-restart, not lock-free," avoiding false claims of glitch-free swapping.
   - Pros: keeps Approach 1's simplicity and small diff while giving acceptable UX; honest about the mechanism instead of overselling RT-safety.
   - Cons: still has a real, if brief, transport stop; still needs the same `SequencePlayer` mutability relaxation as Approach 1.
   - Effort: Medium.

## Recommendation

Approach 1/3 (stop-then-swap, optionally polished for minimal perceived gap) for the MVP of this phase. It reuses 100% of the already-correct `PitchGenerator`/`RhythmGenerator`/`DeterministicRandom` composition (no algorithm changes needed), keeps the diff bounded to `MainComponent` + a narrow, explicit relaxation of `SequencePlayer`'s construction-time immutability, and matches the project's established pattern of choosing simplicity over premature lock-free complexity (no earlier phase needed live structural cross-thread swaps). Approach 2 should be deferred unless a future phase explicitly demands seamless/glitch-free regeneration mid-playback.

Open question for sdd-propose: define exact "Generate" vs "Randomize" vs "New Seed" semantics — proposal text lists all three as separate UI actions without defining the difference. Suggested reading: "Generate" re-runs generation with the CURRENT (possibly locked) seed; "Randomize"/"New Seed" pick a fresh seed first, then generate; "Lock Seed" suppresses reseeding so repeated "Generate" presses are idempotent/reproducible. This must be settled explicitly, not assumed by sdd-apply.

## Risks
- `SequencePlayer`'s non-copyable/non-movable-by-construction design (Phase 4 decision, defended in its own header comment) directly blocks any "swap the Sequence" feature as currently built — this is a genuine architectural collision this phase must resolve deliberately, not a trivial extension.
- `realtime-audio-wiring` spec's "Sequence Built Before Audio Starts" requirement currently states the Sequence is immutable after construction — this phase requires an explicit spec delta/amendment, or a new capability spec, not a silent contradiction.
- `deterministic-generation` spec's design note ("no production class composes RhythmGenerator+PitchGenerator") is already stale (MainComponent does compose them) — should be corrected as part of this phase's spec work regardless of scope decisions.
- Ambiguous UI semantics for Generate/Randomize/New Seed/Lock Seed (see Recommendation) — must be resolved in sdd-propose before design/tasks, or apply will guess.
- Layout budget: 800x600 window already at ~426/588px used; a new GENERATION section needs a fresh budget check per the parameter-controls precedent, or the window/layout must grow.
- Reusing `flushPendingNoteOff` semantics for a mid-playback Generate/Randomize (not just `releaseResources()`, its one current call site) needs verification that flushing then restarting doesn't double-fire or drop a note-off, especially for MIDI output (external device) vs the internal synth path.

## Ready for Proposal
Yes — investigation confirms this is a "wire the first live-regeneration capability" phase (medium-large scope), not a thin UI-only phase, because generation itself is already correctly wired but immutability is structurally enforced (spec + code) and must be deliberately relaxed. sdd-propose should explicitly scope: (a) Generate/Randomize/seed-lock UI, (b) the stop-then-swap live-regeneration mechanism in SequencePlayer/MainComponent, (c) the accompanying spec deltas to `realtime-audio-wiring` and `deterministic-generation`, while explicitly excluding root/scale/steps/density parameter exposure and the Mutation Engine as future phases.
