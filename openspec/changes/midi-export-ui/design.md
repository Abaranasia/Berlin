# Design: MIDI Export UI

## Technical Approach

`MainComponent` gains its first three UI members — a `juce::TextButton`, a `juce::Label` status line, and a `std::unique_ptr<juce::FileChooser>` — and its ctor's one-shot export block is replaced by an async, user-driven chain:

```
exportButton.onClick ──→ launchExportChooser()
                              │  creates member exportChooser, launchAsync(...)
                              ▼
                    [ OS native save dialog ]           user may cancel → no-op
                              │  chosen juce::File
                              ▼
                    exportSequenceTo (destination)      MESSAGE THREAD ONLY
                              │
       buildMidiExportTimeline (sequence, 4, 4, timeline)   ← Source/export/, unchanged
                              │
       MidiFileWriter (kMidiChannel, kNoteVelocity).writeToFile (timeline, kBpm, destination)
                              │  MidiFileWriteResult
                              ▼
                    statusLabel  (always)  +  NativeMessageBox (failure only)
```

No file under `Source/export/`, `Source/playback/`, `Source/midi/`, `Source/core/`, or `Source/generation/` is touched. No new source files are added, so `Berlin.jucer` needs **no** `<FILE>` entry and **no** Projucer `--resave` — this closes the proposal's "possibly regenerated" open item.

*Budget note: the SDD 800-word target is exceeded (~1250 words) for the same reason the archived `midi-export` design was — this document must set the codebase's first UI precedent and resolve five delegated open items. Tables are used wherever prose would do.*

## Verified Corrections to the Proposal

| Proposal said | Verified reality | Source |
|---|---|---|
| `saveMode \| canOverwriteExisting` | **`canOverwriteExisting` does not exist in JUCE.** Correct flags are `saveMode \| canSelectFiles \| warnAboutOverwriting` | `juce_FileBrowserComponent.h:61-76` (pinned JUCE 9.0.1) |
| `SequencePlayer` may *reference* `sequence` (Medium risk, deferred) | **It owns a private copy by value.** Risk is eliminated, not mitigated — see Decision 2 | `SequencePlayer.h:70`, `SequencePlayer.cpp:15-18` |

## Architecture Decisions

### Decision 1: the `FileChooser` is a member `unique_ptr`, and destroying it *provably* cancels the callback

**Choice**: `std::unique_ptr<juce::FileChooser> exportChooser;` as a private `MainComponent` member. `launchExportChooser()` assigns a fresh chooser and calls `launchAsync`; the completion lambda captures `this` by raw pointer.

```cpp
void MainComponent::launchExportChooser()
{
    exportChooser = std::make_unique<juce::FileChooser> ("Export MIDI...",
                                                          defaultExportFile(),
                                                          "*.mid");

    const int flags = juce::FileBrowserComponent::saveMode
                    | juce::FileBrowserComponent::canSelectFiles
                    | juce::FileBrowserComponent::warnAboutOverwriting;

    exportChooser->launchAsync (flags, [this] (const juce::FileChooser& chooser)
    {
        const juce::File destination = chooser.getResult();

        if (destination == juce::File())   // cancelled: silent no-op, NOT a failure
            return;

        exportSequenceTo (destination);
    });
}
```

**Why capturing `this` cannot dangle** — this is the load-bearing verification, not an assumption. `FileChooser::~FileChooser()` is exactly `{ asyncCallback = nullptr; }` (`juce_FileChooser.cpp:130-133`), and the header states *"To abort the file selection, simply delete the FileChooser object"* (`juce_FileChooser.h:210-211`). Because the chooser is a **member**, `~MainComponent` destroys it, which clears the pending callback — so the lambda can never fire against a destroyed `MainComponent`. Ownership and cancellation are the same mechanism.

**Belt-and-braces**: `~MainComponent()` calls `exportChooser.reset();` as its **first** statement, before `shutdownAudio()`. Members are destroyed after the destructor body, so the reset is not strictly required, but it makes the cancellation explicit at the site a reader looks for it rather than implicit in declaration order.

| Alternative | Rejected because |
|---|---|
| Local `FileChooser` on the stack | Destroyed at end of scope → callback silently never fires. The classic JUCE bug; the spec forbids it |
| `new FileChooser` deleted inside its own callback | Deletes the object whose callback is executing; leaks on app shutdown |
| `browseForFileToSave` (blocking) | Blocks the message thread; unusable on the iOS/Android targets `Main.cpp:` already guards |
| `SafePointer<MainComponent>` in the lambda | Redundant — member ownership already makes firing-after-death impossible |

### Decision 2: audio-thread safety is *structural* — export and playback read two different objects

**Resolution of the proposal's Medium risk, by reading the code**: `SequencePlayer` takes `Sequence sequenceToPlay` **by value** and `std::move`s it into `const Sequence sequence;`, a private member (`SequencePlayer.h:70`, `.cpp:15-18`). Its own header states the rationale: *"Owns both the Sequence and the Transport BY VALUE... one owner, one immutable snapshot, no aliasing."*

Therefore:

| Object | Owner | Read by | Mutated after construction |
|---|---|---|---|
| `MainComponent::sequence` | `MainComponent` (`const`) | **Message thread only** — export | Never (`const`) |
| `SequencePlayer::sequence` | `player` (`const`, private copy) | **Audio thread only** — `process()` | Never (`const`) |

These are **two distinct `const` objects**. Export reads the first; `getNextAudioBlock` reads the second. There is no shared mutable state, so the required synchronisation is **none** — no lock, no atomic, no copy-on-export. `getNextAudioBlock` is not modified by this change at all, so no allocation, lock, or exception path is introduced onto the real-time path (`juce-app-dev` hard rule satisfied by construction).

`buildMidiExportTimeline` does allocate (`std::vector::push_back`), but only on the message thread. The audio path allocates nothing — `StepEventBuffer` is fixed-capacity and `midiBlock.ensureSize` runs in `prepareToPlay` — so there is no allocator contention to stall the callback.

**Rejected**: disabling the export button during playback, and snapshotting `sequence` under a lock before export. Both pay a real cost to solve a problem that does not exist here.

### Decision 3: one private export method; it is knowingly untestable

**Choice**:

```cpp
// MainComponent.h, private:
void launchExportChooser();                          // button → dialog
void exportSequenceTo (const juce::File& destination);  // dialog → build + write + feedback
static juce::File defaultExportFile();               // dialog seed only
```

`exportSequenceTo` is the sole call site of the `Source/export/` tier. A file-local `describeWriteFailure (MidiFileWriteResult)` in `MainComponent.cpp`'s existing anonymous namespace maps the enum to user-facing text, matching the file's established constant-placement convention.

**Explicit, accepted tradeoff — not a gap to fix**: this method remains **outside all automated test reach**. `Tests/BerlinTests.jucer` is deliberately `juce_core`-only; `MainComponent` requires `juce_gui_basics`. Extraction improves structure and readability; it does **not** buy coverage. The exploration's claim that it "makes the write logic unit-testable" is wrong and is corrected here. Verification is code review plus the human manual gate — the identical documented tradeoff already recorded as `midi-file-output`'s *Known Coverage Gap*. **Do not expand the harness in this change.**

### Decision 4: two-tier feedback — status label always, native dialog on failure only

**Choice**: every outcome writes the `statusLabel`; only failures additionally raise an async native message box.

```cpp
juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                             "MIDI Export Failed", message, this);
```

Verified present in JUCE 9.0.1 at `juce_NativeMessageBox.h:130-134`, outside any `JUCE_MODAL_LOOPS_PERMITTED` guard. Passing **no** callback makes it fire-and-forget: it returns immediately, owns its own lifetime, and runs no modal loop.

| Outcome | Status label | Dialog | Log |
|---|---|---|---|
| Cancelled (no file chosen) | unchanged | none | none |
| `MidiExportStatus` ≠ `ok` | red — "Export failed: could not build timeline." | yes | yes |
| `MidiFileWriteResult::ok` | normal — `"Exported to " + destination.getFileName()` | **no** | no |
| `invalidTimeline` | red — "Export failed: the timeline was invalid." | yes | yes |
| `pathUnavailable` | red — "Export failed: destination folder unavailable." | yes | yes |
| `writeFailed` | red — "Export failed: could not write the file." | yes | yes |

**Rationale**: success is frequent and expected — a dialog on every export becomes a click-through reflex and trains users to dismiss errors too. Failure is rare and consequential, so it must interrupt. This satisfies both spec scenarios (*"Successful write is confirmed to the user"* non-blocking, and *"Failed write is reported, not silent"*). `juce::Logger::writeToLog` is **kept alongside** for diagnostics — it is no longer the only channel, which is what the proposal actually forbade.

**Rejected**: modal `AlertWindow` (blocks the message thread — explicitly forbidden); log-only (forbidden); a dialog on success too (nag).

### Decision 5: layout via `removeFromTop`, size set after children

```cpp
// MainComponent.cpp anonymous namespace
constexpr int kMargin = 12, kControlHeight = 28, kButtonWidth = 140;

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced (kMargin);
    exportButton.setBounds (area.removeFromTop (kControlHeight).removeFromLeft (kButtonWidth));
    area.removeFromTop (kMargin / 2);
    statusLabel .setBounds (area.removeFromTop (kControlHeight));
}
```

Ctor ordering is load-bearing and matches the JUCE template comment already in the file (*"set the size of the component after you add any child components"*):

```cpp
addAndMakeVisible (exportButton);
addAndMakeVisible (statusLabel);
exportButton.onClick = [this] { launchExportChooser(); };
setSize (800, 600);              // AFTER addAndMakeVisible — triggers the first resized()
```

`exportButton` is declared `juce::TextButton exportButton { "Export MIDI..." };` — in-class initialiser, so the ctor never restates the label.

**Rejected**: absolute pixel `setBounds` (does not survive resize), `FlexBox`/`Grid` (correct for the piano roll later; overhead for two controls now — the `removeFromTop` idiom composes into either).

### Decision 6: the ctor hook is deleted; `resolveExportFile` survives, renamed

| Site | Action |
|---|---|
| `MainComponent.cpp:55-73` — the whole one-shot block **including** the three `// ---- One-shot MIDI export (Decision 1)` / `// Temporary hook - deleted in Phase 7` comment lines | **Delete** |
| `MainComponent.cpp:35-40` `resolveExportFile()` | **Keep, rename to `defaultExportFile()`** |
| `MainComponent.h:36` `// TEMPORARY hook - deleted in Phase 7` | **Delete the comment**, replace with `// default destination seeded into the save dialog` |
| `kExportFileName`, `kExportRepeats`, `kBpm`, `kStepsPerBeat`, `kMidiChannel` | **Keep unchanged** — all still consumed |
| `#include "export/MidiExportTimeline.h"` / `MidiFileWriter.h` | **Keep** — still needed by `exportSequenceTo` |

**Rationale for keeping-and-renaming rather than deleting**: the helper still has a real job — seeding the save dialog with `Documents/Berlin/berlin-export.mid` so the user gets a sane default folder and filename instead of an arbitrary CWD. But `resolve` implied it produced *the* authoritative destination; the user's choice is now authoritative. The rename (two call sites) makes that semantic demotion visible in the diff and, together with the comment deletion, satisfies the success criterion *"No `TEMPORARY`/`Phase 7` hook comment remains."*

## Berlin UI Pattern v1 — precedent for the rest of roadmap Phase 7

Copy this shape for the piano roll, parameter controls, generate/randomize/mutate, and presets.

1. **Controls are owned by value** as `MainComponent` members, declared in the private section *after* the domain members. In-class initialisers carry static config (labels, ranges); the ctor carries only `addAndMakeVisible` and `onClick`/`onValueChange` wiring.
2. **`setSize()` is the last statement** of the constructor's UI section, after every `addAndMakeVisible`.
3. **Layout lives only in `resized()`**, expressed as `getLocalBounds().reduced (kMargin)` consumed by `removeFromTop`/`removeFromLeft`. Never absolute coordinates. Dimension constants go in `MainComponent.cpp`'s anonymous namespace.
4. **Callbacks are `[this]` lambdas** assigned to the widget's `std::function` member (`onClick`), never a `Component::Listener` subclass — `MainComponent` does not inherit UI listener interfaces.
5. **Anything asynchronous with a callback is owned by a member `std::unique_ptr`**, so that destroying `MainComponent` cancels it. Reset it at the top of `~MainComponent()`.
6. **Domain work happens in a named private method** taking plain domain/JUCE value types (`exportSequenceTo (const juce::File&)`), never inline in the lambda. The lambda only unpacks the UI result and delegates.
7. **Feedback is two-tier**: `statusLabel` for every outcome; `juce::NativeMessageBox::showMessageBoxAsync` with no callback for failures. **Never** a modal loop.
8. **The audio thread is never consulted.** UI reads `MainComponent`-owned `const` domain state. `getNextAudioBlock` gains nothing.

## `Source/export/` requires zero changes — exact API consumed

All three entry points are already called from `MainComponent.cpp` today with argument types identical to those `exportSequenceTo` will pass. Only the `juce::File` *value* changes, from `resolveExportFile()` to the chooser's result.

```cpp
// Source/export/MidiExportTimeline.h:60-63
MidiExportStatus berlin::buildMidiExportTimeline (const Sequence& sequence,
                                                  int stepsPerBeat, int repeats,
                                                  MidiExportTimeline& out);
// Source/export/MidiFileWriter.h:46
berlin::MidiFileWriter::MidiFileWriter (int outputChannel, int velocity) noexcept;
// Source/export/MidiFileWriter.h:56-58
MidiFileWriteResult berlin::MidiFileWriter::writeToFile (const MidiExportTimeline& timeline,
                                                          double bpm,
                                                          const juce::File& destination) const;
```

`writeToFile` already creates the parent directory, writes atomically through `juce::TemporaryFile`, and returns the 4-value result the UI maps — the UI adds no file logic. **`Source/export/*` must be byte-for-byte unchanged in the final diff.**

## File Changes

| File | Action | Description |
|---|---|---|
| `Source/MainComponent.h` | Modify | +3 members (`exportButton`, `statusLabel`, `exportChooser`), +2 private methods, `resolveExportFile` → `defaultExportFile`, drop TEMPORARY comment |
| `Source/MainComponent.cpp` | Modify | Delete ctor export block; add layout constants, ctor wiring, `resized()`, `launchExportChooser`, `exportSequenceTo`, `describeWriteFailure`, `~MainComponent` reset |
| `Source/export/*`, `playback/*`, `midi/*`, `core/*`, `generation/*` | Untouched | — |
| `Berlin.jucer`, `Builds/`, `JuceLibraryCode/` | Untouched | No files added → no `<FILE>` entry, no `--resave` |
| `Tests/BerlinTests.jucer`, `Tests/Source/*` | Untouched | Harness stays `juce_core`-only |

## Testing Strategy

| Layer | What | Approach |
|---|---|---|
| Unit | Nothing new | `MainComponent` is unreachable from the `juce_core`-only harness (Decision 3). Existing suites must stay green and unmodified |
| Regression | `BerlinTests.exe --category=Berlin` | Must still exit 0; the diff touches no tested translation unit |
| Review | Chooser member ownership; ctor block deleted; `resized()` bounds; failure→dialog mapping; `Source/export/` untouched | Code review against Decisions 1, 4, 6 and the success criteria |
| Manual gate (human) | Launch writes nothing → click → dialog seeded at `Documents/Berlin/berlin-export.mid` → cancel is silent → export to a real path → open in a DAW (120 BPM, 4/4, 4 bars) → export to a read-only folder shows a dialog → export during playback with no audible glitch | The only falsifiable coverage; same accepted gate as Phase 6 |

## Threat Matrix

`N/A` — no routing, shell command, subprocess, VCS/PR automation, executable-file classification, or process-integration boundary. The one adjacent boundary is the **user-chosen output path**: it is passed as a `juce::File` value directly to `MidiFileWriter::writeToFile` and is never interpolated into a command line or shell string. Path failure modes are already enumerated and typed by the unchanged writer (`pathUnavailable`, `writeFailed`) and made non-destructive by its `juce::TemporaryFile` atomicity — a failed write leaves any pre-existing file intact.

## Migration / Rollout

No migration, no feature flag, no persisted state, no schema. The change is confined to two files. Rollback is `git revert` of the change commits: `Source/MainComponent.h/.cpp` return to their Phase 6 state, the ctor one-shot export resumes, `resized()` is empty again. Because no source file is added or removed, no `.jucer` edit and no Projucer `--resave` are involved in either direction — the revert is purely source-level. Already-exported `.mid` files are inert user data outside the repo.

## Open Questions

None blocking. Resolved during design:

- Audio-thread aliasing (proposal Medium risk) — eliminated, Decision 2.
- Success feedback (proposal Q2) — confirmed visibly via the status label, Decision 4.
- `resolveExportFile()` fate (proposal Q6) — kept and renamed, Decision 6.
- `.jucer` regeneration — not required.

Deferred by design, consistent with the proposal's non-goals: last-used-directory persistence, export-option controls (repeat count, channel, velocity, tempo), and `statusLabel` colours sourced from a `LookAndFeel` colour ID rather than `juce::Colours` literals — revisit when Phase 7 introduces theming.
