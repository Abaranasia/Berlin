# Design: Core Sequencing Model

## Technical Approach

Two Projucer projects, one shared source tree, one hard dependency boundary:

- `Source/core/` (`Step`, `Sequence`, `Scale`) is **JUCE-free** — standard library only.
- `Source/generation/` (`DeterministicRandom`, `PitchGenerator`, `RhythmGenerator`) depends on **`juce_core` only** (for `juce::Random`).

This boundary is what makes the settled "test project needs only `juce_core`" decision provably true rather than hopeful, and keeps the whole change off the audio thread (nothing in `juce-app-dev`'s real-time rules applies yet — no `getNextAudioBlock` path is touched).

All domain types live in namespace `berlin`. Generator objects are **immutable configuration**; the only mutable state is the caller's `DeterministicRandom`. That is what makes the seed-in/identical-out contract (`deterministic-generation` spec) easy to reason about and test.

## Architecture Decisions

### Decision: Domain sources include `<juce_core/juce_core.h>`, never `<JuceHeader.h>`

**Choice**: Shared sources include specific module headers directly.
**Alternatives considered**: `#include <JuceHeader.h>` (JUCE default in both `.jucer` templates).
**Rationale**: `JuceHeader.h` is *generated per project*. Berlin's version pulls in 12 modules; the test project's will pull in one. A shared file including it compiles against two different worlds and silently couples domain code to the GUI app's module list. Both projects already set `useAppConfig="0"`, so direct module includes are the supported path. The test project must also set `addUsingNamespaceToJuceHeader="0"` to match `Berlin.jucer` — JUCE's stock `UnitTestRunner/Source/Main.cpp` sets it to `"1"` and relies on it, so a verbatim copy will **not** compile; every `Logger`/`String`/`ArgumentList`/`UnitTestRunner` reference must be `juce::`-qualified during adaptation.

### Decision: Split `.h/.cpp` for types with out-of-line definitions; header-only for pure aggregates

| Type | Shape | Rationale |
|---|---|---|
| `Step` | `Step.h` only | Aggregate struct, nothing to define out of line |
| `DeterministicRandom` | `DeterministicRandom.h` only | Inline forwarders over `juce::Random` |
| `Sequence`, `Scale`, `PitchGenerator`, `RhythmGenerator` | `.h` + `.cpp` | See below |

**Rationale**: a `.cpp` with real definitions makes a *forgotten `<FILE>` registration* fail loudly as an unresolved-external link error in whichever project missed it. Header-only types get no such alarm, so they are restricted to types with nothing to lose. This turns the top project risk (Projucer does not glob folders) into a build failure instead of silence.

### Decision: `Scale` has a private constructor and public static factories

**Choice**: `Scale::major(root)` / `Scale::minor(root)` are the only ways to build a `Scale`; the interval-table constructor is private and defined in `Scale.cpp`.
**Alternatives considered**: public constructor taking an arbitrary interval list; free factory functions in a `Scales` namespace.
**Rationale**: enforces the settled "minor and major ONLY" scope *at the type level* — no caller can smuggle in an unsanctioned scale. Adding dorian/pentatonic later is purely additive (one new static function, no header shape change, no call-site churn). Interval tables stay in one translation unit.

### Decision: `PitchGenerator` precomputes candidates in the constructor; clamping is a constructor-time fallback

**Choice**: the constructor builds the ascending list of in-scale notes inside `[rangeLow, rangeHigh]`. If that list is empty, it also computes a single `fallbackNote` — the nearest in-scale note outside the range, expanding one semitone at a time, **ties resolved to the lower note**. `generateNextNote` then either indexes the candidate list with one RNG draw, or returns `fallbackNote` **consuming no RNG draw**.
**Alternatives considered**: draw a random note then snap it to the nearest scale member (rejected — non-uniform distribution, and snapping can leave the range); search for the nearest in-scale note on every call (rejected — repeated O(range) work per step and no obvious place to document the tie-break).
**Rationale**: satisfies "never fails, never returns out-of-scale" deterministically and in O(1) per call, and makes the degenerate case a *documented, testable constant* rather than an emergent behaviour. The tie-break rule must be asserted by a test, not left to implementation drift.

### Decision: Berlin test suites declare category `"Berlin"`; the TDD/CI gate runs `--category=Berlin`

**Choice**: every `juce::UnitTest` subclass in `Tests/Source/` passes `"Berlin"` as its category; the green gate is `BerlinTests.exe --category=Berlin`. A bare `runAllTests` stays available for diagnostics.
**Alternatives considered**: gate on `runAllTests` (no filter).
**Rationale**: `JUCE_UNIT_TESTS=1` is required by the settled harness decision, and it also compiles in `juce_core`'s **own** internal suites — 38 `juce_core` files reference that macro. An unfiltered gate would make a JUCE-internal failure look like a Berlin regression and slow every red/green cycle. Filtering keeps the TDD signal about Berlin's code.

### Decision: `Sequence` wraps `std::vector<Step>`

**Choice**: `std::vector<Step>` behind a narrow `size()/resize()/operator[]` surface.
**Alternatives considered**: `juce::Array<Step>` (would drag `juce_core` into `Source/core/`); fixed `std::array` (contradicts the settled resizable decision).
**Rationale**: keeps `Source/core/` JUCE-free. The container is private, so swapping it later is not a breaking change. When `Sequence` is eventually read on the audio thread, `size()`/`operator[]` are non-allocating; `resize()` stays on the message thread — a constraint for the future wiring change, not this one.

## Interfaces / Contracts

```cpp
// Source/core/Step.h — header-only, no JUCE
namespace berlin {
struct Step
{
    int  note   = 0;      // MIDI note number
    bool active = false;  // gate flag
};
bool operator== (const Step&, const Step&);   // non-member: keeps Step an aggregate
}                                             // with exactly two public members
```

```cpp
// Source/core/Sequence.h
namespace berlin {
class Sequence
{
public:
    Sequence() = default;
    explicit Sequence (int numSteps);          // steps default-constructed

    int  size() const noexcept;
    void resize (int numSteps);                // grow pads with default Step, shrink truncates

    Step&       operator[] (int index);        // precondition: 0 <= index < size()
    const Step& operator[] (int index) const;

private:
    std::vector<Step> steps;
};
bool operator== (const Sequence&, const Sequence&);
}
```

```cpp
// Source/core/Scale.h
namespace berlin {
class Scale
{
public:
    static Scale major (int rootNote);         // intervals {0,2,4,5,7,9,11}
    static Scale minor (int rootNote);         // natural minor {0,2,3,5,7,8,10}

    int  getRoot()       const noexcept;
    int  getNumDegrees() const noexcept;
    int  getDegree (int degreeIndex) const;    // semitone offset from root, 0-based
    bool contains (int note) const noexcept;   // octave-agnostic

private:
    Scale (int rootNote, std::vector<int> semitoneIntervals);
    int root { 0 };
    std::vector<int> intervals;                // ascending, within one octave
};
}
```

`contains` must use floored modulo — the classic bug here is `%` on notes below the root:

```cpp
const int pitchClass = ((note - root) % 12 + 12) % 12;   // NOT (note - root) % 12
```

```cpp
// Source/generation/DeterministicRandom.h — header-only
#include <juce_core/juce_core.h>
namespace berlin {
class DeterministicRandom
{
public:
    explicit DeterministicRandom (juce::int64 seedValue)
        : seed (seedValue), rng (seedValue) {}

    DeterministicRandom() = delete;             // contract made explicit & static_assert-able

    int         nextInt (int exclusiveUpperBound) { return rng.nextInt (exclusiveUpperBound); }
    float       nextFloat()                       { return rng.nextFloat(); }
    juce::int64 getSeed() const noexcept          { return seed; }

private:
    juce::int64  seed;
    juce::Random rng;
};
}
```

```cpp
// Source/generation/PitchGenerator.h
namespace berlin {
class PitchGenerator
{
public:
    PitchGenerator (Scale scale, int rangeLow, int rangeHigh);   // swaps if inverted
    int generateNextNote (DeterministicRandom& random) const;    // config is immutable
private:
    Scale scale;
    int rangeLow { 0 }, rangeHigh { 0 };
    std::vector<int> candidates;    // in-scale notes within range, ascending
    int fallbackNote { 0 };         // used only when candidates is empty
};
}
```

```cpp
// Source/generation/RhythmGenerator.h
namespace berlin {
class RhythmGenerator
{
public:
    RhythmGenerator (int numSteps, float density);   // density clamped to [0, 1]
    Sequence generate (DeterministicRandom& random) const;
private:
    int   numSteps { 0 };
    float density  { 0.0f };
};
}
```

`generate` draws **exactly one `nextFloat()` per step, in ascending index order** — that draw order *is* the reproducibility contract and must not be reordered or short-circuited. `note` is left at its default; only `active` is written.

**Edge case**: `juce::Random::nextFloat()` documents its range as 0 to 1.0 without guaranteeing the upper bound is excluded, so `nextFloat() < density` is not safe at `density == 1.0`. Guard the endpoints explicitly (`density >= 1.0f` → always active, `density <= 0.0f` → always inactive) rather than relying on strict-inequality luck. Keep the per-step draw in both cases so changing density never shifts the stream length.

## Data Flow

```
    seed ──→ DeterministicRandom ──┬──→ RhythmGenerator{numSteps, density} ──→ Sequence (active only)
                                   │                                                   │
    root ──→ Scale::minor(root) ───┴──→ PitchGenerator{scale, low, high} ──→ note ──────┘
                                                                    (test-side loop writes note
                                                                     into active steps only)
```

No production class composes these. The 16-step end-to-end demo is a plain loop inside a `juce::UnitTest`, per the settled non-goal on pipeline/orchestrator classes.

## File Changes

| File | Action | Description |
|---|---|---|
| `Source/core/Step.h` | Create | Aggregate `{note, active}` + non-member `operator==` |
| `Source/core/Sequence.h` / `.cpp` | Create | Resizable `Step` collection |
| `Source/core/Scale.h` / `.cpp` | Create | Root + intervals, private ctor, `major`/`minor` factories, `contains`/`getDegree` |
| `Source/generation/DeterministicRandom.h` | Create | Explicit-seed-only `juce::Random` wrapper |
| `Source/generation/PitchGenerator.h` / `.cpp` | Create | Precomputed candidates + clamp fallback |
| `Source/generation/RhythmGenerator.h` / `.cpp` | Create | Per-step independent probability |
| `Tests/BerlinTests.jucer` | Create | `consoleapp`, `JUCE_UNIT_TESTS=1`, `juce_core` only |
| `Tests/Source/Main.cpp` | Create | Adapted `ConsoleLogger` / `ConsoleUnitTestRunner` / CLI args / 0-1 exit code |
| `Tests/Source/{Step,Sequence,Scale,DeterministicRandom,PitchGenerator,RhythmGenerator,Reproducibility}Tests.cpp` | Create | One suite per area, all category `"Berlin"` |
| `Berlin.jucer` | Modify | `<GROUP name="core">` + `<GROUP name="generation">` `<FILE>` entries only |
| `Builds/`, `JuceLibraryCode/`, `Tests/Builds/`, `Tests/JuceLibraryCode/` | Regenerate | `Projucer.exe --resave` output |
| `Source/Main.cpp`, `Source/MainComponent.*` | Untouched | Must be absent from the final diff |

## Projucer Registration Mechanics

`<FILE file="...">` paths resolve **relative to the `.jucer` file's own directory**. `Berlin.jucer` sits at the repo root, `Tests/BerlinTests.jucer` one level down, so the two file lists are identical except for a `../` prefix:

```xml
<!-- Berlin.jucer, inside MAINGROUP -->
<GROUP id="{GUID-A}" name="core">
  <FILE id="coStpH" name="Step.h"       compile="0" resource="0" file="Source/core/Step.h"/>
  <FILE id="coSeqH" name="Sequence.h"   compile="0" resource="0" file="Source/core/Sequence.h"/>
  <FILE id="coSeqC" name="Sequence.cpp" compile="1" resource="0" file="Source/core/Sequence.cpp"/>
  ...
</GROUP>

<!-- Tests/BerlinTests.jucer, inside MAINGROUP — same block, "../" prefixed -->
<FILE id="tcSeqC" name="Sequence.cpp" compile="1" resource="0" file="../Source/core/Sequence.cpp"/>
```

`id` attributes must be unique **within** a project but need not match **across** projects. Headers are `compile="0"`, `.cpp` files `compile="1"`.

**Modules path — do not hardcode.** `MODULEPATH` resolves relative to the `.jucer` directory: `Berlin.jucer` stores `../../Projucer/modules`, which resolves to `H:\Proyectos\Juce\Projucer\modules` (exists). Reading it as build-folder-relative resolves to a nonexistent path, confirming the `.jucer`-relative interpretation. Since every `<MODULE>` carries `useGlobalPath="1"`, the stored path is only a fallback and Projucer's global preference is authoritative. `Tests/BerlinTests.jucer` must therefore use `useGlobalPath="1"` on `juce_core` and mirror the fallback with one extra level (`../../../Projucer/modules`) — never an absolute path, and never a choice between the two checkouts on this machine.

**Test-project settings that must mirror `Berlin.jucer`**: `useAppConfig="0"`, `addUsingNamespaceToJuceHeader="0"`, `VS2026` exporter targeting `Builds/VisualStudio2026` (relative to `Tests/`). Add `headerPath="../../Source"` to each test `CONFIGURATION` so test files can write `#include "core/Step.h"`. Domain files include each other file-relatively (`#include "../core/Scale.h"`), which needs no exporter configuration and behaves identically in both projects.

**Mandatory checklist after ANY file add/rename/delete** — this is a task step, not an aside:

1. Add/update `<FILE>` in `Berlin.jucer` (`Source/...`).
2. Add/update `<FILE>` in `Tests/BerlinTests.jucer` (`../Source/...`).
3. `Projucer.exe --resave Berlin.jucer`
4. `Projucer.exe --resave Tests/BerlinTests.jucer`
5. **Sync check**: the set of `Source/(core|generation)/*` entries in the two files must match exactly modulo the `../` prefix. A mismatch is a defect, not a warning.
6. Build both projects.

## Test-Runner Adaptation Plan

Start from `H:\Proyectos\Juce\JUCE\extras\UnitTestRunner\Source\Main.cpp` (~160 lines) and apply four deltas:

| Delta | Reason |
|---|---|
| Qualify every JUCE symbol with `juce::` | Berlin sets `addUsingNamespaceToJuceHeader="0"` |
| Replace `#include <JuceHeader.h>` with `#include <juce_core/juce_core.h>` | Match the domain-source include policy |
| Drop `DeletedAtShutdown::deleteAll()` from the exit `ScopeGuard` | `DeletedAtShutdown` lives in `juce_events`; nothing in Berlin's tests uses it, so dropping it keeps the module set at `juce_core` alone |
| Keep `--help/-h`, `--list-categories/-l`, `--category/-c`, `--name/-n`, `--seed/-s`, the failure summary, and the 0/1 exit code verbatim | Directly satisfies the `unit-test-harness` spec's discovery, filtering, exit-code, and seed requirements |

Suite discovery needs no registration: `juce::UnitTest`'s base constructor self-registers, so linking a `Tests/Source/*Tests.cpp` is enough — which is exactly why every test file needs its own `<FILE compile="1">` entry.

## Implementation Sequencing (harness-first — binding for sdd-tasks)

Strict TDD is enabled for this project, and no red/green cycle can run until a runner exists. The order below is a constraint, not a suggestion:

1. **Harness only.** `Tests/BerlinTests.jucer` + adapted `Tests/Source/Main.cpp` + one trivial smoke suite (category `"Berlin"`) that asserts a tautology. Resave, build, run, confirm exit code 0. **No domain code exists at this point.** Then make the smoke test fail deliberately and confirm exit code 1 — the harness's own red/green proof.
2. **First shared-source slice** — `Source/core/Step.h` + `Tests/Source/StepTests.cpp`, registered in **both** `.jucer` files. This is the earliest point that exercises the cross-project `../Source/...` relative-path mechanism, so it retires that assumption before five more files depend on it.
3. `Sequence` → 4. `Scale` → 5. `DeterministicRandom` → 6. `PitchGenerator` → 7. `RhythmGenerator`, each with a failing `juce::UnitTest` committed before the implementation, and each running the 6-step registration checklist above.
8. **End-to-end reproducibility suite** — 16 steps from root + `Scale::minor` + seed 12345, generated twice, asserted identical.
9. **Final gate** — `Projucer.exe --resave Berlin.jucer`, GUI app builds, and `git diff` confirms `Source/Main.cpp` / `Source/MainComponent.*` are unchanged.

## Testing Strategy

| Layer | What to Test | Approach |
|---|---|---|
| Compile-time | No default `DeterministicRandom` ctor; `Step` is an aggregate | `static_assert (! std::is_default_constructible_v<DeterministicRandom>)`; `static_assert (std::is_aggregate_v<Step>)`. C++ has no runtime member reflection, so "`Step` has no extra fields" is additionally a review check — call it out explicitly rather than pretending a runtime test covers it |
| Unit | `Step`, `Sequence` resize/index, `Scale` factories + octave-agnostic `contains` (including notes **below** the root), `DeterministicRandom` same-seed draws | One `juce::UnitTest` per type, category `"Berlin"` |
| Unit (behavioural) | `PitchGenerator` in-range/in-scale; empty-range clamp incl. the lower-note tie-break; `RhythmGenerator` size, `active`-only, density endpoints 0.0 and 1.0 | Fixed seeds; assert exact values where deterministic |
| Statistical | Density is average, not a quota | Run across many distinct seeds, assert the mean active count approximates `numSteps * density` within tolerance; assert individual runs are allowed to differ |
| Integration | Same seed → identical 16-step `Sequence` | Generate twice, compare with `Sequence::operator==` |
| Build | Both projects regenerate and compile | `Projucer.exe --resave` + build both; file-list sync check |

Not applicable this change: DSP block-size/sample-rate sweeps, feedback-stability, silent-input, and CPU-budget assertions from `juce-app-testing` — there is no DSP and no audio callback in scope.

## Threat Matrix

N/A — the delivered code has no routing, shell, subprocess, VCS/PR automation, executable-file classification, or process-integration boundary. `Source/core/` and `Source/generation/` are pure in-process value types and generators with no I/O. The `Projucer.exe --resave` invocations are developer/CI build steps performed by `sdd-apply` on trusted, repo-local paths, not a runtime boundary of the shipped artifact.

## Migration / Rollout

No migration required. Every change is additive: three new directories plus `<FILE>` entries in `Berlin.jucer`. No existing runtime code is modified, so rollback (revert + restore the `Berlin.jucer` file list + `--resave`) cannot regress app behaviour.

## Open Questions

- [ ] Does `Projucer.exe --resave` preserve `../`-prefixed `<FILE file>` paths verbatim, or rewrite/reject them? Retired by sequencing step 2 — if it rewrites them, the fallback is relocating the test project to `BerlinTests.jucer` at the repo root, which makes both file lists byte-identical.
- [ ] Which JUCE checkout does Projucer's global modules path actually resolve to on this machine? Must be observed at apply time (`--resave` output / generated `vcxproj` include paths), never assumed or hardcoded.
- [ ] Confirm `juce::Random::nextFloat()`'s upper bound in the installed JUCE version. The design guards `density == 1.0` explicitly so the answer cannot cause a silent behavioural bug either way.
