# Design: MIDI Export

## Technical Approach

A fifth source tier, `Source/export/`, sitting **beside** `Source/midi/` (not above it) and depending on neither `Source/midi/` nor `Source/playback/`:

- `Source/export/MidiExportTimeline.h/.cpp` — **JUCE-free**, `<vector>` + `core/Sequence.h` only. Registered in **both** `.jucer` projects; fully unit-tested.
- `Source/export/MidiFileWriter.h/.cpp` — `juce_audio_basics` + `juce_core`. Registered in `Berlin.jucer` **only**; manual-gate-only.
- `Source/MainComponent.h/.cpp` — one new member, two new constants, one new private static helper, ctor init-list + ctor body touched. **No new UI**: `paint`/`resized` must be absent from the final diff.

The chain is `Sequence → MidiExportTimeline (ticks) → juce::MidiFile → juce::TemporaryFile → target .mid`. `Tests/BerlinTests.jucer` gains one `<GROUP name="export">` (header + `.cpp`) and one test `.cpp`; its `<MODULES>` list stays `juce_core`-only — a diff touching `<MODULES>` is a defect.

**Deliberate deviation from the SDD 800-word design budget.** The proposal delegated a full open item plus five buildable artifacts to this document; the two immediate predecessor designs are 3-10x this length. Tables are used in place of prose wherever possible.

## Ownership Ledger (read this first)

| Concern | Owner | Exact site |
|---|---|---|
| Tick math, note-off pairing, terminal note-off, repeats | `berlin::buildMidiExportTimeline` (free function) | `MidiExportTimeline.cpp` — the only musical logic in the change |
| PPQ constant `kTicksPerQuarterNote = 960` | `MidiExportTimeline.h` — **one definition** | Consumed by `MidiFileWriter::buildFile` via `timeline.ticksPerQuarterNote` |
| Tempo + 4/4 meta-events, SMF header, byte streaming | `berlin::MidiFileWriter` | `MidiFileWriter.cpp` |
| Atomic write / partial-file protection | `MidiFileWriter::writeToFile` (owns the `juce::TemporaryFile`) | `MidiFileWriter.cpp` |
| **Output path resolution** | `MainComponent::resolveExportFile()` — **temporary hook, Phase 7 deletes it** | `MainComponent.cpp` |
| **Export trigger** | `MainComponent` ctor body | `MainComponent.cpp` — the only call site |
| Velocity value | `berlin::MidiEventTranslator::kNoteVelocity` — **still one definition** | Injected into `MidiFileWriter`'s ctor from `MainComponent` |
| Output channel `kMidiChannel` | `MainComponent.cpp` anonymous namespace — **still one definition** | Injected into `MidiFileWriter`'s ctor |
| BPM / `stepsPerBeat` / repeat count | `MainComponent.cpp` anonymous namespace | Injected per call |
| The `Sequence` being exported | `MainComponent::sequence` (new `const` member) | `Source/playback/` gains **no** accessor — see Decision 2 |

## Architecture Decisions

### Decision 1: the export trigger is a one-shot call in the `MainComponent` constructor (resolves the proposal's open item)

**Choice**: export fires unconditionally once during construction, inserted **after** `midiSink.openFirstAvailableDevice()` and **before** the `RuntimePermissions`/`setAudioChannels` block. Launching the app deterministically produces the manual-gate artifact. ~9 lines of glue.

| Alternative | Rejected because |
|---|---|
| `keyPressed` / `KeyListener` override | Requires focus handling and is a user-facing affordance — Phase 7 UI pulled forward |
| Export in the destructor | The artifact only appears after quitting; a crash loses it; ordering versus `shutdownAudio()` becomes load-bearing for no gain |
| `juce::Timer` one-shot after launch | Adds a lifetime and a race with the ctor for zero benefit over calling it directly |
| Test-only entry point, no app trigger | Removes the falsifiable DAW-import gate, which *is* the phase deliverable |

**Rationale**: this is the exact shape of Phase 5's accepted `openFirstAvailableDevice()`-in-the-ctor precedent — non-interactive, deterministic, no UI. Placing it before `setAudioChannels` means no audio thread exists yet, so the new `sequence` member has provably zero concurrent readers, and export cannot contend with the callback. Keeping the two existing setup lines in their proven order makes the ctor-body diff a pure insertion.

### Decision 2: `MainComponent` owns the `Sequence`; `SequencePlayer` gains no accessor

**Choice**: new member `const berlin::Sequence sequence;` declared **before** `player`; init list becomes `sequence (buildSeededSequence()), player (sequence, berlin::Transport (kBpm, kStepsPerBeat))`. `buildSeededSequence()` is still called exactly once.

**Alternatives considered**: `SequencePlayer::getSequence()`; calling `buildSeededSequence()` a second time for export (deterministic, so it would work).

**Rationale**: an accessor would modify `Source/playback/SequencePlayer.h`, violating the proposal's success criterion *"no file under `Source/playback/` or `Source/midi/` is modified"*. Calling the builder twice makes the invariant "the exported file matches what you hear" depend on generator determinism instead of on a single object. Member declaration order is load-bearing — `sequence` must precede `player` or the copy reads an uninitialised object.

### Decision 3: output path is `Documents/Berlin/berlin-export.mid`, resolved in `MainComponent`, never in `Source/export/`

**Choice**: `juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Berlin").getChildFile ("berlin-export.mid")`. Fixed filename, overwritten every launch. `MidiFileWriter` accepts a `juce::File` and resolves nothing.

| Alternative | Rejected because |
|---|---|
| Hardcoded absolute path | Non-portable; breaks on any other machine or CI runner |
| `File::getCurrentWorkingDirectory()` | CWD differs between the VS debugger, Explorer, and a shell — the manual gate could not reliably find the artifact |
| `tempDirectory` | Portable but undiscoverable and subject to OS reaping — hostile to a DAW-import gate |
| `userDesktopDirectory` | Visible, but pollutes the user's desktop on every launch |
| Timestamped filenames | Accumulates junk indefinitely from a hook that is meant to be deleted in Phase 7 |
| Path resolution inside `MidiFileWriter` | Bakes the temporary policy into the permanent tier; Phase 7's file picker would then have to *remove* code from `Source/export/` |

**Rationale**: `userDocumentsDirectory` is writable and present on all three platforms, discoverable by a human running the gate, and trivially cleanable. Keeping resolution in `MainComponent` makes the whole temporary policy deletable in Phase 7 without touching `Source/export/` — the seam matches the proposal's "temporary hook" mandate.

### Decision 4: write failure is a typed return, logged and swallowed — startup never aborts

**Choice**: `writeToFile` returns `MidiFileWriteResult`; `MainComponent` logs a non-`ok` result via `juce::Logger::writeToLog` and continues. No `jassertfalse`, no throw, no dialog, no retry.

| Failure | Detected by | Behaviour |
|---|---|---|
| `Documents` unresolvable | `dir == juce::File()` | `pathUnavailable`; nothing written |
| `Berlin/` cannot be created | `dir.createDirectory()` returns a failed `juce::Result` | `pathUnavailable`; nothing written |
| Target exists and is a directory, or parent is read-only | `TemporaryFile::overwriteTargetFileWithTemporary()` returns `false` | `writeFailed`; temp auto-deleted, **previous good export left intact** |
| Stream cannot be opened | `FileOutputStream::openedOk()` is `false` | `writeFailed`; nothing written |
| Disk full / I/O error mid-write | `OutputStream::write*` returns `false`, or `getStatus()` not ok | `writeFailed`; temp auto-deleted, no truncated `.mid` on disk |
| Timeline rejected upstream | `MidiExportStatus != ok` | Writer is never called |

**Rationale**: a read-only or exotic filesystem must not prevent Berlin from generating and playing. `jassert` would break a developer's debug run for a non-defect. `juce::TemporaryFile` (write temp → `overwriteTargetFileWithTemporary`) is chosen over writing the target directly because it eliminates **two** traps in one construct: (a) `juce::FileOutputStream` opens an existing file **for append**, so writing a shorter export over a longer one would leave trailing garbage and an unparseable `.mid`; (b) any mid-write failure would otherwise leave a truncated file that a DAW reports as corrupt. Both are silent-corruption failures that a "did it return true?" check would miss.

### Decision 5: the tick-domain transform takes no BPM

**Choice**: `buildMidiExportTimeline (sequence, stepsPerBeat, repeats, out)`. Tempo enters only as the writer's meta-event.

**Alternatives considered**: threading `bpm` through the transform for symmetry with `Transport (bpm, stepsPerBeat)`.

**Rationale**: SMF ticks are *musical*, not temporal — tempo is metadata. Excluding BPM makes "no accumulated rounding" true by construction rather than by test, removes the only floating-point input from the tested unit, and makes it structurally impossible for a tempo change to alter note placement. This is the sharpest divergence from `Transport`, which *must* consume BPM because it converts to samples.

### Decision 6: PPQ = 960

**Choice**: `inline constexpr int kTicksPerQuarterNote = 960;` → 240 ticks per 16th step at `stepsPerBeat = 4`.

| Alternative | Tradeoff |
|---|---|
| 96 (classic SMF) | Divisible by 1-4, 6, 8, 12, 16, 24, 32 — adequate now, too coarse if `stepsPerBeat` ever reaches 5, 10, or 20 |
| 480 (DAW de-facto) | Covers 5-tuplets; strictly a subset of 960's divisors |
| 960 | Divisible by every plausible `stepsPerBeat` (1-6, 8, 10, 12, 16, 20, 24, 32, 48, 64), matches common DAW internal resolution so import is rounding-free, and is well inside SMF's 15-bit division field (max 32767) |

**Rationale**: the headroom is free — the division field is one 16-bit header value. `kTicksPerQuarterNote % stepsPerBeat != 0` is **rejected**, never rounded (Requirement: `invalidStepsPerBeat`), so the constant can never silently degrade timing.

### Decision 7: note-off shares the next note-on's tick, note-off first, and the sequence is **never** re-sorted

**Choice**: events are emitted in final order — non-decreasing tick, and at an equal tick a note-off always precedes a note-on. `MidiFileWriter` calls `juce::MidiMessageSequence::addEvent` in that order and **never** calls `sort()` or `updateMatchedPairs()`.

**Alternatives considered**: note-off at `tick - 1`; building unordered and calling `sort()`.

**Rationale**: same-tick placement mirrors live playback *exactly* — `SequencePlayer::process` pushes both events at the identical `sampleOffset` with the note-off first — and satisfies "the note lasts exactly one step and ends where the next begins". `sort()` is refused because a non-stable sort could swap an equal-tick note-off/note-on pair; for two consecutive active steps on the **same pitch** (which Berlin's generator produces) that inversion yields a stuck note in the importing DAW — the change's highest-severity risk.
**Verify at apply**: `juce::MidiMessageSequence::addEvent` is believed to insert *after* existing events at the same timestamp, preserving insertion order. This could not be confirmed against a local JUCE checkout (modules resolve via the global path — same caveat Phase 5 recorded for `clearAllPendingMessages`). If verification shows equal-timestamp reordering, fall back to emitting the note-off at `tick - 1` and record the substitution in the verify report.

### Decision 8: `Source/export/` depends on `Source/core/` only — velocity and channel are injected

**Choice**: `MidiFileWriter (int outputChannel, int velocity)`, both clamped at construction. `MainComponent` passes `kMidiChannel` and `berlin::MidiEventTranslator::kNoteVelocity`.

**Alternatives considered**: `MidiFileWriter` including `midi/MidiEventTranslator.h` to read the constant directly; a second velocity constant in `Source/export/`.

**Rationale**: a second constant is the classic drift bug (the exported file would disagree with what you hear). Including `Source/midi/` from `Source/export/` couples the offline tier to the realtime tier for one integer. Injection keeps **exactly one** definition of each constant — Phase 5's ownership-ledger discipline — while leaving `Source/export/`'s only intra-project dependency as `core/Sequence.h`.

## Interfaces / Contracts

```cpp
// Source/export/MidiExportTimeline.h — JUCE-FREE. Registered in BOTH .jucer projects.
#pragma once
#include <type_traits>
#include <vector>
#include "core/Sequence.h"

namespace berlin
{

inline constexpr int kTicksPerQuarterNote = 960;   // Decision 6

struct MidiExportEvent
{
    long long tick   = 0;       // absolute ticks from the start of the file
    int       note   = 0;       // MIDI note number
    bool      noteOn = false;   // false => note-off
};
static_assert (std::is_aggregate_v<MidiExportEvent>);
bool operator== (const MidiExportEvent&, const MidiExportEvent&);   // non-member: keeps it an aggregate

struct MidiExportTimeline
{
    std::vector<MidiExportEvent> events;                                  // final order; never re-sorted
    long long                    endTick             = 0;                 // repeats * size * ticksPerStep
    int                          ticksPerQuarterNote = 0;                 // 0 until built
    int                          ticksPerStep        = 0;                 // 0 until built
};

enum class MidiExportStatus { ok, invalidRepeatCount, invalidStepsPerBeat };

// Message thread, allocating. `out` is cleared first and left EMPTY on any non-ok status
// (never partially filled). An empty or all-inactive `sequence` returns `ok` with no events.
MidiExportStatus buildMidiExportTimeline (const Sequence& sequence,
                                          int stepsPerBeat,
                                          int repeats,
                                          MidiExportTimeline& out);
}
```

Algorithm — this is the contract, and it is a **deliberate tick-domain duplicate** of `SequencePlayer::process`'s note-off rule (proposal Approach 5; both spec sections must cross-reference so a future gate-length change updates both):

```cpp
if (repeats      < 1) return MidiExportStatus::invalidRepeatCount;     // 0 and negatives REJECTED, never clamped
if (stepsPerBeat < 1
    || kTicksPerQuarterNote % stepsPerBeat != 0) return MidiExportStatus::invalidStepsPerBeat;

const int       ticksPerStep = kTicksPerQuarterNote / stepsPerBeat;    // exact by the guard above
const long long totalSteps   = (long long) repeats * sequence.size();
int             pendingNote  = -1;

for (long long k = 0; k < totalSteps; ++k)
{
    const long long tick = k * ticksPerStep;          // absolute, never accumulated -> no drift at repeat N

    if (pendingNote >= 0)                             // note-off FIRST, SAME tick (Decision 7)
        out.events.push_back ({ tick, pendingNote, false });
    pendingNote = -1;

    const Step& step = sequence[(int) (k % sequence.size())];   // loop wrap == SequencePlayer's
    if (step.active)
    {
        out.events.push_back ({ tick, step.note, true });
        pendingNote = step.note;
    }
}

out.endTick = totalSteps * ticksPerStep;
if (pendingNote >= 0)                                 // TERMINAL note-off — the highest-flagged risk
    out.events.push_back ({ out.endTick, pendingNote, false });
```

The repeat seam is correct **by construction**: `k` is a single absolute counter and `% sequence.size()` is the only wrap, so step `size()` of repeat 1 is indistinguishable from step 0 — no seam-specific branch exists to get wrong. `totalSteps == 0` (empty sequence) yields no events and `endTick == 0`.

```cpp
// Source/export/MidiFileWriter.h — juce_audio_basics + juce_core. Berlin.jucer ONLY.
#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include "export/MidiExportTimeline.h"

namespace berlin
{

enum class MidiFileWriteResult { ok, invalidTimeline, pathUnavailable, writeFailed };

class MidiFileWriter
{
public:
    MidiFileWriter (int outputChannel, int velocity) noexcept;   // both clamped, Decision 8

    // No musical decisions: translates an ALREADY-CORRECT timeline 1:1.
    // Track order: tempoMetaEvent(0), timeSignatureMetaEvent(4,4)@0, events, endOfTrack@endTick.
    void buildFile (const MidiExportTimeline& timeline, double bpm, juce::MidiFile& destination) const;

    bool writeTo (const MidiExportTimeline& timeline, double bpm, juce::OutputStream& out) const;

    // MESSAGE THREAD. Atomic via juce::TemporaryFile (Decision 4). Creates the parent
    // directory if needed. Never throws, never asserts.
    MidiFileWriteResult writeToFile (const MidiExportTimeline& timeline,
                                     double bpm,
                                     const juce::File& destination) const;
private:
    const int channel;    // [1, 16]
    const int velocity;   // [0, 127]
};
}
```

`buildFile` specifics: `destination.setTicksPerQuarterNote (timeline.ticksPerQuarterNote)`; tempo = `juce::MidiMessage::tempoMetaEvent ((int) std::llround (60'000'000.0 / bpm))` (exactly `500000` at 120 BPM); `juce::MidiMessage::timeSignatureMetaEvent (4, 4)`; notes built as `juce::MidiMessage (bytes.status, bytes.data1, bytes.data2)`; single track written as SMF type 1 (JUCE's `writeTo` default).

## Migration / Rollout

No data migration, no feature flag, no persisted app state, no schema. `Source/export/` and `Tests/Source/MidiExportTimelineTests.cpp` are purely additive. The only non-additive edit is `Source/MainComponent.h/.cpp`, whose pre-change content is the archived Phase 5 state, so the reverted result is byte-verifiable. Rollback = revert the commits, restore both `.jucer` `<FILE>` lists, `--resave` both, rebuild; the app returns to generating and playing live MIDI with no export, and every existing suite stays green. Already-written `.mid` files are inert user data outside the repo — nothing else persists, so the revert is complete.

The export trigger and `resolveExportFile()` are **declared temporary in code comments** so Phase 7 replaces them rather than building a UI on top of them. `Source/export/`'s public interface is designed to survive that replacement unchanged: Phase 7 supplies a different `juce::File` and a different call site, nothing more.
