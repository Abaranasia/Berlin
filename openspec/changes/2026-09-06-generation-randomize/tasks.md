# Tasks: Generation / Randomize (roadmap Phase 10)

## Review Workload Forecast

| Field | Value |
|-------|-------|
| Estimated changed lines | 550-750 |
| 400-line budget risk | High |
| Chained PRs recommended | No (session budget is 800; single-pr strategy in effect) |
| Suggested split | Single PR, phases below are internal commit checkpoints, not separate PRs |
| Delivery strategy | single-pr |
| Chain strategy | size-exception |

Decision needed before apply: Yes
Chained PRs recommended: No
Chain strategy: size-exception
400-line budget risk: High

### Suggested Work Units

| Unit | Goal | Likely PR | Focused test command | Runtime harness | Rollback boundary |
|------|------|-----------|----------------------|-----------------|-------------------|
| 1 | `Sequence::swap` + `SkipMaskGenerator` (pure logic, no wiring) | PR 1 (single PR, phase checkpoint) | `BerlinTests.exe --category=Berlin --filter="*Sequence*,*SkipMask*"` | N/A — pure unit-tested logic, no audio device needed | Revert `Sequence.h/.cpp` swap addition + delete `SkipMaskGenerator.*`/its tests; zero callers exist yet |
| 2 | `SequencePlayer` handoff (`publishSequence`, adoption in `process()`) | PR 1 (same PR, phase checkpoint) | `BerlinTests.exe --category=Berlin --filter="*SequencePlayerHandoff*"` | Manual: run Standalone app, no wiring calls `publishSequence` yet so behavior is unchanged | Revert `SequencePlayer.h/.cpp` to `const Sequence`; existing callers (`MainComponent`) unaffected until Unit 3 |
| 3 | `MainComponent` wiring: seed state, controls, `regenerate()`, layout, Export fix | PR 1 (same PR, final checkpoint) | `BerlinTests.exe --category=Berlin` (full suite) | Manual: launch Standalone, click Generate/Randomize/Lock Seed while audio runs, on internal synth AND external MIDI device | Revert `MainComponent.h/.cpp`; Units 1-2 remain inert (no dead code — `SkipMaskGenerator`/`publishSequence` simply gain no caller) |

## Phase 1: `Sequence::swap` (sequencing-core delta)

- [x] 1.1 RED: `Tests/Source/SequenceTests.cpp` — add cases for `A.swap(B)` exchanging contents/sizes, and a `noexcept` compile-time check (`static_assert(noexcept(a.swap(b)))`). Spec: `sequencing-core` "Sequence Supports O(1) Swap".
- [x] 1.2 GREEN: `Source/core/Sequence.h` add `void swap (Sequence& other) noexcept;`; `Source/core/Sequence.cpp` implement via `steps.swap (other.steps)`.
- [x] 1.3 Verify: run `BerlinTests.exe --category=Berlin --filter="*Sequence*"` green.

## Phase 2: `SkipMaskGenerator` (deterministic-generation delta)

- [x] 2.1 RED: create `Tests/Source/SkipMaskGeneratorTests.cpp` covering: exact `activeSteps` count; size `== numSteps`; step 0 never skipped; `note` field untouched (stays default); `activeSteps` clamped at both ends (`0 → 1`, `> numSteps → numSteps`); same-seed → identical mask; different seeds → differing masks; exactly `numSkips` RNG draws consumed. Spec: `deterministic-generation` "SkipMaskGenerator Produces a Deterministic Displacement Pattern".
- [x] 2.2 Register the new test file's `<FILE>` entry in `Tests/BerlinTests.jucer` (build fails to compile it otherwise).
- [x] 2.3 GREEN: create `Source/generation/SkipMaskGenerator.h` (`SkipMaskGenerator(int numSteps, int activeSteps)`, `Sequence generate(DeterministicRandom&) const`) and `SkipMaskGenerator.cpp` implementing Decision 3's algorithm — full pulse train, step-0-anchored scratch array, partial Fisher-Yates over `numSkips = numSteps - activeSteps` draws.
- [x] 2.4 Register `SkipMaskGenerator.h`/`.cpp` `<FILE>` entries in `Berlin.jucer` (matches existing `RhythmGenerator.h/.cpp` entry pattern at `Berlin.jucer:23-24`).
- [x] 2.5 GREEN: add a `SkipMaskGenerator` end-to-end golden to `Tests/Source/ReproducibilityTests.cpp` (mask + `PitchGenerator` composed against one seed) — do NOT touch the existing `RhythmGenerator` golden.
- [x] 2.6 Verify: `BerlinTests.exe --category=Berlin --filter="*SkipMask*,*Reproducibility*"` green. Confirm `RhythmGenerator.*` files are untouched (no diff).

## Phase 3: `SequencePlayer` handoff (realtime-audio-wiring delta) — highest-risk phase

- [x] 3.1 RED: create `Tests/Source/SequencePlayerHandoffTests.cpp` per design's Testing Strategy table: `publishSequence` returns true then false when unadopted; after one `process()` call events come from the new sequence; a sounding note yields a note-off at offset 0 as the FIRST event before any new note-on; nothing sounding → no swap-edge note-off; playhead/next note-on is step 0 after adoption; adoption is one-shot (a second `process()` call does not re-adopt); a different-length sequence wraps on the new size. All deterministic, no real audio device.
- [x] 3.2 Register the new test file's `<FILE>` entry in `Tests/BerlinTests.jucer`.
- [x] 3.3 GREEN: `Source/playback/SequencePlayer.h` — drop `const` from `sequence`; add `Sequence pendingSequence;` and `std::atomic<bool> sequencePending { false };` members; declare `bool publishSequence (Sequence& incoming) noexcept;`; rewrite the class-doc comment's "one immutable snapshot" rationale to "audio-thread-exclusive after construction, replaced only via publish/adopt" per Decision 1's accepted-consequence wording. Do NOT add any call to `flushPendingNoteOff` from this path — the swap-edge note-off is pushed inline in `process()` (see 3.4).
- [x] 3.4 GREEN: `Source/playback/SequencePlayer.cpp` — implement `publishSequence` (swap `incoming` into `pendingSequence`, `store(true, release)`, return false without modifying `incoming` if already pending); add the adoption block at the TOP of `process()` per design's exact code (Decision 1): acquire-load `sequencePending`, push note-off at offset 0 first if `pendingNote >= 0`, `sequence.swap(pendingSequence)`, reset `pendingNote/pendingStep/transport/playhead`, release-store `false`.
- [x] 3.5 Add the compile-time checks from design's Testing Strategy: `static_assert(noexcept(player.publishSequence(s)))`, `static_assert(noexcept(process(...)))` equivalent, `static_assert(std::atomic<bool>::is_always_lock_free)`.
- [x] 3.6 Verify: `BerlinTests.exe --category=Berlin --filter="*SequencePlayerHandoff*"` green.
- [x] 3.7 RT-safety review (juce-app-dev): confirm the adoption block performs no allocation, no lock, no logging; confirm `Sequence::swap` never touches the allocator; confirm `pendingSequence`'s superseded buffer is only ever freed on the message thread (at the next `publishSequence` call), never on the audio thread.

## Phase 4: `MainComponent` wiring (generation-live-control delta)

- [ ] 4.1 `Source/MainComponent.h` — rename `const berlin::Sequence sequence` to non-const `berlin::Sequence currentSequence` (keep its position preceding `player` in the member list per Decision 2's ordering rule); add `juce::int64 currentSeed;`; declare `static berlin::Sequence buildSeededSequence (juce::int64 seed);` (replaces the no-arg overload); declare `void regenerate (bool drawNewSeed);`; add member controls: `juce::TextEditor seedEditor;`, `juce::TextButton generateButton { "Generate" };`, `juce::TextButton randomizeButton { "Randomize" };`, `juce::ToggleButton lockSeedToggle { "Lock Seed" };`; add `juce::Label generationSectionLabel;`.
- [ ] 4.2 `Source/MainComponent.cpp` — replace `#include "generation/RhythmGenerator.h"` with `#include "generation/SkipMaskGenerator.h"`; add `kActiveSteps = 11` constant alongside existing `kNumSteps = 16`; rewrite `buildSeededSequence(juce::int64 seed)` to construct `DeterministicRandom(seed)` and call `SkipMaskGenerator(kNumSteps, kActiveSteps).generate(rng)` in place of `RhythmGenerator(kNumSteps, 0.5f)`, keeping the existing `PitchGenerator` pass over active steps unchanged.
- [ ] 4.3 `Source/MainComponent.cpp` constructor — initialize `currentSeed (kSeed)`, `currentSequence (buildSeededSequence (currentSeed))`, `player (currentSequence, ...)` (same ordering, renamed member); wire `seedEditor` to show `currentSeed` as text; `generateButton.onClick` calls `regenerate(false)`; `randomizeButton.onClick` calls `regenerate(true)` (no-op/disabled while `lockSeedToggle` is on); `lockSeedToggle` default off, toggling it enables/disables `randomizeButton`; `seedEditor.onFocusLost`/`onReturnKey` parses the field as a signed integer — on parse failure, restore the previous seed text and set `statusLabel` to report the rejection (per Decision 4's validation contract); on success do NOT auto-regenerate (typing alone doesn't regenerate, per spec).
- [ ] 4.4 Implement `MainComponent::regenerate (bool drawNewSeed)`: if `drawNewSeed && !lockSeedToggle.getToggleState()`, draw `currentSeed = juce::Random::getSystemRandom().nextInt64()` and update `seedEditor`'s text; build `auto next = buildSeededSequence (currentSeed)`; call `player.publishSequence (next)` — on `false` (unadopted previous publish), set `statusLabel` to report "busy, try again" and do NOT commit `currentSequence`; on `true`, set `currentSequence = std::move (next)` so Export (`MainComponent.cpp:348`, `berlin::buildMidiExportTimeline (sequence, ...)` — update this call site to read `currentSequence`) stays authoritative for the CURRENT pattern, not the stale one.
- [ ] 4.5 `Source/MainComponent.cpp::resized()` — add the `GENERATION` section per Decision 5's exact placement (after the toggle row, before the two-column split): section label row (28px), seed row (28+6px, label + `seedEditor`), button row (28+6px, `generateButton` + `randomizeButton` + `lockSeedToggle`, `3 x kButtonWidth(140) = 420 <= 776`). No change to `setSize(800, 600)`.
- [ ] 4.6 Update `Berlin.jucer` if any new `<FILE>` entries are needed (none expected for `MainComponent.h/.cpp` — already registered); confirm `SkipMaskGenerator.h/.cpp` entries from Phase 2 are present.

## Phase 5: Manual audibility/correctness gate (human-only, not automatable)

- [ ] 5.1 Launch the Standalone build; click Generate/Randomize/Lock Seed repeatedly while audio is running — confirm no hung note, dropout, assert, or crash on the internal synth.
- [ ] 5.2 Repeat 5.1 routed to an external MIDI device — confirm the same, and confirm the swap-edge note-off reaches the external device.
- [ ] 5.3 Type a specific seed into the seed field, press Generate, note the pattern; type it again, press Generate again — confirm byte-identical pattern (round-trip).
- [ ] 5.4 With Lock Seed on, click Randomize — confirm the seed field does not change and no new pattern is generated.
- [ ] 5.5 After a regeneration, use Export — confirm the exported MIDI file reflects the CURRENT (post-regeneration) pattern, not the pattern from app startup.

## Phase 6: Final cleanup & verification

- [ ] 6.1 Run the full suite: `BerlinTests.exe --category=Berlin` exits 0.
- [ ] 6.2 Confirm byte-for-byte no diff on out-of-scope tiers: `Source/synth/*` (except any required call-site update already covered above), `Source/midi/*`, `Source/export/*`, `Source/generation/RhythmGenerator.*`, `PitchGenerator.*`, `DeterministicRandom.h`, `Source/playback/Transport.h/.cpp` (design explicitly requires zero changes — do not "fix" `Transport::running`'s plain-`bool` typing).
- [ ] 6.3 Confirm `RhythmGenerator` retains zero production call sites but its own tests (`RhythmGeneratorTests.cpp`) and golden in `ReproducibilityTests.cpp` remain green and untouched.
- [ ] 6.4 Update the `deterministic-generation` design note that previously claimed no production class composes the generators (per design's Verified Findings table) — reconcile spec/comment wording during apply if any stale in-code comment still says otherwise.
- [ ] 6.5 Spec merge: apply the four spec deltas (`realtime-audio-wiring`, `deterministic-generation`, `sequencing-core`, `generation-live-control`) into their base specs per `openspec-convention.md`.

## Rules Applied

- Strict TDD: Phases 1-3 are RED-before-GREEN (`SequenceTests`, `SkipMaskGeneratorTests`, `SequencePlayerHandoffTests`).
- `flushPendingNoteOff()` is NOT called from the regeneration path — the swap-edge note-off is a small inline duplicate inside `process()` per design Decision 1. Note: `generation-live-control/spec.md`'s prose (lines 13, 59) still literally says "composing `RhythmGenerator`" and "flushes ... via `flushPendingNoteOff`" — these are stale/unreconciled against the design and are superseded by design's actual code; Phase 6.5's spec merge must correct this wording, not just apply it verbatim.
- `Transport.h/.cpp` unchanged (Decision 1 removes the need).
- `Berlin.jucer`/`Tests/BerlinTests.jucer` DO need `<FILE>` registration updates (unlike Phase 9).
