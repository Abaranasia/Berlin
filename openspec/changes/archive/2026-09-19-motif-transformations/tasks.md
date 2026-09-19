# Tasks: Motif Transformations (invert, scaleIntervals, palindrome)

## Review Workload Forecast

| Field | Value |
|-------|-------|
| Estimated changed lines | ~250-300 (header ~20, `.cpp` ~105, tests ~120-150, table edits ~5) |
| 400-line budget risk | Low |
| Chained PRs recommended | No |
| Suggested split | Single PR |
| Delivery strategy | ask-on-risk (default; not overridden) |
| Chain strategy | pending |

Decision needed before apply: No
Chained PRs recommended: No
Chain strategy: pending
400-line budget risk: Low

### Suggested Work Units

| Unit | Goal | Likely PR | Focused test command | Runtime harness | Rollback boundary |
|------|------|-----------|----------------------|-----------------|-------------------|
| 1 | 3 transforms + table growth + dispatch re-verification | PR 1 (single PR) | `BerlinTests.exe --category=Berlin --filter="*MutationEngine*"` | N/A — message-thread-only value transform, no audio device (design's JUCE thread confirmation) | Revert the single commit; 8 existing transforms and dispatch shape untouched |

## Phase 1: `invert` (helpers ship with its GREEN)

- [x] 1.1 RED: add `invert` tests to `Tests/Source/MutationEngineTests.cpp` (involution, first-active-step fixed point, inactive untouched, `[0,127]` range, interval preserved after a `{0,127}` correction). Fails to compile — no `invert` yet.
- [x] 1.2 GREEN: add `firstActiveNote` and `commitWithUniformOffset` helpers (anonymous namespace, `.cpp`-only) per design's Interfaces/Contracts.
- [x] 1.3 GREEN: declare `invert` in `MutationEngine.h` after `compress` with its doc comment; implement in `.cpp` using the helpers, per design's exact contract.
- [x] 1.4 Verify: green (see deviation note — runner has no `--filter`; ran full `--category=Berlin`).

## Phase 2: `scaleIntervals`

- [x] 2.1 RED: add `scaleIntervals` tests (augment doubles axis-distance, diminish halves truncating toward axis, ratio preserved, `>127` no-op fallback, exactly one draw via wide seed sweep). Fails to compile.
- [x] 2.2 GREEN: declare `scaleIntervals` in `MutationEngine.h` after `invert` with its doc comment.
- [x] 2.3 GREEN: implement in `.cpp` per design's contract — unconditional `nextInt(2)` first (mirrors `transpose`), then axis scale + `commitWithUniformOffset`, else unmodified copy.
- [x] 2.4 Verify: green (full-suite run; see deviation note).

## Phase 3: `palindrome`

- [x] 3.1 RED: add `palindrome` tests (16->32, mirror equality, 60->64 and 64->64 ceiling, 1->4 floor). Fails to compile.
- [x] 3.2 GREEN: declare `palindrome` in `MutationEngine.h` after `scaleIntervals` with its doc comment.
- [x] 3.3 GREEN: implement in `.cpp` per design's contract (`n==0` early return, `clamp(2n,4,64)`, wrap-safe mirror index). No RNG draw.
- [x] 3.4 Verify: green (full-suite run; see deviation note).

## Phase 4: Table growth (8 -> 11)

- [x] 4.1 `MutationEngine.cpp`: append the 3 new transforms to `transforms[]` in `applyRandomTransform`, after `compress`.
- [x] 4.2 `MutationEngineTests.cpp:52-57`: append the same 3 entries to the local totality-sweep array.
- [x] 4.3 Verify: green across all 11 transforms x sizes `{0,1,4,64}` x active states.

## Phase 5: Dispatch-coverage re-verification (design-flagged risk)

- [x] 5.1 Confirmed size-set assertion `{8,16,32}` (unchanged) needs no edit — invert/scaleIntervals preserve 16, `palindrome(16)=32`.
- [x] 5.2 Ran the dispatch test under `nextInt(11)` (table now has 11 entries): `sawResize` passes unmodified with the existing seed sweep (0..100000 step 977). No widening needed.
- [x] 5.3 Verify: green.

## Phase 6: Full-suite confirmation

- [x] 6.1 Ran `--name=MutationEngine` (runner's actual name-filter flag) — 27/27 MutationEngine tests green.
- [x] 6.2 Ran full suite (`--category=Berlin`, 223 tests, up from 211 baseline) — `ReproducibilityTests.cpp`'s mutation-chain golden stays green (self-consistency, not fixed values).
- [x] 6.3 Confirmed via `git diff --stat`: only `MutationEngine.h`, `MutationEngine.cpp`, `MutationEngineTests.cpp` changed. Zero diff on the 8 existing transforms and on `Step`, `Sequence`, `SequencePlayer`, `MainComponent`.

## Deviation Note

Tasks referenced `--filter="*Name*"` for the test runner; the actual `BerlinTests.exe` (see `Tests/Source/Main.cpp`) only supports `--category`/`-c` and `--name`/`-n` (no glob `--filter`). Used `--category=Berlin` (full suite) for per-phase verification and `--name=MutationEngine` for the phase 6.1 focused run. No production-code impact.

## Rules Applied

- Strict TDD: every transform is RED (compile-failing test) before GREEN, per Slice 1 precedent.
- Helpers are `.cpp`-private; no direct unit test, covered indirectly via `invert`/`scaleIntervals`.
- No new files, no `.jucer` registration changes.
