# Proposal: Core Sequencing Model

## Intent

Berlin is a bare JUCE scaffold: `MainComponent::getNextAudioBlock` only clears the buffer, there is no domain model, no generation logic, and no test harness. Nothing in the repo can produce or verify a musical sequence today. This change lands the first vertical slice of the Berlin School generative sequencer — the minimal domain model plus seeded pitch/rhythm generation — behind a runnable unit-test harness, so every later generator (velocity, gate, mutation, Euclidean) has a tested foundation and a reproducibility contract to build on. Per proposal sections 44/45, this is the explicitly recommended first implementation task.

## Scope

### In Scope

- `Source/core/Step.h` — value type, fields `note` and `active` ONLY.
- `Source/core/Sequence.h(+.cpp)` — resizable collection of `Step`.
- `Source/core/Scale.h(+.cpp)` — root + intervals, static factories, degree/containment queries.
- `Source/generation/DeterministicRandom.h` — wrapper over `juce::Random(int64)`, explicit-seed-only ctor, surface limited to `nextInt`/`nextFloat`/`getSeed`.
- `Source/generation/PitchGenerator.h/.cpp` — scale-based only: `{Scale, rangeLow, rangeHigh} → int generateNextNote(DeterministicRandom&)`.
- `Source/generation/RhythmGenerator.h/.cpp` — probability/density only: `{numSteps, density} → Sequence generate(DeterministicRandom&)`, populates `active` only.
- `Tests/BerlinTests.jucer` + `Tests/Source/` — sibling Projucer `consoleapp` test runner (`JUCE_UNIT_TESTS=1`), modeled on `JUCE/extras/UnitTestRunner`.
- `Berlin.jucer` — `<FILE>` registration for new sources (file list only; no other project settings change).

### Out of Scope (explicit non-goals)

- `MusicalTime` (deferred to a later change).
- Motif-based / interval-weighted pitch generation; Euclidean / polyrhythm rhythm generation.
- Velocity, gate, duration, probability, timingOffset, accent fields and their generators; mutation engine. All are additive, non-breaking future work.
- Any GUI, MIDI I/O, or audio-thread/playback wiring. `Source/Main.cpp` and `Source/MainComponent.*` stay untouched.
- Any pipeline/orchestrator class composing generators as production code (section 42's full chain).
- CMake. Build system stays Projucer — settled, not revisited.

## Capabilities

### New Capabilities

- `sequencing-core`: `Step`, `Sequence`, `Scale` value types, their invariants, and query behavior.
- `deterministic-generation`: seed-in/identical-out contract, `DeterministicRandom`, `PitchGenerator`, `RhythmGenerator` behavior.
- `unit-test-harness`: Projucer console test runner target, discovery of `juce::UnitTest` suites, exit-code contract.

### Modified Capabilities

None — `openspec/specs/` is empty; this is the project's first change.

## Resolved Open Questions

These were flagged during proposal drafting as domain ambiguities and resolved directly with the user; they are settled, not open, for spec/design:

- **Density semantics**: per-step independent probability, not a guaranteed count. `density=0.5` on 16 steps activates *on average* 8 steps, but the exact count is seed-dependent (7, 8, 9...). Rationale: density is meant to feel organic/generative, matching the user's framing of it as "how busy or sparse the sequence feels" rather than a hard quota.
- **Pitch range with no in-scale note**: `PitchGenerator::generateNextNote` clamps to the nearest in-scale note rather than returning the root or treating it as caller error. The generator must never fail or produce an out-of-scale note.
- **Scale factory set**: `minor` and `major` only for this change. Modal factories (dorian, phrygian, harmonic minor, pentatonic, etc.) are explicit future additive work.
- **Sequence length**: resizable after construction, not fixed at construction time (supersedes the fixed-size framing used earlier in exploration — corrected here).
- **Reproducibility durability**: guaranteed only within a given build (same JUCE version, same binary). Not guaranteed across JUCE version upgrades. `DeterministicRandom` may keep forwarding to `juce::Random` on this basis; pinning a custom algorithm for cross-version durability is out of scope unless this assumption changes later.

## Approach

1. **Harness first.** Strict TDD cannot run red/green until a test runner exists, so `Tests/BerlinTests.jucer` + `Tests/Source/Main.cpp` (near-verbatim adaptation of JUCE's `UnitTestRunner`: `ConsoleLogger`, `ConsoleUnitTestRunner`, `--category/--name/--seed` args, 0/1 exit code) lands and goes green before any domain code.
2. **Two projects, shared sources.** `Source/core/*` and `Source/generation/*` are registered by relative path in BOTH `.jucer` files — no code duplication. The test project's module dependencies stay minimal (`juce_core`) and reuse the existing `useGlobalPath="1"` convention rather than hardcoding a modules path.
3. **Domain model, TDD per file.** `Step` → `Sequence` → `Scale` → `DeterministicRandom` → `PitchGenerator` → `RhythmGenerator`, each with a failing `juce::UnitTest` first.
4. **Reproducibility as a first-class test.** A dedicated suite asserts same seed → byte-identical sequence (sections 13/32).
5. **End-to-end demo as a test, not a class.** A plain loop inside a `juce::UnitTest` composes root + scale + seed into a 16-step sequence and asserts reproducibility, mirroring section 44/45's `Seed: 12345 → C3 G3 Eb3 G3 ...` example.
6. **Mechanical regeneration.** Every `.jucer` file-list edit is followed by `Projucer.exe --resave <path>.jucer`; this is an explicit task step, not an implied one.

## Affected Areas

| Area | Impact | Description |
|------|--------|-------------|
| `Source/core/` | New | `Step.h`, `Sequence.h/.cpp`, `Scale.h/.cpp` |
| `Source/generation/` | New | `DeterministicRandom.h`, `PitchGenerator.h/.cpp`, `RhythmGenerator.h/.cpp` |
| `Tests/` | New | `BerlinTests.jucer`, `Source/Main.cpp`, one test suite per domain area |
| `Berlin.jucer` | Modified | New `<GROUP>`/`<FILE>` entries only |
| `Builds/`, `JuceLibraryCode/` | Regenerated | Output of `Projucer.exe --resave` |
| `Source/Main.cpp`, `Source/MainComponent.*` | Untouched | No playback wiring this change |

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| Projucer does not glob folders — new files silently unbuilt if `<FILE>` entry is forgotten | High | Make `<FILE>` registration + `Projucer.exe --resave` an explicit, verified step in every task that adds a file |
| Two `.jucer` file lists (app + tests) drift out of sync | Med | Single task step adds each new source to both files; verification builds both projects |
| `useGlobalPath="1"` modules path is ambiguous (two JUCE checkouts on this machine) | Med | Do NOT hardcode either path; the test project reuses the same `useGlobalPath="1"` convention as `Berlin.jucer` |
| Strict TDD blocked until the harness exists | High | Harness is sequenced as task #1; no domain code lands before it is green |
| Scope creep toward section 42's full pipeline (octave/velocity/probability stages) | Med | Non-goals listed explicitly above; a reviewer rejects any generator beyond pitch/rhythm |
| Pared-down `Step{note, active}` needs later field additions | Low | Additions are additive and non-breaking by design; accepted deliberately so every field is test-covered now |

## Rollback Plan

Every change is additive. Revert by: (1) `git revert` the change commits — `Source/core/`, `Source/generation/`, and `Tests/` are new directories that disappear wholesale; (2) restore `Berlin.jucer` to its previous file list; (3) run `Projucer.exe --resave Berlin.jucer` to regenerate `Builds/VisualStudio2026/` and `JuceLibraryCode/`. No existing runtime code is modified, so rollback cannot regress app behavior.

## Dependencies

- Projucer binary available headlessly (`Projucer.exe --resave`).
- A JUCE modules checkout resolvable through Projucer's global path setting.
- `juce::Random(int64)` documented determinism — no third-party PRNG introduced.

## Success Criteria

- [ ] `Tests/BerlinTests.jucer` regenerates via `Projucer.exe --resave`, builds, and runs green with exit code 0.
- [ ] Unit test coverage exists for `Step`, `Sequence`, `Scale`, `DeterministicRandom`, `PitchGenerator`, and `RhythmGenerator`.
- [ ] An explicit seed-reproducibility test asserts same seed → identical sequence (sections 13/32).
- [ ] `PitchGenerator` clamps to the nearest in-scale note when the configured range contains no in-scale note (never fails, never returns out-of-scale).
- [ ] `RhythmGenerator` density is tested as per-step independent probability (average active-step count matches `density`, exact count varies by seed) — not as a guaranteed count.
- [ ] An end-to-end test generates a 16-step sequence from root note + scale + fixed seed and proves reproducibility, mirroring section 44/45's example flow.
- [ ] `Berlin.jucer` still regenerates and the GUI app still builds after the file-list additions.
- [ ] `Source/Main.cpp` and `Source/MainComponent.*` are unchanged in the final diff.
