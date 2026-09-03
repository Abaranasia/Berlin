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

> **Note-off status byte**: `Source/export/` does **not** include `midi/MidiMessageBytes.h` (Decision 8's tier boundary), so it emits `juce::MidiMessage::noteOff (channel, note, (juce::uint8) 0)`. JUCE's `noteOff` produces a real `0x80` status with release velocity 0 — byte-identical to `berlin::makeNoteOff`, and distinguishable from a velocity-0 note-on in a hex dump. Apply-time check: confirm the emitted status byte is `0x80 | (channel - 1)`.

## Data Flow

```
[MESSAGE THREAD — MainComponent ctor, BEFORE any audio thread exists]
  init list: sequence (buildSeededSequence())            <-- declared BEFORE player (Decision 2)
             player (sequence, Transport (kBpm, kStepsPerBeat))
             midiTranslator (kMidiChannel), midiSink (kMidiChannel)
  body:      setSize (800, 600)
             player.start()
             midiSink.openFirstAvailableDevice()          <-- Phase 5, unchanged
             ---- NEW: one-shot export (Decision 1) ----
             buildMidiExportTimeline (sequence, kStepsPerBeat, kExportRepeats, timeline)
               |  invalidRepeatCount / invalidStepsPerBeat -> Logger::writeToLog, NO file written
               v  ok
             MidiFileWriter (kMidiChannel, MidiEventTranslator::kNoteVelocity)
               .writeToFile (timeline, kBpm, resolveExportFile())
                  |- resolveExportFile(): Documents/Berlin/berlin-export.mid   (Decision 3)
                  |- dir.createDirectory()                -> failed? pathUnavailable
                  |- juce::TemporaryFile temp (destination)
                  |- FileOutputStream on temp.getFile()   -> !openedOk()? writeFailed
                  |- buildFile -> MidiFile::writeTo (stream)
                  +- stream flush ok? temp.overwriteTargetFileWithTemporary()
                        ok      -> MidiFileWriteResult::ok
                        failure -> temp dtor deletes it; PREVIOUS export untouched
               |  != ok -> juce::Logger::writeToLog ("Berlin: MIDI export failed ...")
               v
             STARTUP CONTINUES REGARDLESS (Decision 4)
             RuntimePermissions / setAudioChannels (2, 2)  <-- audio starts HERE, export already done
--------------------------------------------------------------------------------
[AUDIO THREAD]  prepareToPlay / getNextAudioBlock / releaseResources — BYTE-FOR-BYTE UNCHANGED.
                Export shares no state with the callback; `sequence` is const and read once,
                before the audio device is ever opened.
```

## File Changes

| File | Action | Description |
|---|---|---|
| `Source/export/MidiExportTimeline.h` | Create | `kTicksPerQuarterNote`, `MidiExportEvent`, `MidiExportTimeline`, `MidiExportStatus`, `buildMidiExportTimeline`. JUCE-free |
| `Source/export/MidiExportTimeline.cpp` | Create | The algorithm above. Out-of-line so a missing `<FILE>` fails as an unresolved external |
| `Source/export/MidiFileWriter.h` / `.cpp` | Create | `juce::MidiFile` build + meta-events + atomic `writeToFile` |
| `Source/MainComponent.h` | Modify | `#include "export/MidiExportTimeline.h"`, `#include "export/MidiFileWriter.h"`; new `const berlin::Sequence sequence;` **before** `player`; new `static juce::File resolveExportFile();` |
| `Source/MainComponent.cpp` | Modify | `kExportRepeats = 4`, `kExportFileName`; `resolveExportFile()`; ctor init list; ctor body insertion. Nothing else |
| `Tests/Source/MidiExportTimelineTests.cpp` | Create | See Testing Strategy |
| `Berlin.jucer` | Modify | New `<GROUP name="export">`. **`<MODULES>` unchanged** (`juce_audio_basics` already linked) |
| `Tests/BerlinTests.jucer` | Modify | `<GROUP name="export">` with the timeline `.h`/`.cpp` **only**, plus the test `.cpp`. **`<MODULES>` stays `juce_core`-only** |
| `openspec/specs/unit-test-harness/spec.md` | Modify | Requirement 1 + "Shared sources are not duplicated" generalized from `Source/core/*` and `Source/generation/*` to *all JUCE-free source tiers* (which already includes `playback`/`midi` in practice, and now `export`) |
| `Builds/`, `JuceLibraryCode/`, `Tests/Builds/`, `Tests/JuceLibraryCode/` | Regenerate | `Projucer.exe --resave` x2 |
| `Source/core/`, `Source/generation/`, `Source/playback/`, `Source/midi/`, `Source/Main.cpp` | Untouched | Must be absent from the final diff |
| `MainComponent::paint` / `resized` / `prepareToPlay` / `getNextAudioBlock` / `releaseResources` / dtor | Untouched | Must be absent from the final diff |

## `MainComponent` Modification Plan (exact)

`MainComponent.h` — additions only:

```cpp
#include "export/MidiExportTimeline.h"
#include "export/MidiFileWriter.h"
...
private:
    static berlin::Sequence buildSeededSequence();
    static juce::File       resolveExportFile();     // TEMPORARY hook — deleted in Phase 7

    const berlin::Sequence      sequence;            // MUST precede `player` (Decision 2)
    berlin::SequencePlayer      player;              // now constructed FROM `sequence`
    berlin::StepEventBuffer     blockEvents;         // unchanged
    berlin::MidiEventTranslator midiTranslator;      // unchanged
    berlin::MidiOutputSink      midiSink;            // unchanged
    juce::MidiBuffer            midiBlock;           // unchanged
```

`MainComponent.cpp` — anonymous namespace gains `constexpr int kExportRepeats = 4;` (4 x 16 steps @ 16ths = 16 beats = **exactly 4 bars of 4/4**, a countable manual-gate assertion) and `const char* kExportFileName = "berlin-export.mid";`.

| Method | Change |
|---|---|
| ctor **init list** | `sequence (buildSeededSequence()), player (sequence, berlin::Transport (kBpm, kStepsPerBeat)), midiTranslator (kMidiChannel), midiSink (kMidiChannel)` |
| ctor **body** | pure insertion after `midiSink.openFirstAvailableDevice();`, still before the `RuntimePermissions` block: build timeline → construct writer → `writeToFile` → `Logger::writeToLog` on any non-`ok` result. Return value must **not** be silently discarded |
| `resolveExportFile()` | new; `getSpecialLocation (userDocumentsDirectory).getChildFile ("Berlin").getChildFile (kExportFileName)` |
| everything else | **unchanged** |

## Projucer Registration Mechanics

New group in both projects, following the existing `core`/`generation`/`playback`/`midi` convention (brace-GUID group `id`, unique short file `id`s, headers `compile="0"`, sources `compile="1"`, `../` prefix on the test side):

```xml
<!-- Berlin.jucer, inside MAINGROUP, after the "midi" group -->
<GROUP id="{6B2D4F8A-1C3E-49D5-8A7B-0F2C4E6A8D10}" name="export">
  <FILE id="exTmlH" name="MidiExportTimeline.h"   compile="0" resource="0" file="Source/export/MidiExportTimeline.h"/>
  <FILE id="exTmlC" name="MidiExportTimeline.cpp" compile="1" resource="0" file="Source/export/MidiExportTimeline.cpp"/>
  <FILE id="exWrtH" name="MidiFileWriter.h"       compile="0" resource="0" file="Source/export/MidiFileWriter.h"/>
  <FILE id="exWrtC" name="MidiFileWriter.cpp"     compile="1" resource="0" file="Source/export/MidiFileWriter.cpp"/>
</GROUP>

<!-- Tests/BerlinTests.jucer — the JUCE-FREE half ONLY. MidiFileWriter MUST NOT appear here. -->
<GROUP id="{6B2D4F8A-1C3E-49D5-8A7B-0F2C4E6A8D10}" name="export">
  <FILE id="teTmlH" name="MidiExportTimeline.h"   compile="0" resource="0" file="../Source/export/MidiExportTimeline.h"/>
  <FILE id="teTmlC" name="MidiExportTimeline.cpp" compile="1" resource="0" file="../Source/export/MidiExportTimeline.cpp"/>
</GROUP>
<!-- and in the test project's own Source group: -->
<FILE id="btMxtC" name="MidiExportTimelineTests.cpp" compile="1" resource="0" file="Source/MidiExportTimelineTests.cpp"/>
```

Unlike Phase 5, the test project **does** get a link-error alarm here (`exTmlC`/`teTmlC` carry out-of-line definitions), so a forgotten registration fails loudly in both projects rather than relying on the checklist alone.

**Mandatory checklist after ANY file add/rename/delete** (unchanged from Phases 3-5):

1. Add/update `<FILE>` in `Berlin.jucer` (`Source/...`).
2. Add/update `<FILE>` in `Tests/BerlinTests.jucer` (`../Source/...`).
3. `Projucer.exe --resave Berlin.jucer`
4. `Projucer.exe --resave Tests/BerlinTests.jucer`
5. **Sync check**: `git diff` shows **no `<MODULE>` change in either file**; the `Source/export/*` sets differ only by the deliberate exclusion of `MidiFileWriter.h/.cpp` from the test project.
6. Build both projects.

## Testing Strategy

| Layer | What to Test | Approach |
|---|---|---|
| Compile-time | `MidiExportEvent` is an aggregate; `kTicksPerQuarterNote % 4 == 0` | `static_assert (std::is_aggregate_v<MidiExportEvent>)`; `static_assert (kTicksPerQuarterNote % 4 == 0)` |
| Unit — tick math | `ticksPerStep == 240` at `stepsPerBeat = 4`; boundary *k* lands on exactly `k * 240` for every *k* including the last step of the last repeat; `endTick == repeats * size * 240` | `MidiExportTimelineTests.cpp`, hand-built `Sequence` for exact assertions |
| Unit — note-off pairing | Every note-on has exactly one later note-off on the same pitch; the note-off tick equals the next step's tick; note-off precedes note-on at an equal tick; an inactive step produces no event and lets the previous note end at that step's tick | Walk `events` maintaining a sounding-note set; assert it is **empty** at the end |
| Unit — terminal note-off (highest risk) | Last step active → a note-off exists at `endTick`; last step inactive → no event at `endTick` and the set is still empty; single-step all-active sequence with `repeats = 1` → exactly one on/off pair | Dedicated test case per proposal risk row |
| Unit — repeats and seams | `repeats = 1, 2, 4`: event count and ticks equal *N* concatenated passes; the seam produces neither a double note-on nor a swallowed note; `size = 1` and `size = 16` both correct | Compare against a locally recomputed expectation, not a golden literal |
| Unit — degenerate inputs | `Sequence()` (size 0) → `ok`, no events, `endTick == 0`; all-inactive → `ok`, no events, `endTick == size * repeats * 240`; `repeats` of `0` and `-1` → `invalidRepeatCount` with `out.events` **empty**; `stepsPerBeat` of `0`, `-1`, `7` → `invalidStepsPerBeat` (7 does not divide 960 — **rejected, not rounded**) | Table-driven |
| Unit — ordering invariant | `events` tick sequence is non-decreasing; no note-on ever precedes a same-tick note-off | Single assertion loop reused across every scenario |
| Equivalence with live playback | For the same `Sequence`, the export event order/pitches match `SequencePlayer::process` + `flushPendingNoteOff` over one pass | Drive `SequencePlayer` at a sample rate where `samplesPerStep` is integral, collect `StepEvent`s, compare `(order, note, noteOn)` — the one automated defence against the "exported pattern differs from what you hear" risk |
| Regression | All existing suites green; `process()` behaviour byte-identical | `BerlinTests.exe --category=Berlin` exits 0 |
| Review-only | `Source/export/MidiExportTimeline.h/.cpp` contain **no** `juce` token and **no** JUCE include; `MidiFileWriter` contains no musical branch; `MidiFileWriter.h/.cpp` absent from `Tests/BerlinTests.jucer`; no `sort()`/`updateMatchedPairs()` call anywhere | Reviewer checklist + `rg -n 'juce' Source/export/MidiExportTimeline.*` returning nothing |
| Manual gate (the deliverable) | `Documents/Berlin/berlin-export.mid` opens in a DAW / MIDI reader **without an error or repair prompt**, showing: 120 BPM, 4/4, exactly **4 bars**, every note at velocity **100** on channel **1**, note lengths exactly one 16th, pitches matching the seeded pattern, and **no** note extending past the final bar | Screenshot or MIDI-reader event dump committed or attached to the verify report — an inspectable artifact, not "it looked right" |
| Manual gate — no stuck note | Every note-on in the dump has a matching note-off; the track's last note event is a note-off | Same dump |
| Manual gate — end-of-track | Hex dump of the track chunk tail contains **exactly one** `FF 2F 00` | `xxd`/hex viewer on the tail bytes |
| Manual gate — write failure | Point `resolveExportFile()` at an unwritable path (or make `Documents/Berlin` read-only): app **launches, plays, and exits cleanly**; the log line appears; no zero-byte or truncated `.mid` is left behind | Standalone launch; inspect the directory afterwards |
| Manual gate — atomic overwrite | Export twice with different `kExportRepeats`; the second file is a valid *N*-bar file with **no** trailing bytes from the first | DAW import of the second file (guards the `FileOutputStream`-appends trap) |
| Build | Both projects resave and compile; file-list sync; `<MODULES>` unchanged | `Projucer.exe --resave` x2 + `git diff` |

**Known coverage gap, stated rather than hidden** — same pattern and same accepted tradeoff as Phases 4 and 5. The `juce_core`-only harness reaches `buildMidiExportTimeline` **completely**, and reaches **none** of: `MidiFileWriter::buildFile` (needs `juce::MidiFile`), `writeTo`, `writeToFile`, the `TemporaryFile` atomic-overwrite path, the meta-event byte layout, the SMF header, `resolveExportFile()`, or the `MainComponent` trigger glue. That is the deliberate price of keeping `juce_audio_basics` out of the test project. It is mitigated three ways: all musical logic is *structurally* outside the uncovered code (Decision 5 removes even tempo arithmetic from it), the uncovered units are small enough to verify by eye, and every uncovered behaviour has a **falsifiable manual-gate row** above with a named inspectable artifact. Adding `juce_audio_basics` to `Tests/BerlinTests.jucer` to close this gap is out of scope and a review rejection.

## Threat Matrix

`references/threat-matrix.md` is **not applicable as a whole**: this change introduces no routing (request/command sense), no shell command, no subprocess, no VCS/PR automation, no executable-file classification, and no process-integration boundary. Row by row — documentation-like paths, Git repository selection, commit state, push state, PR commands: **`N/A` — this change contains no Git, shell, or file-classification surface.** The `Projucer.exe --resave` invocations are developer/CI build steps on trusted repo-local paths, not a runtime boundary of the shipped artifact.

This change *does* add Berlin's first **filesystem write**, so that boundary is characterized explicitly even though the matrix does not cover it:

| Concern | Applicable? | Behaviour and defence |
|---|---|---|
| Path injection / traversal | **N/A** | The path is fully program-derived (`getSpecialLocation` + two `getChildFile` literals). No user input, no config, no argv, no environment variable is interpreted as a path |
| Writing outside the intended directory | **N/A** | No user-supplied component exists to escape with; the target is a fixed leaf under `Documents/Berlin/` |
| Overwriting unrelated user data | **Applicable** | Fixed filename `berlin-export.mid` under an app-specific `Berlin/` subdirectory. It overwrites **only its own previous export**. RED test: manual-gate "atomic overwrite" row |
| Partial / corrupt output | **Applicable** | `juce::TemporaryFile` + `overwriteTargetFileWithTemporary()`; failure leaves the previous good file intact (Decision 4). RED tests: manual-gate "write failure" and "atomic overwrite" rows |
| Unwritable or absent destination | **Applicable** | Typed `pathUnavailable`/`writeFailed`, logged, startup continues. RED test: manual-gate "write failure" row |
| Executable-file classification | **N/A** | `.mid` is inert data; nothing is marked executable and nothing written is ever executed |
| Symlink / TOCTOU on the target | **Accepted, documented** | `overwriteTargetFileWithTemporary` is not a transactional rename against a hostile local attacker. Berlin is a single-user desktop app writing to the invoking user's own `Documents`; an attacker with write access there already has the user's privileges. Not mitigated further this phase |
| Audio-thread safety | **N/A this phase** | Export runs on the message thread before `setAudioChannels`; no audio thread exists yet and no state is shared. Phase 7's on-demand trigger **must re-establish** this (see Open Questions) |

## Migration / Rollout

No data migration, no feature flag, no persisted app state, no schema. `Source/export/` and `Tests/Source/MidiExportTimelineTests.cpp` are purely additive. The only non-additive edit is `Source/MainComponent.h/.cpp`, whose pre-change content is the archived Phase 5 state, so the reverted result is byte-verifiable. Rollback = revert the commits, restore both `.jucer` `<FILE>` lists, `--resave` both, rebuild; the app returns to generating and playing live MIDI with no export, and every existing suite stays green. Already-written `.mid` files are inert user data outside the repo — nothing else persists, so the revert is complete.

The export trigger and `resolveExportFile()` are **declared temporary in code comments** so Phase 7 replaces them rather than building a UI on top of them. `Source/export/`'s public interface is designed to survive that replacement unchanged: Phase 7 supplies a different `juce::File` and a different call site, nothing more.

## Open Questions

- [ ] **`MidiMessageSequence::addEvent` equal-timestamp ordering** (Decision 7). Believed to preserve insertion order; unverified against a local JUCE checkout. Verify at apply; if it reorders, emit note-offs at `tick - 1` and record the substitution in the verify report. This is the change's highest-severity unknown — a reordering here is a stuck note in the importing DAW.
- [ ] **Duplicate end-of-track.** An explicit `endOfTrack()` at `endTick` is added so the file's length is exactly *N* bars even when the final steps are inactive. JUCE's `writeTrack` is believed to skip its own end-of-track when the sequence already contains one; if the hex check finds two `FF 2F 00`, drop the explicit event and accept a track that ends at the last note-off instead.
- [ ] **`kExportRepeats = 4`.** The proposal fixes the constant's *existence*, not its value; 4 was chosen so the gate reads "exactly 4 bars". Confirm or override at apply time — it affects no tested contract.
- [ ] **`Documents/Berlin/` as the temporary location** (Decision 3). Portable and discoverable, but it does create a directory in the user's Documents on first launch. Confirm this is acceptable for a hook that Phase 7 removes, or substitute `tempDirectory` and accept a harder manual gate.
- [ ] **Phase 7 must re-establish the threading guarantee.** Export is currently thread-trivial only because it runs before the audio device opens. An on-demand Phase 7 trigger will read a live `Sequence` while the audio callback is running, which needs a snapshot or a lock — a real design question deferred, not solved here.
