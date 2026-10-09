# Design: Playback Transport & Step Clock

## Technical Approach

A third source tier, `Source/playback/`, sitting **below** `Source/core/`'s JUCE boundary rather than beside it:

- `Source/core/` (`Step`, `Sequence`, `Scale`) — JUCE-free (existing).
- `Source/generation/` (`DeterministicRandom`, …) — `juce_core` only (existing).
- `Source/playback/` (`StepEvent`, `StepEventBuffer`, `Transport`, `SequencePlayer`) — **JUCE-free**, standard library only (`<array>`, `<atomic>`, `<cmath>`), consuming `Source/core/` and nothing else.

That tier is the whole answer to the testability question. The clock takes `int numSamples`, not `juce::AudioSourceChannelInfo`; the audio-buffer type never crosses into the tested surface, so `Tests/BerlinTests.jucer` **stays `juce_core`-only and needs no module addition** (see *Testing Strategy*).

`Source/MainComponent.h/.cpp` — touched for the first time — is the only JUCE-aware code in the change and is reduced to ~10 lines of glue: build the sequence in the constructor's **initialiser list** (before `setAudioChannels`), forward `sampleRate` in `prepareToPlay`, call `process` in `getNextAudioBlock`. The buffer is still cleared; output stays silent (Phase 8).

## Architecture Decisions

### Decision: `Transport` is a query/commit clock, not a callback or a boundary container

**Choice**: `countBoundaries(numSamples)` / `getBoundary(i)` are `const` queries against the *current* position; a single `advance(numSamples)` commits it.
**Alternatives considered**: `advance(n, std::function<void(...)>)` (allocates and type-erases on the audio thread); `advance` filling a second fixed-capacity boundary array (a second capacity constant and overflow policy to reason about).
**Rationale**: zero allocation, zero indirect calls, and each method is independently assertable in a `juce::UnitTest` without a sink object. `SequencePlayer` becomes a nine-line loop.

### Decision: boundaries derive from an absolute integer step counter, never from accumulation

**Choice**: boundary *k* lands at absolute sample `std::llround (k * samplesPerStep)`, where `k` is a `long long` counter and `samplesPerStep` a `double`.
**Alternatives considered**: adding a rounded integer `samplesPerStep` per step; a fractional error accumulator.
**Rationale**: makes the success criterion (`timestamp == round(k * samplesPerStep)` ±1) exactly 0 error and drift *structurally impossible* rather than bounded-by-test. Precision holds: 8 steps/s × 24 h ≈ 6.9e5 steps → `k * 5512.5` ≈ 3.8e9, far inside double's exact-integer range (2^53).

### Decision: the loop wrap (`% size`) lives in `SequencePlayer`, not `Transport`

**Choice**: `Transport` reports a monotonic absolute `stepCounter`; `SequencePlayer` maps it to `stepIndex = counter % sequence.size()`.
**Alternatives considered**: giving `Transport` a `numSteps` and wrapping there.
**Rationale**: `Transport` would otherwise need the sequence length — config the proposal defines as `{bpm, stepsPerBeat}` — and would have to define wrap behaviour for `size() == 0`, which only the sequence owner can answer. **Flagged**: the proposal's scope line puts "loop wrap" under `Transport`. Behaviour is identical; only the owning class differs. See *Open Questions*.

### Decision: fixed capacity 64, drop-and-flag, with a hard structural bound

**Choice**: `StepEventBuffer::capacity = 64`. `push` returns `false` and latches `overflowed` when full.

| Bound | Value |
|---|---|
| Events per boundary | ≤ 2 (note-off + note-on) |
| Boundaries per block | ≤ `floor(maxBlock / samplesPerStep) + 1` |
| Worst documented case: block 8192, 44.1 kHz, a hypothetical future 300 BPM → `samplesPerStep` 2205 | ≤ 4 boundaries → **≤ 8 events** |
| Shipped case: block 8192, 44.1 kHz, 120 BPM → `samplesPerStep` 5512.5 | ≤ 2 boundaries → **≤ 4 events** |
| Chosen capacity | **64** — 8× headroom, 1 KiB (64 × 16 B), member-sized |

**Rationale**: 8192 is ~4× JUCE's typical 512–2048 block, and 300 BPM is 2.5× the shipped constant, so 64 survives both moving at once. The count can never run away regardless: `Transport` treats `samplesPerStep < 1.0` as *not prepared* (zero boundaries), so boundaries ≤ `numSamples` by construction.

### Decision: `SequencePlayer` owns the `Sequence` by value; `MainComponent` owns the player

**Choice**: `SequencePlayer (Sequence, Transport)` stores both by value; `MainComponent` holds one `SequencePlayer` member, built in the initialiser list.
**Alternatives considered**: a `Sequence` member in `MainComponent` with the player holding `const Sequence&`.
**Rationale**: the reference version makes correctness depend on member *declaration order*, silently, on the audio thread. By-value gives one owner, one immutable snapshot, no aliasing. `SequencePlayer` contains a `std::atomic<int>` and is therefore non-copyable/non-movable — which forces in-place initialiser-list construction, i.e. the compiler enforces "fully built before `setAudioChannels`" rather than a comment doing it.

### Decision: header-only for `StepEvent`/`StepEventBuffer`, `.h/.cpp` for `Transport`/`SequencePlayer`

Same rule as `core-sequencing-model`: aggregates and inline forwarders stay header-only; anything with out-of-line definitions gets a `.cpp` so a forgotten `<FILE>` registration fails as an unresolved-external link error instead of silence. `Transport.cpp` and `SequencePlayer.cpp` are that alarm for the whole new group.

## Interfaces / Contracts

```cpp
// Source/playback/StepEvent.h — header-only, JUCE-free, Step.h's aggregate style
namespace berlin {
struct StepEvent
{
    int  sampleOffset = 0;      // samples from the START of the current block
    int  stepIndex    = 0;      // sequence step that OWNS the event (note-off carries
    int  note         = 0;      // the originating step, not the one it lands on)
    bool isNoteOn     = false;
};
static_assert (std::is_aggregate_v<StepEvent>);
bool operator== (const StepEvent&, const StepEvent&);   // non-member, keeps it an aggregate
}
```

`sampleOffset` is relative to the block, **not** to `bufferToFill.startSample`. Future consumers (Phase 5/8) must add `startSample` themselves; this is the single most likely misread in the whole change.

```cpp
// Source/playback/StepEventBuffer.h — header-only, non-allocating sink
namespace berlin {
class StepEventBuffer
{
public:
    static constexpr int capacity = 64;

    void clear() noexcept;                          // count = 0, overflowed = false
    bool push (const StepEvent&) noexcept;          // false = full, event dropped
    int  size() const noexcept;
    bool hasOverflowed() const noexcept;            // latched until clear()
    const StepEvent& operator[] (int index) const noexcept;
private:
    std::array<StepEvent, capacity> events {};
    int  count { 0 };
    bool overflowed { false };
};
}
```

```cpp
// Source/playback/Transport.h — JUCE-free step clock
namespace berlin {
struct StepBoundary { int sampleOffset = 0; long long stepCounter = 0; };

class Transport
{
public:
    Transport (double bpm, int stepsPerBeat) noexcept;   // bpm <= 0 / stepsPerBeat <= 0 -> never prepared

    void prepare (double sampleRate) noexcept;   // samplesPerStep = sampleRate*60/(bpm*stepsPerBeat); resets
    void reset()  noexcept;                      // position 0, counter 0; preserves running state
    void start()  noexcept;
    void stop()   noexcept;

    bool      isRunning()  const noexcept;
    bool      isPrepared() const noexcept;       // samplesPerStep >= 1.0  (guards div-by-zero AND runaway counts)
    double    getSamplesPerStep()  const noexcept;
    long long getSamplePosition()  const noexcept;
    long long getNextStepCounter() const noexcept;

    // --- block protocol: pure queries, then one commit. Allocation-free, noexcept. ---
    int          countBoundaries (int numSamples) const noexcept;   // 0 if stopped or !isPrepared()
    StepBoundary getBoundary (int index) const noexcept;            // 0 <= index < countBoundaries()
    void         advance (int numSamples) noexcept;                 // no-op when stopped
private:
    double samplesPerStep { 0.0 };               // 0 until prepare()
    double bpm; int stepsPerBeat;
    long long position { 0 }, nextStepCounter { 0 };
    bool running { false };
};
}
```

Boundary *k* is at absolute sample `llround (k * samplesPerStep)`; `countBoundaries(n)` counts `k >= nextStepCounter` with that value in `[position, position + n)`. `advance` must add the count computed against the **old** position before moving it.

```cpp
// Source/playback/SequencePlayer.h — JUCE-free
namespace berlin {
class SequencePlayer
{
public:
    SequencePlayer (Sequence sequenceToPlay, Transport transportToUse);

    void prepare (double sampleRate) noexcept;   // forwards + reset(); preserves running state
    void start() noexcept;  void stop() noexcept;  void reset() noexcept;

    void process (int numSamples, StepEventBuffer& out) noexcept;   // AUDIO THREAD, RT-safe

    int getPlayheadStep() const noexcept;        // atomic load, any thread; the ONLY observability seam
private:
    const Sequence sequence;                     // immutable after construction
    Transport transport;
    int pendingNote { -1 };                      // -1 = nothing sounding
    int pendingStep { 0 };
    std::atomic<int> playhead { 0 };
};
}
```

`process` body, in order — this ordering *is* the note-off-before-note-on contract:

```cpp
out.clear();
if (sequence.size() > 0)
{
    const int n = transport.countBoundaries (numSamples);
    for (int i = 0; i < n; ++i)
    {
        const StepBoundary b = transport.getBoundary (i);
        if (pendingNote >= 0)                                     // note-off FIRST, same offset
            out.push ({ b.sampleOffset, pendingStep, pendingNote, false });
        pendingNote = -1;

        const int stepIndex = static_cast<int> (b.stepCounter % sequence.size());   // loop wrap
        const Step& step = sequence[stepIndex];
        if (step.active)
        {
            out.push ({ b.sampleOffset, stepIndex, step.note, true });
            pendingNote = step.note;  pendingStep = stepIndex;
        }
        playhead.store (stepIndex, std::memory_order_relaxed);
    }
}
transport.advance (numSamples);
```

**RT-safety inventory for this body** — safe: `std::array` writes into pre-sized storage, `int`/`long long`/`double` arithmetic, `std::llround`, `%`, relaxed atomic store, `const Sequence::operator[]` (a `std::vector` index, no allocation). Forbidden and absent: any `resize`/`push_back` on `std::vector`, `new`/`delete`, `juce::String`, `juce::Logger`, any mutex, any file or device call. `process` is `noexcept`, which the test suite asserts at compile time.

## `MainComponent` Modification Plan (exact)

`MainComponent.h` — three additions only; `paint`/`resized`/`releaseResources` declarations untouched:

```cpp
#include <JuceHeader.h>                     // unchanged: app tier keeps JuceHeader.h
#include "playback/SequencePlayer.h"        // file-relative from Source/ — no headerPath change
#include "playback/StepEventBuffer.h"
...
private:
    static berlin::Sequence buildSeededSequence();   // message thread only
    berlin::SequencePlayer  player;                  // built in the ctor init list
    berlin::StepEventBuffer blockEvents;             // audio-thread scratch
```

`MainComponent.cpp` — constants in an anonymous namespace (`kBpm = 120.0`, `kStepsPerBeat = 4`, `kNumSteps = 16`, `kSeed = 12345`), plus:

| Method | Change |
|---|---|
| ctor **init list** | `: player (buildSeededSequence(), berlin::Transport (kBpm, kStepsPerBeat))` — runs before the body, therefore before `setAudioChannels(2, 2)`. Non-movable `player` makes any later-assignment refactor a compile error. |
| ctor **body** | `setSize(800,600)` unchanged; add `player.start();` **before** the existing `RuntimePermissions` / `setAudioChannels` block. Nothing else moves. |
| `buildSeededSequence()` | `DeterministicRandom rng (kSeed); auto seq = RhythmGenerator (kNumSteps, 0.5f).generate (rng); PitchGenerator pitch (Scale::minor(48), 36, 72); for each step: if active, seq[i].note = pitch.generateNextNote (rng); return seq;` — message thread, allocation allowed. |
| `prepareToPlay` | body becomes `juce::ignoreUnused (samplesPerBlockExpected); player.prepare (sampleRate);`. JUCE calls this on the **audio thread** (per the stock comment) — `prepare` is arithmetic only, no allocation. |
| `getNextAudioBlock` | `bufferToFill.clearActiveBufferRegion();` stays **first**; then `player.process (bufferToFill.numSamples, blockEvents);`. `blockEvents` is consumed by nobody this phase. |
| `releaseResources` | **unchanged** (empty). Running state is set once at construction; `prepare` preserves it, so a device restart resumes correctly. |
| `paint`, `resized`, dtor | **unchanged**. Must be absent from the final diff. |

`Source/Main.cpp` is untouched.

## Data Flow

```
[MESSAGE THREAD — MainComponent ctor init list]
  seed 12345 -> DeterministicRandom --+-> RhythmGenerator(16, 0.5) -> Sequence (active flags)
                                      +-> PitchGenerator(minor(48), 36..72) -> notes
                                                     |
                                         buildSeededSequence()
                                                     v
                                SequencePlayer { const Sequence, Transport(120, 4) }
                                                     v
                                         setAudioChannels(2, 2)   <-- audio starts HERE
--------------------------------------------------------------------------------
[AUDIO THREAD]
  prepareToPlay(sr) -> Transport::prepare -> samplesPerStep = sr*60/(bpm*4)

  getNextAudioBlock(n)
    |- clearActiveBufferRegion()                       (silence; Phase 8 fills it)
    +- SequencePlayer::process(n, StepEventBuffer&)
         |- Transport::countBoundaries/getBoundary     (pure arithmetic, const)
         |- push note-off(prev) THEN note-on(cur)      (fixed capacity, no alloc)
         |- playhead.store(stepIndex, relaxed) --------> readable by ANY thread (Phase 7 UI)
         +- Transport::advance(n)
                       v
              StepEventBuffer  --X  consumed by nobody yet (Phase 5 MIDI / Phase 8 voices)
```

## File Changes

| File | Action | Description |
|---|---|---|
| `Source/playback/StepEvent.h` | Create | Aggregate `{sampleOffset, stepIndex, note, isNoteOn}` + non-member `operator==` |
| `Source/playback/StepEventBuffer.h` | Create | `std::array`-backed, capacity 64, drop-and-flag |
| `Source/playback/Transport.h` / `.cpp` | Create | Query/commit step clock, absolute-position boundaries |
| `Source/playback/SequencePlayer.h` / `.cpp` | Create | Sequence + Transport → ordered `StepEvent`s, atomic playhead |
| `Source/MainComponent.h` | Modify | 2 includes, 1 static helper, 2 members |
| `Source/MainComponent.cpp` | Modify | ctor init list + `player.start()`, `prepareToPlay`, `getNextAudioBlock`, helper |
| `Tests/Source/TransportTests.cpp` | Create | Clock math, boundaries, degenerate inputs |
| `Tests/Source/SequencePlayerTests.cpp` | Create | Event ordering, wrap, inactive/empty, playhead, buffer overflow |
| `Tests/Source/PlaybackTimingTests.cpp` | Create | Absolute-timestamp + long-run drift suite |
| `Berlin.jucer`, `Tests/BerlinTests.jucer` | Modify | New `<GROUP name="playback">` `<FILE>` entries; **module lists unchanged** |
| `Builds/`, `JuceLibraryCode/`, `Tests/Builds/`, `Tests/JuceLibraryCode/` | Regenerate | `Projucer.exe --resave` output |
| `Source/Main.cpp`, `Source/core/`, `Source/generation/` | Untouched | Must be absent from the final diff |

## Projucer Registration Mechanics

New group in both projects, mirroring the existing `core`/`generation` convention (brace-GUID group `id`, unique short file `id`s, headers `compile="0"`, sources `compile="1"`, `../` prefix on the test side because `<FILE file>` resolves relative to the `.jucer`'s own directory):

```xml
<!-- Berlin.jucer, inside MAINGROUP, after the "generation" group -->
<GROUP id="{2E6F8A1B-5C3D-4F70-9A82-6B4D8E0F2A31}" name="playback">
  <FILE id="pbEvtH" name="StepEvent.h"        compile="0" resource="0" file="Source/playback/StepEvent.h"/>
  <FILE id="pbBufH" name="StepEventBuffer.h"  compile="0" resource="0" file="Source/playback/StepEventBuffer.h"/>
  <FILE id="pbTrnH" name="Transport.h"        compile="0" resource="0" file="Source/playback/Transport.h"/>
  <FILE id="pbTrnC" name="Transport.cpp"      compile="1" resource="0" file="Source/playback/Transport.cpp"/>
  <FILE id="pbSplH" name="SequencePlayer.h"   compile="0" resource="0" file="Source/playback/SequencePlayer.h"/>
  <FILE id="pbSplC" name="SequencePlayer.cpp" compile="1" resource="0" file="Source/playback/SequencePlayer.cpp"/>
</GROUP>

<!-- Tests/BerlinTests.jucer — same block, "../" prefixed, tp* ids -->
<FILE id="tpTrnC" name="Transport.cpp" compile="1" resource="0" file="../Source/playback/Transport.cpp"/>
```

Plus three `<FILE compile="1">` entries in the test project's own `Source` group (`btTrnC`, `btSplC`, `btPtmC`). No exporter change is needed: the app includes `"playback/SequencePlayer.h"` file-relative from `Source/MainComponent.h`, and tests include `"playback/Transport.h"` via the existing `headerPath="../../../Source"`. **No `<MODULE>` line changes in either file** — a diff touching `<MODULES>` is a defect.

**Mandatory checklist after ANY file add/rename/delete** (unchanged from `core-sequencing-model`, a task step not an aside):

1. Add/update `<FILE>` in `Berlin.jucer` (`Source/...`).
2. Add/update `<FILE>` in `Tests/BerlinTests.jucer` (`../Source/...`).
3. `Projucer.exe --resave Berlin.jucer`
4. `Projucer.exe --resave Tests/BerlinTests.jucer`
5. **Sync check**: the `Source/(core|generation|playback)/*` entry sets must match exactly modulo the `../` prefix, and `git diff` must show no `<MODULE>` change. A mismatch is a defect, not a warning.
6. Build both projects.

## Testing Strategy

The headless harness never touches an audio device *or* an audio buffer type. Tests call `player.prepare (44100.0)` and then `player.process (blockSize, buffer)` directly with plain integers — `juce::AudioSourceChannelInfo`/`AudioBuffer` live in `juce_audio_basics` and appear **only** inside `MainComponent::getNextAudioBlock`, which the test project does not compile. **Confirmed: no module addition to `Tests/BerlinTests.jucer`.**

| Layer | What to Test | Approach |
|---|---|---|
| Compile-time | `StepEvent` is an aggregate; `process` is `noexcept`; `StepEventBuffer` is trivially copyable | `static_assert (std::is_aggregate_v<StepEvent>)`; `static_assert (noexcept (player.process (0, buf)))` — a compile-time proof of the no-throw/RT contract |
| Unit — `Transport` | `samplesPerStep` at 44.1/48/96 kHz; boundary offsets across irregular block sizes and blocks **shorter** than a step; stopped, unprepared, `bpm <= 0`, `stepsPerBeat <= 0` | `TransportTests.cpp`, direct query/advance calls |
| Unit — `StepEventBuffer` | Capacity, `push` returns false when full, event dropped, `hasOverflowed` latches, `clear` resets both | Fill past 64 deliberately |
| Unit — `SequencePlayer` | Note-off emitted **before** note-on at a shared offset; inactive steps emit nothing; empty sequence emits nothing; loop wrap has no gap or double event; `getPlayheadStep` tracks the last boundary | Fixed hand-built `Sequence`, not a generated one, so assertions are exact |
| Timing (the deliverable) | Absolute timestamp of boundary *k* equals `llround (k * samplesPerStep)` within ±1, across ≥ one full loop wrap; note values match the seeded sequence in order | `PlaybackTimingTests.cpp` sums block lengths into an absolute position and rebases each event's `sampleOffset` |
| Drift | Thousands of steps with mixed block sizes; cumulative error stays bounded (must be exactly 0 by construction) | Same suite, long-run case |
| Review-only | No allocation/lock/logging in `getNextAudioBlock` or `process` | Explicit reviewer checklist against the constitution rule — C++ offers no runtime assertion here |
| Manual gate | App builds, launches, opens its device, runs silently, no dropouts or asserts | Standalone launch; the **only** coverage for the `MainComponent` glue |
| Build | Both projects resave and compile; file-list sync; `<MODULES>` unchanged | `Projucer.exe --resave` ×2 + `git diff` |

**Known coverage gap, stated rather than hidden**: the ~10 lines of `MainComponent` glue (init-list ordering, `prepare` forward, `process` call) cannot be reached by a `juce_core`-only harness. That is the deliberate price of keeping `juce_audio_basics` out of the test project, and it is why the glue is kept to a size a reviewer can verify by eye.

## Threat Matrix

N/A — no routing, shell command, subprocess, VCS/PR automation, executable-file classification, or process-integration boundary. `Source/playback/` is pure in-process arithmetic over value types with no I/O; `MainComponent` gains no new external surface. The `Projucer.exe --resave` invocations are developer/CI build steps on trusted repo-local paths, not a runtime boundary of the shipped artifact.

The real safety boundary of this change is the **audio thread**, covered above by the RT-safety inventory, `noexcept` static assertion, fixed-capacity sink, and reviewer checklist.

## Migration / Rollout

No data migration, no feature flag, no persisted state or file format. The `Source/playback/` tier and the test suites are purely additive; the only non-additive edit is `Source/MainComponent.h/.cpp`, whose pre-change content is the untouched Projucer template — so the reverted state is byte-verifiable. Rollback = revert the commits, restore both `.jucer` file lists, `--resave` both, rebuild. The app returns to clearing the buffer and the existing 34 tests stay green.

## Open Questions

- [ ] **Loop-wrap ownership.** The proposal's scope line lists "loop wrap" under `Transport`; this design puts the `% size()` in `SequencePlayer` because `Transport` is deliberately sequence-length-agnostic. Behaviour is identical — flagging it so a spec requirement worded "the transport wraps" gets re-homed to `step-event-scheduling` rather than quietly failing review.
- [ ] **Demo sequence parameters are unspecified.** The proposal fixes the seed's *existence*, not its value. Design proposes `kSeed = 12345`, 16 steps, density `0.5f`, `Scale::minor(48)`, range `[36, 72]`. These affect only the standalone demo, no tested contract — confirm or override at apply time.
- [ ] **`stop()` emits no all-notes-off flush.** Nothing can hear a hanging note this phase, and a flush needs a sample offset with no boundary to attach to. Deferred to Phase 5, where a stuck MIDI note first becomes observable. Confirm this is acceptable rather than an omission.
- [ ] **Max observed block size.** Capacity 64 is sized against an 8192-sample block; confirm at apply time that the device never calls `getNextAudioBlock` with more than `samplesPerBlockExpected` (JUCE does not formally guarantee it). If it can, `hasOverflowed` makes the violation visible instead of undefined.
