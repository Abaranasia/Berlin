# Design: Tempo Control + Tempo-Synced Delay UI

Change `tempo-delay-ui` (Slice 2/5). Implements `proposal.md`. Reuses the Slice-1
(`scale-aware-generation`) preset-versioning precedent and `SynthVoice`'s existing
message-thread-setter / audio-thread-`applyParameters()` pattern verbatim.

## Technical Approach

BPM becomes live state owned by `BerlinAudioProcessor` (message thread), published to
the audio thread through one `std::atomic<double>` on `SequencePlayer`, and applied to
`Transport` exactly once per audio block via a **phase-preserving origin rebase** that
makes the boundary grid relative instead of absolute. A new JUCE-free `TempoSync` unit
maps `(bpm, division) -> seconds`, feeding `SynthPatch.delayTimeSeconds`. `SynthEffects`
gains `SynthVoice`-style atomic setters so delay/reverb become live, and the editor gains
its first TEMPO and DELAY/REVERB widget sets. Preset schema goes to v3.

**Ownership (single source of truth, for spec alignment):** `BerlinAudioProcessor::currentBpm`
(message thread) is authoritative for BPM. The editor is a view. `Transport` is an
audio-thread-only consumer and never a source. MIDI export reads `currentBpm`.

## Architecture Decisions

### Decision 1: the cross-thread atomic lives on `SequencePlayer`, not `Transport`

| Option | Tradeoff |
|---|---|
| `std::atomic<double>` member on `Transport` | **Rejected** — deletes `Transport`'s copy/move ctors, and `SequencePlayer (Sequence, Transport transportToUse)` (`SequencePlayer.cpp:15`) takes it **by value**. Would force a hand-written copy ctor on a class whose whole value is being trivially-testable arithmetic. |
| Atomic on `SequencePlayer`; `Transport::setBpm(double)` is plain, audio-thread-only | **Chosen** |

**Rationale**: `SequencePlayer` already owns every cross-thread atomic in this tier
(`playhead`, `loopCount`, `sequencePending`). `Transport` stays JUCE-free, allocation-free,
copyable, and unit-testable with zero threading in the test. Thread discipline lives in one
class; timing math lives in the other.

### Decision 2: relative (origin-rebased) boundary grid — **the core risk mitigation**

Today `getBoundary`/`countBoundaries` compute `llround(k * samplesPerStep)` off an
**absolute** origin (`Transport.cpp:83,98`), so changing `samplesPerStep` retimes all
history. Replace the formula with an explicit origin anchor:

```cpp
// Transport private state (new): double originSample {0.0}; long long originStep {0};
//                                double sampleRate {0.0};   // cached by prepare()
long long Transport::boundarySampleFor (long long k) const noexcept   // single chokepoint
{
    return std::llround (originSample + static_cast<double> (k - originStep) * samplesPerStep);
}
```

`countBoundaries`, `getBoundary` and (transitively) `advance` all call this one helper.
Defaults `originSample = 0.0, originStep = 0` make it **bit-identical to today's formula**
when BPM never changes — every existing `TransportTests` drift assertion stays green
unmodified. `reset()` and `prepare()` set the origin back to `(0.0, 0)`.

### Decision 3: phase-preserving rebase at the moment of change

```cpp
void Transport::setBpm (double newBpm) noexcept   // AUDIO THREAD, block start only
{
    if (newBpm <= 0.0 || newBpm == bpm) return;          // cheapest path: called every block

    const double newSamplesPerStep = (sampleRate > 0.0 && stepsPerBeat > 0)
        ? sampleRate * 60.0 / (newBpm * static_cast<double> (stepsPerBeat)) : 0.0;

    if (samplesPerStep <= 0.0)                            // never prepared / degenerate
    {
        bpm = newBpm; samplesPerStep = newSamplesPerStep;
        originSample = 0.0; originStep = 0; return;
    }

    // Fraction of the CURRENT step already elapsed, under the OLD tempo.
    const double prevBoundary = originSample
        + static_cast<double> (nextStepCounter - 1 - originStep) * samplesPerStep;
    double phase = (static_cast<double> (position) - prevBoundary) / samplesPerStep;
    phase = std::clamp (phase, 0.0, 1.0);

    originStep   = nextStepCounter - 1;                   // anchor = last EMITTED boundary
    originSample = static_cast<double> (position) - phase * newSamplesPerStep;
    samplesPerStep = newSamplesPerStep;
    bpm = newBpm;
}
```

**Invariant this buys**: the next boundary lands at
`position + (1 - phase) * newSamplesPerStep`, i.e. **strictly >= `position`, never in the
past and never skipped**. The remaining fraction of the in-flight step is re-scaled by the
new tempo — exactly what a DAW does. Already-emitted boundaries are never re-queried, so
moving their notional origin is unobservable.

- `phase == 1.0` (clamped) is the only degenerate case: the next boundary lands exactly at
  `position`, i.e. fires at sample offset 0 of this block. Still no skip, no double.
- `nextStepCounter == 0` (nothing emitted yet) falls out of the same formula with
  `originStep = -1`; no special case needed.
- `originSample` is a **`double`**, not `long long`, so repeated rebases (a slider drag =
  hundreds of changes) introduce **zero** accumulated rounding — rounding happens once, at
  the `llround` in `boundarySampleFor`. Drift stays ±1 sample against the post-change ideal
  grid, preserving `Transport.h`'s documented contract.

**Alternatives rejected**: (a) anchor at `position` with `originStep = nextStepCounter` —
fires a step immediately, truncating the current one; (b) anchor at the last boundary
without phase scaling — the next boundary can land *behind* `position` when tempo rises,
yielding a negative `sampleOffset`; (c) defer the change to the next boundary — musically
laggy at 40 BPM (up to 1.5 s) and needs extra pending state.

### Decision 4: apply point = top of `SequencePlayer::process`, after adopt, before query

```cpp
void SequencePlayer::process (int numSamples, StepEventBuffer& out) noexcept
{
    out.clear();
    if (sequencePending.load (acquire)) { ...existing adopt, which calls transport.reset()... }

    transport.setBpm (pendingBpm.load (std::memory_order_relaxed));   // <-- NEW, exactly here
    if (sequence.size() > 0) { const int n = transport.countBoundaries (numSamples); ... }
    transport.advance (numSamples);
}
```

**Rationale**: the block protocol is "pure queries, then one commit". A tempo change *between*
`countBoundaries()` and `getBoundary(i)`/`advance()` would desynchronise the loop, so the
mutation is confined to a single point before the first query. Placing it **after** the adopt
block means `transport.reset()`'s origin zeroing is never undone. `advance()` re-derives its
count with the same `samplesPerStep` used by the query, so per-block composition is exact.
`setBpm` early-returns on an unchanged value, so the steady-state cost is one relaxed atomic
load plus a `double` compare — no lock, no allocation, no branch misprediction of note.
Public entry: `SequencePlayer::setBpm(double)` (message thread) → `pendingBpm.store(relaxed)`.
Documented in `Transport.h`: `setBpm` is legal **only** at block start.

### Decision 5: `kMaxDelaySeconds` 2.0 -> 3.0, moved to `SynthPatch.h`

At the product-resolved floor of 40 BPM, the longest exposed division (1/2) needs exactly
`60/40 * 2 = 3.0 s`. Raising the bound makes **every exposed division reachable at every
legal BPM**, so the sync path never silently lies to the user. Cost is memory only
(`3.0 * 48000 * 4 B * 2 ch ≈ 1.1 MB`), allocated in `SynthEffects::prepare` where
allocation is already allowed. The clamp stays as a **defensive** bound for Free mode and
untrusted preset XML, not as a normal-path behaviour. Constant moves from
`SynthEffects.cpp:17`'s anonymous namespace to `SynthPatch.h` next to the other `k*` bounds,
matching the precedent already recorded there ("`kMinCutoffHz`... now live in `SynthPatch.h`").
This **supersedes** the proposal's "silent clamp at 2.0 s" assumption; a RED test pins
`max(delaySecondsFor(kMinBpm, d)) <= kMaxDelaySeconds` over all divisions so adding a longer
division later fails loudly.

### Decision 6: `SynthEffects` mirrors `SynthVoice`'s atomic-target pattern

Message-thread setters clamp and `store(relaxed)` into a `target` struct; `process()` calls a
new private `applyParameters()` **before** the channel loops. Delay time glides via a
one-pole per block (`current += (target - current) * coeff`, ~100 ms time constant) and is
pushed with one `delayLine.setDelay()` — `juce::dsp::DelayLine<float>` defaults to `Linear`
interpolation, so a fractional glide yields a tape-style pitch slide instead of a click.
Feedback and mix use a per-sample linear ramp computed once per block (`step = (end-start)/n`,
recomputed identically per channel) inside the existing inner loop. Reverb parameters are
compared against a cached copy and `setParameters()` is called only on change — same
change-gating rationale as `SynthVoice`'s ADSR gate (`SynthVoice.cpp:203-212`).
**Rejected**: per-sample `setDelay()` (bounds-checks per sample, no audible gain);
`juce::SmoothedValue` per sample (the channel loop is outer, so a per-sample smoother would
advance twice per sample frame).

### Decision 7: preset schema v3, name-keyed division, strict-when-present

`kSchemaVersion` 2 -> 3. New `<Transport bpm="..."/>` child; `delayTimeSeconds`,
`delayFeedback`, `delayMix`, `reverbRoomSize`, `reverbDamping`, `reverbWetLevel`,
`reverbDryLevel`, `delaySynced`, `delayDivision` join the existing `<Synth>` node.
`delayDivision` persists as a **name string** via a `divisionNames()` table, mirroring
`scaleNames()`/`waveformNames()` (`PresetManager.cpp:20-53`) — never a raw ordinal.
Migration mirrors v2 exactly: `version >= 3` requires every new attribute (missing ->
`parseFailed`); `version < 3` defaults from `kDefaultPatch` + `kDefaultBpm` + sync-off +
quarter. Every numeric field passes through `clampParameter` against its published bound
before storage — the R4-001 untrusted-XML precedent already recorded in
`PresetManager.cpp:210-219`. `plugin-state-recall` inherits this for free via
`getStateInformation`/`setStateInformation`, which already reuse `toValueTree`/`fromValueTree`.

### Decision 8: FX section greys out while `fxToggle` is off

Delay/reverb are audible only when `SynthEngine::effectsEnabled` is true (default **off**).
Silently auto-enabling FX on a control move is surprising; leaving the section live but
inaudible is worse. The FX section calls `setEnabled(fxToggle.getToggleState())` on toggle.
This is a two-line affordance, not the conditional-visibility framework reserved for Slice 5.

## Data Flow

    [GUI thread]                                   [audio thread]
    bpmSlider ──┐
    divisionBox ├─> Editor::pushTempoFromWidgets()
    syncToggle ─┘            │
                             ├─> processor.setBpm(v) ─> currentBpm ─> player.setBpm(v)
                             │                                            │
                             │                              pendingBpm (atomic<double>)
                             │                                            │
                             │                          SequencePlayer::process (block start)
                             │                                            │
                             │                              Transport::setBpm -> origin rebase
                             │                                            │
                             │                         countBoundaries/getBoundary/advance
                             │
                             └─> TempoSync::delaySecondsFor(bpm, division)
                                          │
                                 currentPatch.delayTimeSeconds
                                          │
                        processor.pushPatchToSynth() -> SynthEngine::setDelayTimeSeconds
                                          │
                            SynthEffects target atomics ──> applyParameters() @ block start

    currentBpm ──> exportMidiTo() ──> MidiFileWriter::writeToFile(timeline, currentBpm, dest)
    currentBpm + patch + division ──> PresetManager::toValueTree (schema v3)

## File Changes

| File | Action | Description |
|---|---|---|
| `Source/core/TempoSync.h` / `.cpp` | Create | JUCE-free `enum class SyncDivision`, `kNumSyncDivisions`, `factorFor`, `delaySecondsFor(bpm, division)`. Reused verbatim by Slice 3. |
| `Source/playback/Transport.h` / `.cpp` | Modify | `sampleRate`/`originSample`/`originStep` members; `boundarySampleFor` chokepoint; `setBpm`; `getBpm`. Header doc replaces the "No runtime bpm mutation API" paragraph. |
| `Source/playback/SequencePlayer.h` / `.cpp` | Modify | `std::atomic<double> pendingBpm`; `setBpm(double)`; one `transport.setBpm(...)` call in `process`. |
| `Source/synth/SynthPatch.h` | Modify | `kMinBpm/kMaxBpm/kDefaultBpm`, `kMin/kMaxDelaySeconds` (3.0f), feedback/mix/reverb bounds; `delaySynced` + `delayDivision` fields. |
| `Source/synth/SynthEffects.h` / `.cpp` | Modify | Atomic `target` struct, 7 setters, `applyParameters()`, delay glide + gain ramps. |
| `Source/synth/SynthEngine.h` / `.cpp` | Modify | 7 one-line forwarders, mirroring lines 129-139. |
| `Source/plugin/BerlinAudioProcessor.h` / `.cpp` | Modify | `kBpm` -> `kDefaultBpm`; `currentBpm` member; `setBpm`/`getBpm`; `exportMidiTo` passes `currentBpm` (was `.cpp:259`); `pushPatchToSynth` gains the 7 effect pushes; `loadPreset`/`setStateInformation` restore BPM. |
| `Source/plugin/BerlinAudioProcessorEditor.h` / `.cpp` | Modify | TEMPO row + DELAY/REVERB sections; `pushTempoFromWidgets`, `recomputeSyncedDelayTime`, extended `applyPatchToWidgets`/`refreshFromProcessor`; `setSize` height grows by the new rows. |
| `Source/preset/Preset.h` | Modify | `double bpm`. (`delaySynced`/`delayDivision` ride inside `patch`.) |
| `Source/preset/PresetManager.h` / `.cpp` | Modify | `kSchemaVersion = 3`; `divisionNames()`; `<Transport>` node; v3 read/write + clamps. |
| `Berlin.jucer`, `BerlinTests.jucer` | Modify | Register `TempoSync.h/.cpp` in both (the deliberate unresolved-external tripwire). |
| `Tests/Source/TempoSyncTests.cpp` | Create | Division math, clamp bound, round-trip. |
| `Tests/Source/TransportTempoChangeTests.cpp` | Create | Origin-rebase behaviour (see Testing Strategy). |
| `Tests/Source/SynthEffectsTests.cpp` | Create | First-ever coverage for `SynthEffects` (currently "no covering tests found"). |
| `Tests/Source/PresetManagerTests.cpp`, `BerlinAudioProcessorTests.cpp`, `MidiExportTimelineTests.cpp` | Modify | v3 round-trip + v1/v2 defaulting; live-BPM export. |

## Interfaces / Contracts

```cpp
// Source/core/TempoSync.h  - JUCE-free. Declaration order is persisted via
// divisionNames(); NEVER reorder without a schema bump.
namespace berlin {
enum class SyncDivision { half, quarter, dottedEighth, eighth, eighthTriplet, sixteenth };
inline constexpr int kNumSyncDivisions = 6;

constexpr double factorFor (SyncDivision d) noexcept;   // beats per repeat: 2, 1, 0.75, 0.5, 1/3, 0.25
double delaySecondsFor (double bpm, SyncDivision d) noexcept;   // (60/bpm) * factorFor(d); bpm<=0 -> 0
}

// Source/playback/Transport.h
void   setBpm (double newBpm) noexcept;   // AUDIO THREAD, block start ONLY (before countBoundaries)
double getBpm() const noexcept;

// Source/playback/SequencePlayer.h
void setBpm (double newBpm) noexcept;     // MESSAGE THREAD; relaxed store, adopted next block

// Source/synth/SynthEffects.h + identical forwarders on SynthEngine  (MESSAGE THREAD, clamped)
void setDelayTimeSeconds (float) noexcept;  void setDelayFeedback (float) noexcept;
void setDelayMix (float) noexcept;          void setReverbRoomSize (float) noexcept;
void setReverbDamping (float) noexcept;     void setReverbWetLevel (float) noexcept;
void setReverbDryLevel (float) noexcept;

// Source/plugin/BerlinAudioProcessor.h  (MESSAGE THREAD)
void   setBpm (double bpm);   // clamps to [kMinBpm, kMaxBpm], updates currentBpm + player
double getBpm() const;
```

## Testing Strategy

Strict TDD — every row below is a RED test written before its implementation.
Runner: `BerlinTests.exe --category=Berlin`.

| Layer | What to test | Approach |
|---|---|---|
| Unit — `TempoSync` | `delaySecondsFor` for all 6 divisions at 40/120/240 BPM; `bpm <= 0` -> 0; `max over divisions at kMinBpm <= kMaxDelaySeconds` | Pure arithmetic, no JUCE |
| Unit — `Transport` (**critical**) | 1. Existing `driveAndCheckBoundaries` drift suites stay green unmodified (proves the origin refactor is behaviour-preserving). 2. After `setBpm` mid-run, the next boundary sample is `> position` and `stepCounter == nextStepCounter` — **no skip, no double, no negative `sampleOffset`**. 3. `getSamplesPerStep() == sampleRate*60/(bpm*stepsPerBeat)` after every change. 4. Post-change boundaries stay within ±1 of the ideal grid anchored at the rebase point. 5. 500 successive `setBpm` calls (slider-drag simulation) accumulate no drift. 6. `setBpm` before `prepare`, with `bpm <= 0`, while stopped, and at `nextStepCounter == 0` are all safe. | Deterministic, single-threaded — `Transport` is JUCE-free and needs no thread to test |
| Unit — `SequencePlayer` | BPM change mid-loop produces monotonically increasing `sampleOffset`s within the block and preserves playhead/loop-count semantics; change applied after an adopt does not resurrect a stale origin | Drive `process()` with a `StepEventBuffer` |
| Unit — `SynthEffects` | Setters clamp (incl. NaN via `clampParameter`); delay time reaches target over N blocks; tail continuity — no sample-to-sample discontinuity above a threshold when delay time jumps; `process()` is allocation-free | New test file; impulse + step-response |
| Unit — `PresetManager` | v3 round-trips BPM/division/sync/effects; v1 and v2 files load with documented defaults; out-of-range BPM/effects in XML are clamped; unknown division name -> `parseFailed` | `toValueTree`/`fromValueTree` are pure statics |
| Integration — processor | `setBpm` reaches the player; `exportMidiTo` writes a tempo meta event matching live BPM (not 120); `setStateInformation` round-trips BPM | `BerlinAudioProcessorTests` + `MidiFileWriter::writeTo` into a `MemoryOutputStream` |
| Manual (review gate) | Audible delay/reverb from UI with no preset editing; no click on tempo or delay-time change; audio-path diff contains no allocation/lock/logging | `juce-app-dev` output contract |

## Threat Matrix

N/A — no routing, shell, subprocess, VCS/PR automation, executable-file classification, or
process-integration boundary is added. The one untrusted-input surface (preset XML) is
pre-existing; Decision 7 extends its established clamp-on-parse rule to every new field
rather than introducing a new boundary.

## Migration / Rollout

Preset schema v2 -> v3, forward-migrating with documented defaults; no user action required.

**Rollback correction (the proposal is optimistic here):** `fromValueTree` returns
`unsupportedVersion` when `version > kSchemaVersion` (`PresetManager.cpp:151`). A v3 preset
file is therefore **rejected**, not "ignored-unknown-fields", by a reverted v2 build — and
`listPresetNames()` silently drops it from the picker. Rollback is still safe (no crash, no
corruption, old files unaffected), but the honest statement is "v3 presets become invisible
on a reverted build", and that must be verified as written, not assumed away.

## Open Questions

- [ ] BPM slider granularity: `setRange(40, 240, 0.1)` vs integer `1.0`. Design assumes 0.1
      with a text box; neither affects the Transport mechanism. Defer to `sdd-tasks`.
- [ ] Whether Free-mode delay time should snap to the nearest division on toggling Sync on
      (design assumes: Sync recomputes from `(bpm, division)` and overwrites, Free keeps the
      last computed value as its starting point).
