# Design: Generation / Randomize (roadmap Phase 10)

Artifact store: hybrid. Engram: `sdd/generation-randomize/design`. Every line/behaviour cited below was read from the working tree, not recalled.

## Verified findings that shaped this design

| Proposal / exploration claim | Verified reality | Source |
|---|---|---|
| Naive `stop()` → swap from the message thread is a real race | **Confirmed, and worse than stated.** `Transport::running` is a plain `bool` written by `stop()` and read by `countBoundaries`/`advance` — formal UB. But the decisive point is that `stop()` does **not** wait for an in-flight callback: after it returns, `process()` may still be mid-block reading `sequence[stepIndex]`. Stopping is not a drain barrier. | `Transport.h:66`, `Transport.cpp:42-45,74,108`; `SequencePlayer.cpp:47-73` |
| `Transport::running` may need to become atomic | **Not needed.** With adoption inside `process()` (Decision 1) nothing calls `start()`/`stop()` at runtime. The only production writes stay in the ctor at `MainComponent.cpp:175`, *before* `setAudioChannels` at `:188`. `Transport` is untouched this phase. | `MainComponent.cpp:175,188` |
| Regeneration adds a second `flushPendingNoteOff` call site | **Avoided.** The swap-edge note-off is pushed by `process()` itself into `blockEvents`, which `midiTranslator.translate` **and** `synth.render` both already consume in the same block — so *both* output paths are covered with zero new plumbing. `step-event-scheduling:101` ("only production call site MUST be `releaseResources()`") stays true and needs **no delta**. | `MainComponent.cpp:223-226,237`; `step-event-scheduling/spec.md:99-101` |
| `sequencing-core` is unaffected | **False.** The swap needs `Sequence::swap` (the vector is private). Small additive delta required. | `core/Sequence.h:39-41` |
| Only `player` holds a Sequence | **False, and a latent bug.** `MainComponent::sequence` is *also* the MIDI-export source. Regenerating only the player would make Export write the stale pattern. | `MainComponent.cpp:348` |
| A skip mask contradicts an existing spec requirement | **Confirmed.** `deterministic-generation` requires `active` be set "by an independent probability equal to `density`, NOT by a guaranteed/exact active-step count" — exactly what a mask produces. | `deterministic-generation/spec.md:67` |

## Architecture Decisions

### Decision 1 — `SequencePlayer` owns the primitive: publish-into-staging-slot, adopt-by-swap inside `process()`

**Choice**: two new members and one new message-thread method. Playback never stops.

```cpp
// MESSAGE THREAD. Swaps `incoming` into the staging slot. Returns false iff a
// previous publish has not yet been adopted (then nothing is published).
bool publishSequence (Sequence& incoming) noexcept;
```
```cpp
void SequencePlayer::process (int numSamples, StepEventBuffer& out) noexcept
{
    out.clear();

    if (sequencePending.load (std::memory_order_acquire))
    {
        if (pendingNote >= 0)
            out.push ({ 0, pendingStep, pendingNote, false });   // swap-edge note-off, offset 0, FIRST

        sequence.swap (pendingSequence);   // O(1) pointer swap: no alloc, no free, no lock
        pendingNote = -1;  pendingStep = 0;
        transport.reset();                 // restart from step 1 (preserves running state)
        playhead.store (0, std::memory_order_relaxed);
        sequencePending.store (false, std::memory_order_release);
    }
    ... existing boundary loop and transport.advance() unchanged ...
}
```

**Alternatives**: `MainComponent` orchestrating `stop → flush → rebuild → prepare → start` (rejected: `stop()` is not a drain barrier, so the swap still races a live `process()`; and the message thread would have to write `blockEvents`/`midiBlock`, which the audio thread owns — a *second* race); `std::optional<SequencePlayer>` re-emplace (destroys the object under the audio thread); double-buffer + atomic index (two live buffers whose reuse safety depends on the audio thread having left the old one — needs an ack counter, strictly more state); `juce::SpinLock::tryEnter` (permitted by the skill, but buys nothing the flag doesn't already give).

**Rationale**: the invariant becomes *stronger and simpler* than "immutable" — **`sequence` is only ever read or replaced by the audio thread**; the message thread only ever touches `pendingSequence`, and only while `sequencePending == false`. The acquire-load of `false` synchronises-with the release-store of `false` that follows the swap, so the swap happens-before the next publish. Release ordering on the publish makes the fully-built sequence visible before the flag.

**Why `swap` and not move-assign**: after the swap, `pendingSequence` holds the *old* buffer. The next `publishSequence` swaps it out into a message-thread local, which frees it **on the message thread**. `std::swap(Sequence&,Sequence&)` would move-assign through null buffers, i.e. touch the allocator on the audio thread; an explicit `steps.swap()` cannot.

**Accepted consequences**: (a) `const Sequence sequence` loses its `const` — the Phase 4 "one owner, one immutable snapshot" rationale is rewritten to "one owner, one audio-thread-exclusive snapshot"; the `std::atomic<int> playhead` still keeps the class non-copyable/non-movable, so in-place ctor is still compiler-enforced. (b) `publishSequence` can return `false`; the window is one audio block (≤ ~20 ms), unreachable at human click rate. `MainComponent` reports it in `statusLabel` rather than silently dropping. (c) If the device is closed, a publish stays pending until audio resumes — a liveness wart in a state that is already silent; documented, not fixed. (d) One extra event per block against `StepEventBuffer::capacity == 64` — no overflow risk.

### Decision 2 — New `SkipMaskGenerator`; `RhythmGenerator` and its spec requirement are untouched

**Choice**: `Source/generation/SkipMaskGenerator.h/.cpp`, `SkipMaskGenerator (int numSteps, int activeSteps)`, `Sequence generate (DeterministicRandom&) const`.

**Alternatives**: a `style` enum on `RhythmGenerator` (rejected — it would **supersede** `deterministic-generation:67`, break its 3 scenarios, break `RhythmGeneratorTests`, and break the `ReproducibilityTests` golden, all to save one file); deleting `RhythmGenerator` (rejected — density-as-probability is a spec'd, tested capability that the deferred density-UI phase wants).

**Rationale**: this is the Phase-9-`pulseWidth`-style call. A mask *does* produce the exact active-step count that requirement forbids, so making `RhythmGenerator` the implementing class demands a real, deliberate supersession of a requirement that is otherwise still correct and still wanted. A new class gets its own contract; nothing existing changes meaning.

**Accepted consequence**: `RhythmGenerator` has **no production call site** after this change. Retained deliberately as a spec'd, tested capability — and the `deterministic-generation` design note that already wrongly claims no production class composes the generators gets corrected in the same delta.

### Decision 3 — Mask algorithm: all-on pulse train, anchored step 0, partial Fisher–Yates skip selection

Sourced from research Finding 3 (`docs/research/berlin_school_sequence_conventions.md`, MIDIbox skip-trigger technique: mark steps to skip, displacing a 16-step grid to ~11 effective steps).

```cpp
Sequence sequence (numSteps);
for (int i = 0; i < numSteps; ++i) sequence[i].active = true;   // start from a full pulse train

// indices scratch = [1 .. numSteps-1]; step 0 is the phase anchor and is never skipped
const int numSkips = numSteps - activeSteps;                    // activeSteps clamped to [1, numSteps]
for (int i = 0; i < numSkips; ++i)                              // exactly one draw per skip
{
    const int j = i + random.nextInt ((numSteps - 1) - i);
    std::swap (scratch[i], scratch[j]);
    sequence[scratch[i]].active = false;
}
```

| Property | Value |
|---|---|
| Parameter surface | `density: float` → `activeSteps: int`, clamped to `[1, numSteps]` |
| Output size | exactly `numSteps` (existing "Output size matches numSteps" scenario is unchanged in spirit) |
| Active count | **exactly** `activeSteps` — this is the observable, testable difference from per-step probability |
| RNG draws | exactly `numSkips`, ascending `i` order — the reproducibility contract |
| Fields written | `active` only; `note` left at default (same as `RhythmGenerator`) |
| Production values | `kNumSteps = 16`, `kActiveSteps = 11` — the research's cited 16→11 displacement |

**Rationale**: subtracting from a full pulse train (rather than flipping independent coins) is the essence of the technique and is what makes the result *feel* like displacement rather than noise; anchoring step 0 gives the odd active count a stable phase reference to displace *against*. Step-0 anchoring is the one aesthetic judgement here and is called out so spec/tasks carry it explicitly rather than rediscover it.

### Decision 4 — `generation-live-control`: three controls, one seed field, message-thread-only seed state

| Control | Type | Contract |
|---|---|---|
| Seed field | `juce::TextEditor` | Shows the current seed. On commit, text MUST parse as a signed integer (non-empty, `[-0-9]` only); otherwise the previous seed is restored into the field and `statusLabel` reports the rejection. Editing alone does not regenerate. |
| `Generate` | `TextButton` | Regenerates from the seed currently in the field. Same seed → byte-identical `Sequence`. |
| `Randomize` | `TextButton` | Draws a fresh seed from `juce::Random::getSystemRandom().nextInt64()`, writes it into the field, then regenerates. No-op (and disabled) while `Lock Seed` is on. |
| `Lock Seed` | `ToggleButton` | Default off. On: suppresses reseeding, so repeated `Generate` is reproducible. |

`Randomize` and the proposal's separate "New Seed" collapse into **one** button (the proposal's own table already treats them as one row) — fewer controls, and it fits the layout budget.

**Berlin UI Pattern v1 conformance**: each handler calls one forwarder that validates/clamps then crosses the thread seam. The seam here is `player.publishSequence` rather than a `std::atomic<T>` scalar, per the `juce-app-dev` decision gate ("atomics for scalars, a lock-free structure for block data"). `currentSeed` is a plain `juce::int64` — message-thread only, never read by the audio thread, so no atomic is warranted.

**`DeterministicRandom` is untouched**: the fresh seed is a *value* produced externally, which is exactly what its explicit-seed-only requirement permits.

### Decision 5 — `GENERATION` section goes in the header block; no window resize

Measured from `resized()` (`MainComponent.cpp:255-304`) with `kMargin 12`, `kControlHeight 28`: usable height `600 - 24 = 576`. Header (export + status + toggles, each `28 + 6`) = **102**. The taller of the two columns is the right one (ENVELOPE `28 + 4×34 = 164` + LFO `28 + 3×34 = 130`) = **294**. Total **396**, leaving **180 px**.

`GENERATION` costs **96 px**: section label `28`, seed row `28 + 6`, button row `28 + 6`. Placed after the toggle row and before the two-column split → `396 + 96 = 492 ≤ 576`, **84 px slack**. Button row: `3 × kButtonWidth (140) = 420 ≤ 776`. Fits.

**Alternative**: append to the left column, which has ~102 px more slack than the right. Rejected — it would file a global, transport-level action visually under `OSCILLATOR`/`FILTER`. **Rationale**: `GENERATION` belongs with Export/status/toggles (global actions), and the header has the budget. No change to `setSize (800, 600)`.

## Data Flow

```
MESSAGE THREAD                                          AUDIO THREAD
  Generate / Randomize click
        │
        ├─ seed: field text (validated) or system RNG
        ├─ buildSeededSequence (seed)
        │     SkipMaskGenerator(16, 11).generate(rng)
        │     → PitchGenerator per active step        (draw order = reproducibility contract)
        ├─ forPlayer = next                (copy; export source stays authoritative)
        ├─ player.publishSequence (forPlayer)
        │      pendingSequence.swap(forPlayer)
        │      sequencePending.store(true, release) ──────────┐
        │      (false → statusLabel "busy", no commit)        │
        └─ currentSequence = std::move (next)   ← Export      │
                                                              ▼
                                             process(): if (pending, acquire)
                                               push note-off @0  ──┐
                                               sequence.swap(pendingSequence)
                                               transport.reset(); playhead = 0
                                               pending.store(false, release)
                                             ... boundary loop ...  │
                                                                    ▼
                                          blockEvents ──┬─→ midiTranslator → midiSink  (external MIDI)
                                                        └─→ synth.render               (internal synth)
```

Both output paths consume the same `blockEvents`, so the single swap-edge note-off covers the external device **and** the internal voice. Old buffer memory is freed on the **message thread** at the next publish.

## File Changes

| File | Action | Description |
|---|---|---|
| `Source/generation/SkipMaskGenerator.h` / `.cpp` | Create | Decision 2/3. JUCE-free apart from `DeterministicRandom` |
| `Source/core/Sequence.h` / `.cpp` | Modify | Additive `void swap (Sequence& other) noexcept` → `steps.swap` |
| `Source/playback/SequencePlayer.h` / `.cpp` | Modify | `pendingSequence`, `std::atomic<bool> sequencePending`, `publishSequence`; adoption at top of `process()`; drop `const` on `sequence`; rewrite the header immutability rationale |
| `Source/MainComponent.h` / `.cpp` | Modify | `sequence` → non-const `currentSequence` (still MUST precede `player`); `juce::int64 currentSeed`; `buildSeededSequence (juce::int64)`; `regenerate()`; seed field + 3 controls; `GENERATION` in `resized()` |
| `Source/playback/Transport.h` / `.cpp` | **Unchanged** | Decision 1 removes the need — recorded so apply does not "fix" `running` |
| `Source/generation/RhythmGenerator.*`, `DeterministicRandom.h`, `PitchGenerator.*` | **Unchanged** | Decision 2/4 |
| `Tests/Source/SkipMaskGeneratorTests.cpp` | Create | Mask contract |
| `Tests/Source/SequencePlayerHandoffTests.cpp` | Create | Publish/adopt contract |
| `Tests/Source/ReproducibilityTests.cpp` | Modify | Add a `SkipMaskGenerator` end-to-end golden; existing `RhythmGenerator` golden untouched |
| `Tests/Source/SequenceTests.cpp` | Modify | `swap` semantics |
| `Berlin.jucer`, `Tests/BerlinTests.jucer` | Modify | `<FILE>` registrations for every new `.h`/`.cpp` (a miss fails loudly as an unresolved external) |

Specs: **new** `generation-live-control` (UI contracts + the publish/adopt handoff contract, incl. the swap-edge note-off ordering); **modified** `realtime-audio-wiring` ("never modified afterward" → "read or replaced only by the audio thread; the message thread publishes into a separate staging slot"), `deterministic-generation` (add the `SkipMaskGenerator` requirement, correct the stale composition design note), `sequencing-core` (add `Sequence::swap`); **`step-event-scheduling` needs no delta** — see the findings table. This last point contradicts the proposal and must be reconciled with `sdd-spec`.

## Interfaces / Contracts

```cpp
// SequencePlayer.h
class SequencePlayer
{
public:
    // MESSAGE THREAD ONLY. Swaps `incoming` into the staging slot and publishes it.
    // Returns false iff a previous publish is still unadopted; then nothing is published
    // and `incoming` is unmodified. The audio thread adopts at the top of the next
    // process(): it emits a note-off at offset 0 for any sounding note, swaps the
    // sequence in (O(1), no alloc/free/lock), and restarts the pattern from step 0.
    bool publishSequence (Sequence& incoming) noexcept;

private:
    Sequence sequence;               // AUDIO-THREAD-EXCLUSIVE after construction
    Sequence pendingSequence;        // staging slot; message thread only while !sequencePending
    std::atomic<bool> sequencePending { false };
};

// Sequence.h
void swap (Sequence& other) noexcept;   // O(1); no allocator interaction (audio-thread safe)

// SkipMaskGenerator.h
class SkipMaskGenerator
{
public:
    SkipMaskGenerator (int numSteps, int activeSteps);   // activeSteps clamped to [1, numSteps]
    Sequence generate (DeterministicRandom& random) const;
};
```

## Testing Strategy

| Layer | What to Test | Approach |
|---|---|---|
| Unit — mask | exact `activeSteps` count; size `== numSteps`; step 0 never skipped; `note` untouched; `activeSteps` clamped at both ends (0 → 1, > numSteps → numSteps); `numSkips` draws consumed | `SkipMaskGeneratorTests.cpp`, RED first |
| Unit — determinism | same seed → identical mask; different seeds → differing masks; end-to-end mask + `PitchGenerator` golden | `SkipMaskGeneratorTests` + `ReproducibilityTests` addition |
| Unit — handoff | `publishSequence` true then false (unadopted); after one `process()` events come from the NEW sequence; a sounding note yields a note-off at offset 0 as the **first** event, before any new note-on; nothing sounding → no swap-edge note-off; playhead/next note-on is step 0; adoption is one-shot; a **different-length** sequence wraps on the new size | `SequencePlayerHandoffTests.cpp`, RED first — deterministic, no real audio device |
| Unit — compile-time | `static_assert (noexcept (player.publishSequence (s)))`, `noexcept (process(...))`, `std::atomic<bool>::is_always_lock_free` | in-test `static_assert` |
| Unit — core | `Sequence::swap` exchanges contents and sizes; `noexcept` | `SequenceTests.cpp` |
| Code review | adoption block has no allocation, no lock, no logging; free happens only on the message thread | RT-safety constitution review, per `realtime-audio-wiring`'s code-review-verified requirements |
| Manual gate | Generate / Randomize / Lock Seed while audio runs → no hung note, dropout, assert, or crash, on the internal synth **and** an external MIDI device; seed field round-trips; Export writes the *current* pattern | Manual smoke gate, same accepted tradeoff as Phases 4/5/8 |

## Threat Matrix

N/A — no routing, shell command, subprocess, VCS/PR automation, executable-file classification, or process-integration boundary. The seed text field is parsed as an integer and never executed; its validation is a correctness concern (Decision 4), not an injection surface.

## Migration / Rollout

No migration required. No persisted state, no file format, no schema. Single-commit revert restores `const Sequence` and initialiser-list construction.

## Open Questions

- [ ] `step-event-scheduling` delta: this design concludes **none is needed** (the swap-edge note-off lives in `process()`, not in a second `flushPendingNoteOff` call site) while the proposal lists it as modified. Reconcile with `sdd-spec` before `sdd-tasks`.
- [ ] `sequencing-core` delta for `Sequence::swap` is **new** relative to both proposal and exploration — confirm it is in scope rather than deferred.
