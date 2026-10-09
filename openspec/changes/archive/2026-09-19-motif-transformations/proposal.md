# Proposal: Motif Transformations (invert, augment/diminish, palindrome)

## Intent

Roadmap Phase 10, Slice 3/3 — the last slice before Phase 11 (VST3/AU). `MutationEngine` ships 8 transforms, none of which reshape a motif's pitch *contour* or its temporal *symmetry*: every existing transform shifts, reorders, resizes, or edits individual steps. Invert, interval augment/diminish, and palindrome add genuinely motivic variation to Mutate and Auto-Evolve without touching the invocation model.

## Scope

### In Scope
- 3 new transforms in `MutationEngine.h/.cpp`, same `Sequence (*)(const Sequence&, DeterministicRandom&)` signature and the family-wide totality guarantee.
- `transforms[]` grows 8 → 11 entries; `applyRandomTransform`'s single-selection-draw dispatch is otherwise unchanged.
- Tests: extend the shared totality sweep, add per-transform behavioral tests, revisit the dispatch-coverage assertions.

### Out of Scope
- **No duration field** on `Step`/`Sequence` (decided: augment/diminish = pitch-interval scaling; real durations remain a separate future change).
- **No new UI** — invocation stays the existing random draw behind the Mutate button / Auto-Evolve timer. No per-transform user selection.
- No changes to `SequencePlayer`, MIDI export, or preset persistence.

## Capabilities

### New Capabilities
- None

### Modified Capabilities
- `sequence-mutation`: the transform set grows to 11; `Pure Mutation Transforms` and `Transform Bounds` gain the three new transforms' semantics and range/length rules.

## Approach

Table-only addition, dispatched through the existing single random draw.

| Transform | Draws | Semantics | Bounds rule |
|---|---|---|---|
| `invert` | 0 | Mirror active notes about an axis: `n' = 2a - n`. Axis `a` = note of the **first active step** (classic melodic inversion; the anchor note is fixed, so it is an involution when no correction applies). Inactive steps untouched. Size unchanged. | Mirroring preserves span, so range is restored by **one uniform corrective offset** applied to all active notes (shift up by `-min'` if `min' < 0`, down by `127 - max'` if `max' > 127`). Never per-note. |
| `scaleIntervals` (augment/diminish) | 1 (`nextInt(2)`, like `transpose`) | Scale each active note's interval from the same axis `a`: augment `n' = a + 2(n - a)`, diminish `n' = a + (n - a)/2` (C++ truncation, i.e. toward the axis). Size unchanged. | Same uniform-offset correction as `invert`. Augment doubles the span; if the scaled span exceeds 127 no uniform shift fits, so the transform is a **no-op copy** rather than clamping per-note. |
| `palindrome` | 0 | Forward then backward mirror, **pivot NOT shared**: `result[i] = input[i < n ? i : 2n-1-i]`, wrap-safe via `((j % n) + n) % n`. | `targetSize = clamp(2n, 4, 64)`, exactly `stretch`/`compress`'s precedent. `n == 0` → early-return copy. |

### Resolved open questions

1. **`invert` vs. transpose's "clamp once, never per-note" principle.** Honoured, not broken: inversion preserves the interval span, so a single uniform corrective offset always restores `[0, 127]` for any valid input. No note is ever clamped independently. `scaleIntervals` uses the same rule, falling back to a no-op in the one case (augmented span > 127) where no uniform shift exists.
2. **`palindrome` pivot convention.** Non-shared pivot (size `2n`) is chosen over the shared pivot (`2n-1`) deliberately: for the dispatch-coverage test's 16-step input, `2n = 32`, which is **already inside** the existing asserted size set `{8, 16, 32}`, whereas `2n-1 = 31` would force that closed set open for no musical gain. Net effect: the dispatch test's size set needs no change; only the "more than one transform selected" reasoning is re-verified.
3. **Draw-probability shift.** Growing the table 8 → 11 changes every already-shipped transform's selection probability from 1/8 to 1/11. This is an **accepted consequence**, not a blocker: selection is documented as "chosen at random from the full transform set", the set is explicitly the thing being extended, and the reproducibility golden asserts self-consistency, not fixed output values.

## Affected Areas

| Area | Impact | Description |
|------|--------|-------------|
| `Source/generation/MutationEngine.h` | Modified | 3 declarations + doc comments |
| `Source/generation/MutationEngine.cpp` | Modified | 3 implementations; `transforms[]` 8 → 11 |
| `Tests/Source/MutationEngineTests.cpp` | Modified | Totality sweep array, 3 behavioral tests, dispatch-coverage re-verification |
| `Source/core/Step.h`, `Sequence.h` | Unchanged | Explicitly no duration field |
| `Source/MainComponent.*` | Unchanged | No new UI surface |

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| Shared axis convention (first active step) feels arbitrary vs. min/max midpoint | Med | One axis rule across both pitch transforms; documented in the header, easy to change later in one place |
| `scaleIntervals` no-ops more often than expected on wide-range sequences | Med | Test the >127-span no-op path explicitly; it is a well-formed copy, identical in kind to the existing 6 no-op guards |
| Existing transforms feel "rarer" after 1/8 → 1/11 | Low | Named as accepted above; no behavioral contract binds the per-transform rate |
| New transforms are structural, not stochastic — user cannot invoke them on demand | Low | Matches this slice's scope and the project's precedent of deferring UI; candidate for a future explicit-invocation slice |

## Rollback Plan

Purely additive — the 8 existing transforms' bodies are untouched. Revert the single commit: `transforms[]` returns to 8 entries and the draw distribution to 1/8. No persisted state, file format, or UI depends on the new entries, so no migration is needed.

## Dependencies

- None. Slices 1 and 2 of Phase 10 (advanced euclidean/random generators) are already on `feat/advanced-generators`.

## Success Criteria

- [ ] `invert`, `scaleIntervals`, `palindrome` implemented with the semantics above and reachable via `applyRandomTransform`.
- [ ] All 11 transforms pass the shared totality sweep (sizes {0,1,4,64} × {all-inactive, all-active}) with no crash.
- [ ] `invert` is an involution when no corrective offset applies; `scaleIntervals` preserves interval *ratios* and no-ops when span > 127; `palindrome` yields `clamp(2n,4,64)` steps with a mirrored second half.
- [ ] No note outside `[0, 127]` is produced by any new transform; no per-note clamping anywhere.
- [ ] `Step`/`Sequence` and `MainComponent` are unchanged; full test suite green.
