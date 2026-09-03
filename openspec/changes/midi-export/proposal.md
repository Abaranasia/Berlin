# Proposal: MIDI Export

## Intent

Phase 5 made Berlin audible through an external synth, but everything it generates is **ephemeral**: close the app and the sequence is gone. There is no serialization code anywhere in the repo — a generated pattern cannot be kept, shared, or moved into a DAW.

Roadmap Phase 6 (proposal doc lines 955-984, 1491-1506) closes that with a single deliverable: "Exportable `.mid` files" containing tempo, time signature, notes, velocities, note durations, and MIDI channel. It is also the first artifact a user can hand to someone else, and the first output verifiable *outside* Berlin (any DAW or MIDI file reader).

## Scope

### In Scope

- `Source/export/MidiExportTimeline.h/.cpp` — **JUCE-free** transform: `Sequence` + tempo/steps-per-beat + repeat count → an ordered, tick-stamped note-event timeline. Exact integer tick math from a PPQ constant divisible by `stepsPerBeat`; no sample rate involved. Fully unit-tested.
- Note-off convention mirroring live playback **exactly**: each active step's note is held until the next step boundary — note-off immediately precedes the next step's note-on. The last sounding note of the final repeat gets its note-off at the end of the grid (live playback loops forever; a file must terminate cleanly).
- Configurable **repeat count** (≥ 1) baked into the exported file: total length = `repeats × sequence.size()` steps, contiguous, no gap or overlap at loop seams.
- `Source/export/MidiFileWriter.h/.cpp` — thin `juce::MidiFile` / `juce::MidiMessageSequence` wrapper: PPQ header, tempo meta-event from the transport BPM, hardcoded 4/4 time-signature meta-event, note-on/off on a fixed channel at fixed velocity, end-of-track, `writeTo(OutputStream&)`. Manual-gate-only.
- Degenerate inputs: empty sequence or all-inactive steps produce a **valid** `.mid` with tempo/time-signature meta-events and zero notes; invalid repeat count is rejected, never silently reinterpreted.
- `Tests/Source/` — suites for the tick-domain transform (tick math, note-off pairing, repeats, seams, degenerate inputs).
- `Berlin.jucer` + `Tests/BerlinTests.jucer` `<FILE>` registration + `Projucer.exe --resave`. Test harness module list stays `juce_core`-only.

### Out of Scope (explicit non-goals)

- **Per-step velocity or gate-length fields.** Export uses the existing fixed velocity constant (`kNoteVelocity = 100`) and the full-step-length convention. No `Step`/`Sequence` field is added — generated velocity is roadmap section 11, gate% is section 10.
- **Time signature as a domain concept.** 4/4 is hardcoded in the meta-event, matching `Transport`'s "fixed at construction, no runtime config" precedent.
- **Multiple tracks, multiple sequences, CC automation, mid-file tempo changes** — explicitly deferred by the roadmap itself.
- **Export UI**: no button, dialog, file browser, or remembered output directory — Phase 7.
- **MIDI file import/read**, project files, presets — Phases 11 / 25.
- **Reuse of `StepEventBuffer`/`StepEvent`/`SequencePlayer`** for export. Those are RT, sample-domain, fixed-64-capacity types; export is offline and tick-domain.
- Hand-rolled SMF byte encoding. `juce::MidiFile` is already linked via `juce_audio_basics`.
- CMake. Build stays Projucer.

## Capabilities

### New Capabilities

- `midi-export-timeline`: JUCE-free `Sequence` → tick-stamped note-event timeline. Tick derivation from BPM/PPQ/`stepsPerBeat`, note-off-before-next-note-on pairing, terminal note-off, repeat count semantics and loop seams, degenerate-input behaviour, monotonic event ordering.
- `midi-file-output`: `juce::MidiFile`-based writer. SMF header/PPQ, tempo meta-event, 4/4 time-signature meta-event, fixed channel and velocity, end-of-track, write-to-path success and failure behaviour, and the manual verification gate.

### Modified Capabilities

- `unit-test-harness`: the shared-source registration requirement currently names only `Source/core/*` and `Source/generation/*`, while the harness already compiles `Source/playback/*` and `Source/midi/*`. Generalize it to "all JUCE-free source tiers", including the new `Source/export/*`.

## Resolved Questions

Settled by exploration (`sdd/midi-export/explore`) plus an explicit user decision round — **not to be re-litigated**:

- **Velocity**: fixed existing default (`kNoteVelocity = 100`) for every exported note. No new `Step` field.
- **Duration/gate**: mirror live playback exactly — note-off immediately before the next step's note-on. No gate-length concept introduced.
- **Time signature**: hardcoded 4/4 meta-event. No domain-model field.
- **Export length**: configurable repeat count baked into the file, **not** a single pass.
- **Architecture**: JUCE-free tick-domain transform + thin `juce::MidiFile` writer (exploration approach 2), not the RT sample-domain pipeline.
- **Testing split**: the transform is unit-tested in the `juce_core`-only harness; the `juce::MidiFile` writer is manual-gate-only, the same accepted tradeoff and "Known Coverage Gap" spec section as Phases 4-5.

## Open Item for `design.md`

**Export trigger and output path.** No UI, no persistence, and no output-directory convention exist yet, and Phase 7 UI work must not be pulled forward. `design.md` MUST resolve this with a *minimal, non-UI* answer — a callable export API plus, at most, a temporary hardcoded trigger/path in `MainComponent` (mirroring Phase 5's auto-open-first-device precedent) — and must specify path-resolution and write-failure behaviour. This is a decision to make, not a question to leave open.

## Approach

1. **Tick domain, TDD first.** `MidiExportTimeline` lands red/green in the `juce_core`-only harness before any `juce::MidiFile` type is touched. All musical correctness lives here.
2. **Integer ticks, no sample rate.** PPQ is chosen divisible by `stepsPerBeat` so every step boundary is an exact integer tick — no seconds/samples round-trip, no accumulated rounding across repeats.
3. **Thin JUCE seam.** `MidiFileWriter` only translates an already-correct timeline into `juce::MidiMessageSequence` events and writes bytes. It contains no musical decisions, so its zero-coverage status costs little.
4. **New offline tier.** `Source/export/` keeps `Source/midi/` as the realtime dispatch tier; export shares no state with the audio thread and runs entirely on the message thread.
5. **Convention duplicated deliberately, documented explicitly.** The note-off rule now exists twice (live sample-domain, offline tick-domain). Both spec sections cross-reference each other so a future gate-length change updates both.
6. **Falsifiable manual gate.** The exported `.mid` is opened in a DAW / MIDI file reader and checked for BPM, 4/4, note pitches, channel, velocity, note lengths, and total bar count for the configured repeat count.

## Affected Areas

| Area | Impact | Description |
|------|--------|-------------|
| `Source/export/` | New | `MidiExportTimeline` (JUCE-free), `MidiFileWriter` (thin `juce::MidiFile`) |
| `Source/MainComponent.h/.cpp` | Modified | Minimal non-UI export trigger + output path (shape settled in `design.md`) |
| `Tests/Source/` | New | `MidiExportTimelineTests.cpp` |
| `Berlin.jucer` | Modified | New `<GROUP name="export">` `<FILE>` entries; module list **unchanged** (`juce_audio_basics` already linked) |
| `Tests/BerlinTests.jucer` | Modified | `<FILE>` entries only; module list stays `juce_core`-only |
| `openspec/specs/unit-test-harness/` | Modified | Shared-source requirement generalized |
| `Builds/`, `JuceLibraryCode/` | Regenerated | `Projucer.exe --resave` |
| `Source/core/`, `Source/generation/`, `Source/playback/`, `Source/midi/` | Untouched | Consumed read-only |

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| Exported timing drifts from live playback (a listener hears two different patterns) | Med | Single shared step-boundary rule; PPQ divisible by `stepsPerBeat` for exact integer ticks; tests assert boundary ticks across all repeats |
| Dangling note-on at end of file (stuck note in the importing DAW) | High | Terminal note-off is a spec requirement with a dedicated test; every note-on must be matched |
| Broken loop seam (double note-on, or a swallowed note between repeats) | Med | Repeat-seam scenarios are explicit spec requirements and unit tests |
| Zero coverage on the `juce::MidiFile` half | Med | All musical logic is pushed out of it; documented "Known Coverage Gap" plus a falsifiable DAW-import manual gate |
| Scope creep into velocity/gate fields, time-signature config, or Phase 7 export UI | High | Each is an enumerated non-goal mapped to a named later phase; the four product decisions are recorded as Resolved Questions |
| Pressure to add `juce_audio_basics` to the test harness "so export is testable" | Med | Settled; the test `.jucer` module list must be unchanged in the final diff |
| Write failure (bad path, read-only dir) crashes or silently reports success | Med | Explicit success/failure return and specified behaviour, decided in `design.md` |

## Rollback Plan

`git revert` the change commits: `Source/export/` and `Tests/Source/MidiExportTimelineTests.cpp` disappear wholesale; `Source/MainComponent.h/.cpp` return to the archived Phase 5 version (the export trigger is purely additive); `openspec/specs/unit-test-harness/spec.md` reverts to its pre-change text. Then restore the `<FILE>` lists in `Berlin.jucer` and `Tests/BerlinTests.jucer` and run `Projucer.exe --resave` on both. Post-rollback the app again generates and plays live MIDI with no export capability, and the existing suites stay green. Any already-exported `.mid` files are inert user data outside the repo — nothing else persists, so the revert is complete.

## Dependencies

- Exploration artifact: Engram `sdd/midi-export/explore`.
- `sequencing-core`, `playback-transport`, `midi-output-dispatch`, `step-event-scheduling` as merged through `2026-09-03-midi-output-routing`.
- `juce_audio_basics` — already in `Berlin.jucer`; ships `juce::MidiFile` + `juce::MidiMessageSequence`. No module addition.
- A DAW or MIDI file reader for the manual gate.
- Projucer available headlessly with a resolvable global JUCE modules path.

## Success Criteria

- [ ] A generated `Sequence` exports to a `.mid` file that a DAW opens without error or repair prompt.
- [ ] The file reports the transport BPM and 4/4 time signature.
- [ ] Every exported note carries the fixed velocity constant and the fixed MIDI channel; no `Step` velocity/gate field was added.
- [ ] Each active step's note lasts exactly one step and ends where the next step begins; the final sounding note has a matching note-off — no unmatched note-on exists in the file.
- [ ] With repeat count *N*, the file contains *N* contiguous passes of the pattern with correct seams and a total length of `N × sequence.size()` steps.
- [ ] Step boundaries land on exact integer ticks with no accumulated rounding at the last repeat.
- [ ] An empty or all-inactive sequence exports a valid, note-free `.mid`; an invalid repeat count is rejected rather than reinterpreted.
- [ ] Export is invoked through a JUCE-free-core API with no new UI component; `resized()` is unchanged.
- [ ] `Tests/BerlinTests.jucer` still links `juce_core` only; its module list is unchanged in the final diff.
- [ ] `BerlinTests.exe --category=Berlin` exits 0 with the previous suites plus `MidiExportTimelineTests`.
- [ ] Live playback behaviour is byte-for-byte unchanged: no file under `Source/playback/` or `Source/midi/` is modified.
