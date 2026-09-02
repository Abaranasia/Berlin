# Design: MIDI Output Routing

## Technical Approach

A fourth source tier, `Source/midi/`, sitting **above** `Source/playback/` and holding every `juce_audio_devices`/`juce_audio_basics` type in the change:

- `Source/core/`, `Source/generation/`, `Source/playback/` — **untouched tiers**, except one purely additive method on `SequencePlayer`. Still JUCE-free / `juce_core`-only.
- `Source/midi/` — `MidiMessageBytes.h` (JUCE-**free**, `constexpr`, unit-tested), `MidiEventTranslator.h/.cpp` and `MidiOutputSink.h/.cpp` (JUCE-aware).
- `Source/MainComponent.h/.cpp` — three new members, two constants, four touched methods. **No new UI**: `paint`/`resized` must be absent from the final diff.

The dispatch chain is `StepEventBuffer → juce::MidiBuffer → juce::MidiOutput::sendBlockOfMessages`, with `millisecondCounterToStartAt` derived from a sample counter the sink owns. `Tests/BerlinTests.jucer` gains exactly one header (`MidiMessageBytes.h`) and two test `.cpp`s; its `<MODULES>` list stays `juce_core`-only — a diff touching `<MODULES>` is a defect.

## Ownership Ledger (spec/design reconciliation — read this first)

| Concern | Owner | Exact call site |
|---|---|---|
| Emitting the tracked note-off | `berlin::SequencePlayer::flushPendingNoteOff (StepEventBuffer&)` | Called by `MainComponent::releaseResources()` — **the only call site** |
| `stop()` / `reset()` emitting | **Nobody.** Signatures unchanged; they take no sink and cannot emit | n/a — see Decision 2 |
| CC 123 All Notes Off (panic guard) | `berlin::MidiOutputSink::closeDevice()` | Called by `MainComponent::~MainComponent()` and by `~MidiOutputSink()` (idempotent) |
| CC 123 byte packing | `berlin::makeAllNotesOff (int channel)` in `MidiMessageBytes.h` | Used by `MidiOutputSink::closeDevice()` |
| Device enumeration + open | `MidiOutputSink::openFirstAvailableDevice()` | Called from `MainComponent`'s ctor **body, before `setAudioChannels`** |
| Output channel constant `kMidiChannel` | `MainComponent.cpp` anonymous namespace — **one definition** | Injected into `MidiEventTranslator` and `MidiOutputSink` ctors |
| Velocity constant `kNoteVelocity = 100` | `MidiEventTranslator::kNoteVelocity` — **one definition** | Used only by `MidiEventTranslator::translate` |
| `millisecondCounterToStartAt` | `MidiOutputSink` (`startMs` + `elapsedSamples`) | `MidiOutputSink::dispatch` |
| `juce::MidiBuffer` storage + reservation | `MainComponent::midiBlock`, reserved in `prepareToPlay` | `MainComponent` |

**Spec wording note**: the `step-event-scheduling` requirement must be phrased as *"the stop/shutdown path emits a note-off"*, **not** *"`stop()` emits"*. `stop()` has no sink parameter and is not being given one.

## Architecture Decisions

### Decision 1: the JUCE-free packing helper is on the real path, not a parallel implementation

**Choice**: `MidiMessageBytes.h` exposes `constexpr` `makeNoteOn`/`makeNoteOff`/`makeAllNotesOff` returning three raw bytes. `MidiEventTranslator` and `MidiOutputSink` build `juce::MidiMessage (status, data1, data2)` **from those bytes**.
**Alternatives considered**: calling `juce::MidiMessage::noteOn/noteOff/controllerEvent` directly and testing the helper separately.
**Rationale**: the alternative makes the unit-tested helper dead code that proves nothing about shipped behaviour. Routing production through it turns the proposal's "packing helper is unit-testable" from a box-tick into real coverage of the one byte-level thing that can silently be wrong (off-by-one channel). Being `constexpr`, most assertions become compile-time `static_assert`s, mirroring Phase 4's compile-time layer.

### Decision 2: `flushPendingNoteOff` is the single emitting operation; `stop()`/`reset()` stay unchanged

**Choice**: `bool flushPendingNoteOff (StepEventBuffer& out) noexcept` — clears `out`, pushes one note-off at `sampleOffset 0` if `pendingNote >= 0`, sets `pendingNote = -1`, returns whether it emitted. Idempotent. Callers MUST call it **before** `reset()`.
**Alternatives considered**: `stop (StepEventBuffer&)` / `reset (StepEventBuffer&)` overloads; auto-flush inside `stop()` into a member-owned buffer.
**Rationale**: overloads leave the old no-sink signatures reachable — the wrong path stays compilable, which is exactly how the Phase 4 debt happened. A member-owned buffer gives `SequencePlayer` a second sink with no consumer. One explicit method has one contract, one test, and one call site. Ordering is load-bearing only against `reset()` (which clears `pendingNote`); `stop()` preserves it, so flush-after-stop also works.

### Decision 3: `releaseResources()` flushes but does **not** stop the player

**Choice**: `releaseResources()` = flush → translate → `sendImmediately`. It does not touch `player.stop()`.
**Alternatives considered**: `player.stop()` then flush.
**Rationale**: Phase 4's design fixed `releaseResources` as empty precisely so an audio-device restart resumes: running state is set once at construction and `prepare` preserves it. Calling `stop()` here would silently leave the transport stopped after any device change — a regression invisible in the test harness. After the flush, `pendingNote` is `-1`, so the restart cannot produce a ghost note-off.

### Decision 4: exact device-close sequence, and why `clearAllPendingMessages` comes first

**Choice**: `closeDevice()` runs, in order — (1) `device->clearAllPendingMessages()`, (2) `device->sendMessageNow (allNotesOff)`, (3) `device->stopBackgroundThread()`, (4) `device.reset()`. Idempotent when already closed.
**Alternatives considered**: CC 123 first, then stop the thread; or stop the thread first and rely on the tracked note-off alone.
**Rationale**: `sendMessageNow` bypasses the background queue. If future-scheduled note-ons are still queued, a CC 123 sent first is overtaken and the note hangs — the exact failure the guard exists to prevent. Discarding the queue first makes CC 123 provably last on the wire. Dropping a queued note-off with it is harmless, because CC 123 supersedes it.
**Verify at apply**: `juce::MidiOutput::clearAllPendingMessages()` could not be confirmed against a local JUCE checkout (modules resolve via the global path). If the method is absent in the pinned JUCE version, substitute `stopBackgroundThread()` before `sendMessageNow` and record the substitution.

### Decision 5: timestamp anchor is captured in `prepare()`, one read, never per block

**Choice**: `MidiOutputSink::prepare (double sampleRate)` sets `sampleRate`, zeroes `elapsedSamples`, and captures `startMs = juce::Time::getMillisecondCounterHiRes()` once. Per block: `startMs + elapsedSamples * 1000.0 / sampleRate`, then `elapsedSamples += numSamples` **unconditionally** (so a re-open cannot inherit a stale offset).
**Alternatives considered**: reading the counter per block (the drift `Transport` exists to eliminate); anchoring in the ctor on the message thread as the proposal's wording suggests.
**Rationale**: the anchor must be zeroed together with the sample counter, and `elapsedSamples` resets at `prepare`. JUCE calls `prepareToPlay` on the audio thread, so this is a **deviation from the proposal's literal "captured on the message thread"** — it is one bounded QPC read per device start, not per block, and it is the only way the anchor and the counter share an origin. Zero wall-clock reads exist in the per-block path.

### Decision 6: device pointer lifetime is enforced by call ordering, not a lock or an atomic

**Choice**: the device is opened in the ctor **before** `setAudioChannels`, and closed in the dtor **after** `shutdownAudio()`. `dispatch` early-returns on `device == nullptr`.
**Alternatives considered**: `std::atomic<bool>` open flag; a lock around the pointer.
**Rationale**: identical to Phase 4's "fully built before `setAudioChannels`" discipline — the audio thread never observes a transition, so no synchronisation is needed. The null check is not a race guard; it is the **zero-devices / failed-open** runtime state, which must degrade to silent-but-running.

## Interfaces / Contracts

```cpp
// Source/midi/MidiMessageBytes.h — JUCE-FREE, header-only, constexpr, unit-tested
namespace berlin {
struct MidiMessageBytes { unsigned char status = 0, data1 = 0, data2 = 0; };
static_assert (std::is_aggregate_v<MidiMessageBytes>);
bool operator== (const MidiMessageBytes&, const MidiMessageBytes&);   // non-member, keeps it an aggregate

inline constexpr int kAllNotesOffController = 123;

constexpr int clampMidiChannel (int channel) noexcept;   // -> [1, 16]
constexpr int clampMidiData    (int value)   noexcept;   // -> [0, 127]

constexpr MidiMessageBytes makeNoteOn      (int channel, int note, int velocity) noexcept; // 0x90|(ch-1)
constexpr MidiMessageBytes makeNoteOff     (int channel, int note) noexcept;               // 0x80|(ch-1), release vel 0
constexpr MidiMessageBytes makeAllNotesOff (int channel) noexcept;                          // 0xB0|(ch-1), 123, 0
}
```

Note-off is a real `0x80` status, **not** note-on with velocity 0 — the manual-gate monitor must be able to distinguish them.

```cpp
// Source/playback/SequencePlayer.h — ADDITIVE, still JUCE-free
bool flushPendingNoteOff (StepEventBuffer& out) noexcept;
// Clears `out` (same sink ownership convention as process()), then pushes at most one
// StepEvent { 0, pendingStep, pendingNote, false } if a note is sounding.
// Sets pendingNote = -1; leaves pendingStep untouched. Returns true iff it emitted.
// Idempotent: a second call emits nothing and returns false.
// NOT for the audio callback; call before reset(), which discards pendingNote.
```

```cpp
// Source/midi/MidiEventTranslator.h/.cpp — JUCE-aware, stateless apart from the channel
namespace berlin {
class MidiEventTranslator
{
public:
    static constexpr int kNoteVelocity = 100;              // Step has no velocity field until Phase 6

    explicit MidiEventTranslator (int outputChannel) noexcept;   // clamped to [1,16] at construction

    // AUDIO THREAD, RT-safe. destination.clear() then one 3-byte message per StepEvent at
    // timestamp = StepEvent::sampleOffset. Never calls ensureSize / never grows the buffer.
    void translate (const StepEventBuffer& events, juce::MidiBuffer& destination) const noexcept;

    int getChannel() const noexcept;
private:
    const int channel;
};
}
```

`StepEvent::sampleOffset` is block-relative and `juce::MidiBuffer` timestamps are block-relative sample positions, so they map **one-to-one**. `bufferToFill.startSample` is *not* added — Phase 4's warning applies to indexing the audio buffer, not to MIDI timestamps. Adding it here is the most likely apply-time bug.

```cpp
// Source/midi/MidiOutputSink.h/.cpp — owns the device and the timestamp origin
namespace berlin {
class MidiOutputSink
{
public:
    explicit MidiOutputSink (int outputChannel) noexcept;   // clamped to [1,16]
    ~MidiOutputSink();                                      // calls closeDevice()

    // ---- MESSAGE THREAD ONLY, audio callback provably stopped ----
    bool openFirstAvailableDevice();        // getAvailableDevices()[0].identifier -> openDevice
                                            // + startBackgroundThread(); false = none / open failed
    void closeDevice() noexcept;            // Decision 4 sequence; idempotent
    bool isDeviceOpen() const noexcept;
    juce::String getOpenDeviceName() const; // manual gate / Phase 7 readout; NEVER from the callback

    // ---- prepareToPlay (audio thread, per JUCE) ----
    void prepare (double sampleRateToUse) noexcept;   // sampleRate; elapsedSamples = 0; startMs anchor

    // ---- AUDIO THREAD ----
    void dispatch (const juce::MidiBuffer& buffer, int numSamples) noexcept;

    // ---- releaseResources / shutdown; NOT the audio callback ----
    void sendImmediately (const juce::MidiBuffer& buffer) noexcept;   // sendMessageNow per message
private:
    std::unique_ptr<juce::MidiOutput> device;
    const int channel;
    double    sampleRate     { 0.0 };
    double    startMs        { 0.0 };
    long long elapsedSamples { 0 };
};
}
```

`dispatch` body — this ordering is the contract:

```cpp
if (device != nullptr && sampleRate > 0.0 && ! buffer.isEmpty())
    device->sendBlockOfMessages (buffer,
                                 startMs + static_cast<double> (elapsedSamples) * 1000.0 / sampleRate,
                                 sampleRate);
elapsedSamples += numSamples;   // ALWAYS, even with no device
```

**RT-safety inventory for `translate` + `dispatch`** — safe: `juce::MidiBuffer::addEvent` into pre-reserved storage; `juce::MidiMessage`'s 3-byte constructor (≤ 4 bytes lives in the inline union, no heap); `double`/`long long` arithmetic. Forbidden and absent: `ensureSize` in the callback, `juce::String`, `juce::Logger`, `Time::getMillisecondCounter*`, `new`/`delete`, any file or enumeration call. Both methods are `noexcept`.

## Constitution Exception: one named, bounded lock on the audio thread

`realtime-audio-wiring` Requirement 3 forbids lock acquisition in `getNextAudioBlock`. This change takes **exactly one** documented exception.

| Field | Value |
|---|---|
| **Where** | `juce::MidiOutput::sendBlockOfMessages`, called from `MidiOutputSink::dispatch` |
| **What** | A short internal `juce::CriticalSection` taken to append to `MidiOutput`'s pending-message list |
| **Bound** | List append only — no I/O, no allocation of unbounded size, no driver syscall on the calling thread |
| **Contention** | Single other acquirer: `MidiOutput`'s own background thread, which holds it only to pop due messages |
| **Frequency** | Guarded by `! buffer.isEmpty()`. At 120 BPM / 16ths / 44.1 kHz one boundary occurs every 5512.5 samples, so with a 512-sample block **roughly 1 block in 10 takes the lock at all** |
| **Rejected: `sendMessageNow` per event** | Blocking OS-driver syscall (winmm/CoreMIDI/ALSA) on the audio thread with undocumented worst-case latency, and it discards `StepEvent::sampleOffset` entirely — strictly worse on both axes |
| **Rejected: hand-rolled SPSC queue + timer** | Reimplements shipped, tested JUCE behaviour for no precision gain, and introduces a second concurrency surface this phase has no budget to verify |
| **Blast radius if wrong** | Priority inversion causing an audio dropout. Berlin outputs silence this phase, so a dropout is inaudible now; it becomes real in Phase 8 and must be re-evaluated then |
| **Status** | Accepted, recorded here, and to be amended into the `realtime-audio-wiring` spec as a named exception — not waived silently |

`realtime-audio-wiring`'s "Output Stays Silent" requirement is simultaneously narrowed to **audio** output: `clearActiveBufferRegion()` still runs first and no oscillator/voice is introduced, but the callback now legitimately emits MIDI.

## Data Flow

```
[MESSAGE THREAD — MainComponent ctor]
  init list: player(...), midiTranslator(kMidiChannel), midiSink(kMidiChannel)
  body:      setSize -> player.start()
             -> midiSink.openFirstAvailableDevice()    <-- enumerate + open + startBackgroundThread
                  |  none available / open failed -> stays closed, app runs silent (no crash, no assert)
             -> setAudioChannels(2, 2)                 <-- audio starts HERE, device already resolved
--------------------------------------------------------------------------------
[AUDIO THREAD]
  prepareToPlay(sr) -> player.prepare(sr)
                    -> midiSink.prepare(sr)            <-- elapsedSamples = 0, startMs anchored ONCE
                    -> midiBlock.ensureSize(kMidiBufferBytes)   <-- the ONLY reservation

  getNextAudioBlock(n)
    |- clearActiveBufferRegion()                              (audio still silent; Phase 8)
    |- player.process(n, blockEvents)                         (JUCE-free, unchanged)
    |- midiTranslator.translate(blockEvents, midiBlock)       (clear + addEvent, no growth)
    +- midiSink.dispatch(midiBlock, n)
         |- sendBlockOfMessages(buf, startMs + elapsed*1000/sr, sr)   [THE named lock]
         +- elapsedSamples += n
                       v
              MidiOutput background thread -> OS driver -> external / virtual port
--------------------------------------------------------------------------------
[SHUTDOWN / DEVICE RESTART]
  releaseResources()  : player.flushPendingNoteOff(blockEvents)   <-- tracked note-off, offset 0
                        -> translate -> midiSink.sendImmediately(midiBlock)
                        (player is NOT stopped -> a device restart still resumes)
  ~MainComponent()    : shutdownAudio()  then  midiSink.closeDevice()
                        -> clearAllPendingMessages -> CC123 -> stopBackgroundThread -> reset
```

Canonical **stop → flush → all-notes-off → swap → restart** discipline: `releaseResources` is *flush*, `closeDevice` is *all-notes-off*, `openFirstAvailableDevice` is *restart*. *Swap* has no trigger this phase (no UI); the sequence is implemented so Phase 7 inherits it rather than inventing it.

## File Changes

| File | Action | Description |
|---|---|---|
| `Source/midi/MidiMessageBytes.h` | Create | JUCE-free `constexpr` packing: note-on/off, CC 123, channel/data clamps |
| `Source/midi/MidiEventTranslator.h` / `.cpp` | Create | `StepEventBuffer` → `juce::MidiBuffer`, fixed velocity, no buffer growth |
| `Source/midi/MidiOutputSink.h` / `.cpp` | Create | Device open/close, background thread, elapsed-sample timestamping, CC 123 guard |
| `Source/playback/SequencePlayer.h` / `.cpp` | Modify | **Additive only**: `flushPendingNoteOff`. `stop()`/`reset()`/`process()` unchanged |
| `Source/MainComponent.h` | Modify | 2 includes (`midi/MidiEventTranslator.h`, `midi/MidiOutputSink.h`), 3 members |
| `Source/MainComponent.cpp` | Modify | `kMidiChannel`/`kMidiBufferBytes` constants; ctor init list + body; `prepareToPlay`; `getNextAudioBlock`; `releaseResources`; dtor |
| `Tests/Source/MidiMessageBytesTests.cpp` | Create | Status bytes, channel 1-16 mapping, clamping, CC 123, compile-time asserts |
| `Tests/Source/SequencePlayerStopTests.cpp` | Create | Flush emits/omits, idempotency, ordering vs `reset()`, no unmatched note-on across cycles |
| `Berlin.jucer` | Modify | New `<GROUP name="midi">` `<FILE>` entries. **`<MODULES>` unchanged** |
| `Tests/BerlinTests.jucer` | Modify | `MidiMessageBytes.h` + 2 test `<FILE>`s. **`<MODULES>` stays `juce_core`-only** |
| `Builds/`, `JuceLibraryCode/`, `Tests/Builds/`, `Tests/JuceLibraryCode/` | Regenerate | `Projucer.exe --resave` ×2 |
| `Source/core/`, `Source/generation/`, `Transport.h/.cpp`, `StepEvent.h`, `StepEventBuffer.h`, `Source/Main.cpp` | Untouched | Must be absent from the final diff |
| `MainComponent::paint` / `resized` | Untouched | No UI this phase — must be absent from the final diff |

## `MainComponent` Modification Plan (exact)

`MainComponent.h` — additions only:

```cpp
#include "midi/MidiEventTranslator.h"
#include "midi/MidiOutputSink.h"
...
private:
    static berlin::Sequence buildSeededSequence();
    berlin::SequencePlayer      player;            // unchanged
    berlin::StepEventBuffer     blockEvents;       // unchanged
    berlin::MidiEventTranslator midiTranslator;    // ctor init list, kMidiChannel
    berlin::MidiOutputSink      midiSink;          // ctor init list, kMidiChannel
    juce::MidiBuffer            midiBlock;         // audio-thread scratch, reserved in prepareToPlay
```

`MainComponent.cpp` — anonymous namespace gains `constexpr int kMidiChannel = 1;` and `constexpr int kMidiBufferBytes = 1024;` (64-event `StepEventBuffer` capacity × ~12 B/event ≈ 768 B, plus headroom).

| Method | Change |
|---|---|
| ctor **init list** | append `, midiTranslator (kMidiChannel), midiSink (kMidiChannel)` |
| ctor **body** | after `player.start();` add `midiSink.openFirstAvailableDevice();` (return deliberately ignored — false is the valid silent state). Still **before** the `RuntimePermissions`/`setAudioChannels` block. |
| `prepareToPlay` | add `midiSink.prepare (sampleRate);` and `midiBlock.ensureSize (kMidiBufferBytes);` after `player.prepare (sampleRate);` |
| `getNextAudioBlock` | after `player.process(...)`: `midiTranslator.translate (blockEvents, midiBlock); midiSink.dispatch (midiBlock, bufferToFill.numSamples);`. `clearActiveBufferRegion()` stays **first**. |
| `releaseResources` | was empty; becomes `if (player.flushPendingNoteOff (blockEvents)) { midiTranslator.translate (blockEvents, midiBlock); midiSink.sendImmediately (midiBlock); }`. **No `player.stop()`** (Decision 3). |
| dtor | `shutdownAudio();` stays first; add `midiSink.closeDevice();` **after** it |
| `paint`, `resized`, `buildSeededSequence` | **unchanged** |

## Projucer Registration Mechanics

New group in both projects, following the existing `core`/`generation`/`playback` convention (brace-GUID group `id`, unique short file `id`s, headers `compile="0"`, sources `compile="1"`, `../` prefix on the test side):

```xml
<!-- Berlin.jucer, inside MAINGROUP, after the "playback" group -->
<GROUP id="{3F7A9C2D-6E4B-481A-B5C3-7D9E1F0A4B26}" name="midi">
  <FILE id="mdMsgH" name="MidiMessageBytes.h"    compile="0" resource="0" file="Source/midi/MidiMessageBytes.h"/>
  <FILE id="mdTrnH" name="MidiEventTranslator.h" compile="0" resource="0" file="Source/midi/MidiEventTranslator.h"/>
  <FILE id="mdTrnC" name="MidiEventTranslator.cpp" compile="1" resource="0" file="Source/midi/MidiEventTranslator.cpp"/>
  <FILE id="mdSnkH" name="MidiOutputSink.h"      compile="0" resource="0" file="Source/midi/MidiOutputSink.h"/>
  <FILE id="mdSnkC" name="MidiOutputSink.cpp"    compile="1" resource="0" file="Source/midi/MidiOutputSink.cpp"/>
</GROUP>

<!-- Tests/BerlinTests.jucer — ONLY the JUCE-free header, plus the two suites -->
<GROUP id="{3F7A9C2D-6E4B-481A-B5C3-7D9E1F0A4B26}" name="midi">
  <FILE id="tmMsgH" name="MidiMessageBytes.h" compile="0" resource="0" file="../Source/midi/MidiMessageBytes.h"/>
</GROUP>
<!-- and in the test project's own Source group: -->
<FILE id="btMmbC" name="MidiMessageBytesTests.cpp"   compile="1" resource="0" file="Source/MidiMessageBytesTests.cpp"/>
<FILE id="btSpsC" name="SequencePlayerStopTests.cpp" compile="1" resource="0" file="Source/SequencePlayerStopTests.cpp"/>
```

`MidiEventTranslator.cpp` / `MidiOutputSink.cpp` are the link-error alarm for the app project. The **test** project gets no such alarm (its only `midi` entry is a header resolved via the existing `headerPath="../../../Source"`), so checklist step 5 is the sole guard there — treat a missing `tmMsgH` entry as a defect, not a cosmetic omission.

**Mandatory checklist after ANY file add/rename/delete** (unchanged from `core-sequencing-model` / `playback-transport-clock`):

1. Add/update `<FILE>` in `Berlin.jucer` (`Source/...`).
2. Add/update `<FILE>` in `Tests/BerlinTests.jucer` (`../Source/...`).
3. `Projucer.exe --resave Berlin.jucer`
4. `Projucer.exe --resave Tests/BerlinTests.jucer`
5. **Sync check**: `git diff` shows **no `<MODULE>` change in either file**; the `Source/midi/*` sets differ only by the deliberate JUCE-aware exclusions above.
6. Build both projects.

## Testing Strategy

| Layer | What to Test | Approach |
|---|---|---|
| Compile-time | `MidiMessageBytes` is an aggregate; packing is `constexpr`; `flushPendingNoteOff` is `noexcept` | `static_assert (std::is_aggregate_v<MidiMessageBytes>)`; `static_assert (makeNoteOn (1, 60, 100) == MidiMessageBytes { 0x90, 60, 100 })`; `static_assert (noexcept (player.flushPendingNoteOff (buf)))` |
| Unit — packing | Note-on `0x90\|(ch-1)`, note-off `0x80\|(ch-1)` release vel 0, CC 123 `0xB0\|(ch-1),123,0`; channel 1 and 16 endpoints; channel 0/17/-1 clamp; note/velocity 128 and -1 clamp | `MidiMessageBytesTests.cpp`, table-driven |
| Unit — flush | Emits `{0, pendingStep, pendingNote, false}` when a note sounds; emits nothing when none does; clears `out` first; idempotent on a second call; `reset()` before flush emits nothing (documented ordering); no unmatched note-on across repeated start/process/flush/stop cycles including stop mid-step | `SequencePlayerStopTests.cpp`, hand-built `Sequence` for exact assertions |
| Regression | Existing 3 playback suites still green; `process()` behaviour byte-identical | `BerlinTests.exe --category=Berlin` exits 0 |
| Review-only | No allocation/`String`/`Logger`/wall-clock read in `getNextAudioBlock`; exactly **one** lock, the named `sendBlockOfMessages` acquisition; `ensureSize` appears only in `prepareToPlay`; `startSample` is **not** added to MIDI timestamps | Reviewer checklist against the constitution rule — C++ offers no runtime assertion here |
| Manual gate (the deliverable) | Virtual MIDI port (loopMIDI or equivalent) + monitor capture showing: note-on/note-off **pairs**, correct channel, correct fixed velocity 100, inter-note interval matching 120 BPM / 16ths (125 ms), across ≥ one full 16-step loop wrap | Captured monitor log committed or attached to the verify report — an artifact a reviewer inspects, not "I heard it" |
| Manual gate — panic | Quit mid-note: monitor shows the tracked note-off **and** a following CC 123 on the channel | Same capture |
| Manual gate — degraded | Zero MIDI devices present (or `openDevice` forced to fail): app launches, runs, exits cleanly — no crash, assert, or hang | Standalone launch with the virtual port disabled |
| Build | Both projects resave and compile; file-list sync; `<MODULES>` unchanged | `Projucer.exe --resave` ×2 + `git diff` |

**Known coverage gap, stated rather than hidden** — same pattern and same accepted tradeoff as Phase 4's `realtime-audio-wiring` gap. The `juce_core`-only harness can reach **only** `flushPendingNoteOff` and `MidiMessageBytes`. It cannot reach: `MidiEventTranslator::translate` (needs `juce::MidiBuffer`), any of `MidiOutputSink` (needs `juce_audio_devices`), the background thread, the CC 123 close sequence, the auto-open logic, the elapsed-sample timestamp arithmetic, or the `MainComponent` glue. That is the deliberate price of keeping `juce_audio_devices`/`juce_audio_basics` out of the test project, and it is why every one of those units is kept small enough for a reviewer to verify by eye and why the manual gate is defined as a **captured artifact** rather than a subjective listen. Adding either module to `Tests/BerlinTests.jucer` to close this gap is out of scope and a review rejection.

## Threat Matrix

N/A — no routing (in the request/command sense), shell command, subprocess, VCS/PR automation, executable-file classification, or process-integration boundary. Every row of `references/threat-matrix.md` (documentation-like paths, Git repository selection, commit state, push state, PR commands) is `N/A: this change contains no Git, shell, or file-classification surface`. `Source/midi/` performs in-process calls into JUCE's MIDI device API; no user-controlled string is interpreted as a path or command, and the `Projucer.exe --resave` invocations are developer/CI build steps on trusted repo-local paths, not a runtime boundary of the shipped artifact.

The real safety boundaries of this change are (a) the **audio thread**, covered by the RT-safety inventory, `noexcept` contracts, pre-reserved `MidiBuffer`, and the named lock exception above; and (b) the **external device lifetime**, covered by Decisions 4 and 6 plus the degraded-mode manual gate.

## Migration / Rollout

No data migration, no feature flag, no persisted state, no file format. Device and channel are deliberately **not saved** between launches — there is no UI to change them. `Source/midi/` and both test suites are purely additive; `SequencePlayer` gains one method and loses nothing. The only non-additive edit is `Source/MainComponent.h/.cpp`, whose pre-change content is the archived Phase 4 state, so the reverted result is byte-verifiable. Rollback = revert the commits, restore both `.jucer` file lists, `--resave` both, rebuild; the app returns to running the transport silently with an unconsumed `StepEventBuffer` and the existing suites stay green.

## Open Questions

- [ ] **`clearAllPendingMessages()` availability** (Decision 4). Could not be confirmed against a local JUCE checkout. Verify at apply time; if absent in the pinned version, reorder to `stopBackgroundThread()` → `sendMessageNow(CC123)` and record the substitution in the verify report.
- [ ] **Audio-clock vs system-clock divergence.** `millisecondCounterToStartAt` extrapolates the *system* clock from an *audio* sample count. Over hours the two drift, so scheduled timestamps slowly lead or lag real dispatch time, changing effective latency (not inter-note spacing). A one-loop-wrap manual gate will not catch it. Accepted for this phase; re-anchoring strategy deferred. Confirm this is acceptable rather than an omission.
- [ ] **`kMidiChannel = 1`.** The proposal fixes the constant's *existence*, not its value. Design proposes `1`. Affects no tested contract; confirm or override at apply time.
- [ ] **`kMidiBufferBytes = 1024`.** Sized against `StepEventBuffer::capacity = 64` and JUCE's per-event overhead, which is not a documented constant. If `juce::MidiBuffer` ever grows past the reservation the allocation is silent — unlike `StepEventBuffer::hasOverflowed`. Consider an apply-time `jassert (midiBlock.data.size() <= kMidiBufferBytes)` in a debug-only path, or accept it as review-only.
- [ ] **Phase 8 re-evaluation of the lock exception.** Once Berlin produces audio, a priority inversion becomes audible. The exception should be re-reviewed then, not inherited silently.
