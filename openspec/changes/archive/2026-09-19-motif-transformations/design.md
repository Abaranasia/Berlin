# Design: Motif Transformations (invert, augment/diminish, palindrome)

## Technical Approach

Append three free functions to `berlin::MutationEngine` with the existing
`Sequence (*)(const Sequence&, DeterministicRandom&)` signature, append them to
`transforms[]` (8 → 11), and leave `applyRandomTransform`'s single-selection-draw
dispatch byte-for-byte unchanged. Zero new files, zero changes to `Step`,
`Sequence`, `SequencePlayer`, or `MainComponent`.

**JUCE thread confirmation (juce-app-dev skill).** `applyRandomTransform` is
reached only from `MainComponent::mutate()`, called by `mutateButton.onClick`
and `timerCallback()` — both message thread. Results cross to audio only through
the existing lock-free `player.publishSequence` swap. The skill's hard rules
(no allocation/locks in `getNextAudioBlock`, `ScopedNoDenormals`, atomics) are
**N/A**: this change touches no audio-thread or DSP path. Heap allocation inside
the transforms is legitimate here and matches all 8 existing transforms.

## Architecture Decisions

| Decision | Choice | Alternatives rejected | Rationale |
|---|---|---|---|
| Axis + range-correction code sharing | Two file-local helpers in an **anonymous namespace** in `MutationEngine.cpp`, not declared in the header | (a) duplicate the clamp-once math in both transforms; (b) public header helpers | Duplicating the `[0,127]` correction twice is the single likeliest source of a per-note-clamp regression. Keeping helpers `.cpp`-private holds the public header surface to exactly 3 new declarations, as the proposal states. |
| Helper shape | Plain functions, no templates: `firstActiveNote(const Sequence&, int& axis) -> bool` and `commitWithUniformOffset(Sequence& result, const std::vector<int>& mappedNotes) -> bool` | Template taking a mapping lambda | The codebase uses no templates in this module; two tiny non-generic functions read the same as the existing `transpose` body. |
| Table position | **Append** `invert, scaleIntervals, palindrome` after `compress` in header, `.cpp`, and `transforms[]` | Group `invert` next to `transpose` (pitch family) | Append is the minimal diff and preserves indices 0–7; reordering has no benefit because `nextInt(11)` remaps every index anyway. |
| `scaleIntervals` draw ordering | Draw `nextInt(2)` **first, unconditionally**, then the no-op guard | Guard first, draw only when there is work | Mirrors `transpose` exactly (`.cpp:21-24`): RNG stream length must stay independent of content. Guarding first would desynchronise the mutation chain. |
| `palindrome` size clamp | Inline `std::clamp(n * 2, 4, 64)` | Extract a shared clamp helper with `stretch`/`compress` | **No such helper exists today** — `stretch` (`.cpp:158`) and `compress` (`.cpp:176`) both inline `std::clamp`. Following the existing precedent beats introducing an abstraction for three call sites. |

## Data Flow

    Mutate button / Auto-Evolve timer
             │
             ▼
    MainComponent::mutate()  ──▶ mutationSeed(seed, gen) ──▶ DeterministicRandom
             │
             ▼
    applyRandomTransform ── nextInt(11) ──▶ transforms[0..10]
             │                               [0..7] existing, bodies untouched
             │                               [8]  invert         ─┐ axis = first active note
             │                               [9]  scaleIntervals ─┘ + ONE uniform offset
             │                               [10] palindrome       clamp(2n, 4, 64)
             ▼
    publishSequence(copy)  +  currentSequence = result

## File Changes

| File | Action | Description |
|---|---|---|
| `Source/generation/MutationEngine.h` | Modify | 3 declarations + doc comments appended after `compress` (line 59) |
| `Source/generation/MutationEngine.cpp` | Modify | 2 anon-namespace helpers; 3 implementations appended after `compress` (line 182); `transforms[]` (line 188) 8 → 11 |
| `Tests/Source/MutationEngineTests.cpp` | Modify | See Testing Strategy |

No new includes: `<algorithm>` (`std::clamp`) and `<vector>` are already in use.

## Interfaces / Contracts

```cpp
// Mirrors active notes about an axis: n' = 2a - n, where a is the note of the
// FIRST active step. Inactive steps untouched, size unchanged. No draw. Range
// restored by ONE uniform offset (never per-note). An involution when no
// corrective offset applies. No-op copy with zero active steps.
Sequence invert (const Sequence& input, DeterministicRandom& random);

// Augment/diminish as PITCH-INTERVAL scaling about the same axis a:
// augment n' = a + 2(n - a), diminish n' = a + (n - a) / 2 (C++ truncation,
// toward the axis). Exactly one draw (nextInt(2)), always, before any guard -
// same invariant as transpose. No-op copy with zero active steps OR when the
// scaled span exceeds 127, where no single uniform shift fits.
Sequence scaleIntervals (const Sequence& input, DeterministicRandom& random);

// Forward then backward mirror, pivot NOT shared: result[i] = input[j] with
// j = (i < n) ? i : 2n - 1 - i, wrapped via ((j % n) + n) % n. No draw.
// targetSize = std::clamp(n * 2, 4, 64), stretch/compress's precedent.
// n == 0 -> early-return copy (the modulo would be UB).
Sequence palindrome (const Sequence& input, DeterministicRandom& random);
```

Helper contract: `commitWithUniformOffset` returns `false` and leaves `result`
untouched when `max - min > 127`; the caller then returns the unmodified copy.
For `invert` this branch is unreachable with in-range input (mirroring preserves
span) but is kept for defensive totality — `Step::note` is a bare `int` with no
enforced `[0,127]` invariant.

## Testing Strategy

| Layer | What to test | Approach |
|---|---|---|
| Totality | All 11 transforms across sizes `{0,1,4,64}` × `{all-inactive, all-active}` | **Add the 3 entries to the local `transforms[]` at `MutationEngineTests.cpp:52-57`** (8 → 11). Exercises `palindrome`'s `n == 0` guard and `n == 64` (clamped to 64 → identity) |
| Unit — `invert` | Involution when no offset applies; first active step is a fixed point; inactive steps untouched; all notes in `[0,127]`; intervals preserved (no per-note clamp) after a correction fires (e.g. notes `{0, 127}`) | New `beginTest` blocks, mirroring the existing transpose tests' shape |
| Unit — `scaleIntervals` | Augment doubles distances from the axis; diminish halves them (truncating toward axis); ratio preserved; explicit `> 127` scaled-span no-op path; exactly one draw consumed | Seed sweep as in the transpose clamp test, which needs a wide range because `nextInt(2)` reads top bits |
| Unit — `palindrome` | `size 16 → 32`; second half mirrors the first (`result[2n-1-i] == result[i]`); `size 60 → 64` and `size 64 → 64` ceiling clamp; `size 1 → 4` floor clamp | Direct `expectEquals` on sizes, element compare for the mirror |
| Dispatch coverage (`:297-325`) | **Size-set assertion `{8, 16, 32}` at line 318 needs NO change** — verified: `invert`/`scaleIntervals` preserve 16, `palindrome(16) = clamp(32,4,64) = 32`, already in the set | Re-run as-is. **Re-verify `sawResize` (line 324) empirically**: the seed sweep is unchanged but `nextInt(11)` maps top bits differently than `nextInt(8)`, so the 103 sampled seeds must still hit at least one size-changing transform. Widen the sweep only if it fails. |

No test hardcodes the table size 8, so no other assertion is affected.

## Threat Matrix

N/A — no routing, shell, subprocess, VCS/PR automation, executable-file
classification, or process-integration boundary. Pure in-process value
transformation on the message thread.

## Migration / Rollout

No migration required. Purely additive; the 8 existing transform bodies are
untouched. The only behavioural side effect is the accepted 1/8 → 1/11 selection
probability shift. `ReproducibilityTests.cpp`'s mutation-chain golden asserts
self-consistency, not fixed values, so it stays green. Rollback = revert the
single commit.

## Open Questions

None. All three of the exploration's definitional questions were resolved in the
proposal and are pinned above.
