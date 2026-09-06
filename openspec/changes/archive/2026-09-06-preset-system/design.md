# Design: Preset System (roadmap Phase 11)

Artifact store: hybrid. Engram: `sdd/preset-system/design`. Every line/behaviour cited below was read from the working tree or from JUCE's own source at `H:/Proyectos/Juce/JUCE/modules`, not recalled.

## Verified findings that shaped this design

| Proposal / exploration claim | Verified reality | Source |
|---|---|---|
| "JUCE's `var`/JSON path has no `int64` fidelity guarantee" | **Wrong as stated.** `parseNumber` accumulates into an `int64` and returns `var (correctedValue)` whenever `(intValue >> 31) != 0` — int64 fidelity *is* preserved for in-range integers. | `juce_JSON.cpp:252-287` |
| (not previously claimed) JSON has a **worse**, concrete defect | **Confirmed.** `intValue = intValue * 10 + digit` has **no overflow guard**. `Random::nextInt64()` composes two `uint32` halves, so the seed domain is the *full* int64 range **including `INT64_MIN`**; its decimal text (`-9223372036854775808`) overflows the signed accumulator → UB at parse time. | `juce_JSON.cpp:252,263`; `juce_Random.cpp:131-133` |
| The String/XML path is safe for the same input | **Confirmed.** `CharacterFunctions::getIntValue` accumulates into `UIntType` (unsigned → defined modular arithmetic), negating only at the end. No signed overflow on the accumulation. | `juce_CharacterFunctions.h:429-451` |
| ValueTree→XML round-trips typed values | **False, and it bites twice.** (a) `copyToXmlAttributes` writes `i.value.toString()`; `String(double)` calls `createFromDouble(n, 0, false)` and `writeDouble` sets `o.precision()` **only when `numDecPlaces > 0`** — so a double var stringifies at the default ostream precision of **6 significant digits**. `resonance = 0.7071068f` would persist as `0.707107`. (b) `setFromXmlAttributes` reads every attribute back as `var (String)`, so a loaded tree holds *only* String vars and is never `isEquivalentTo` a freshly built typed tree. | `juce_NamedValueSet.cpp:235-275`; `juce_String.cpp:481-505,516-536` |
| Right column has ~78px slack; a full-width row costs 34px | **Confirmed by re-derivation.** Usable `600 - 2×12 = 576`. Header = 6 rows × `(28 + 6)` = **204**. Columns get **372**. Right = ENVELOPE `28 + 4×34` + LFO `28 + 3×34` = **294** → **78px slack**. Left = **192** → 180px slack. One shared row costs `28 + 6 = 34` → **44px residual**. | `MainComponent.cpp:20,316-376` |
| A section-label row is the house pattern | **True, and it does not fit.** Label row + control row = **68px**, leaving **10px**. Decision 6 compresses it deliberately. | `MainComponent.cpp:332-344` |
| `pushAllParametersToSynth()` is the reusable load half | **Confirmed, and stronger than stated** — it reads the 11 *widgets*, so restoring widgets first makes it the entire synth-side load path, unchanged. | `MainComponent.cpp:406-419` |
| Async modal precedent exists | **Confirmed** — `NativeMessageBox::showMessageBoxAsync`, plus the re-entrancy guard `exportButton.setEnabled(false)` around `launchAsync`. | `MainComponent.cpp:424,457,471` |
| Setters already clamp | **Confirmed** — ranges are pinned and "setters clamp on the message thread to these bounds"; `clampParameter` already exists. | `SynthPatch.h:33-49` |

## Architecture Decisions

### Decision 1 — Format: one versioned `juce::ValueTree`, serialized to XML text, with **every property stored as an explicitly formatted `juce::String`**

**Choice**: `ValueTree` → `toXmlString()` → `*.xml`. Nothing is ever stored as a `double`/`int64` var. Floats are written with `juce::String (value, 9)`; the seed with `juce::String (juce::int64)`; enums as names (Decision 3). Reads go through `.toString().getDoubleValue()` / `.getLargeIntValue()`.

**Alternatives**: raw JSON via `juce::JSON` (rejected — `INT64_MIN` seed is signed-overflow UB at parse, see findings; plus no versioning idiom, and it contradicts the `juce-app-dev` decision gate); `ValueTree::writeToStream` binary (rejected — satisfies the skill but violates proposal §25's "presets should be human-readable", which is the whole reason this conflict exists); typed vars in the tree (rejected — 6-significant-digit stringification silently mangles `resonance`, and the asymmetric String-var readback makes tree equality untestable).

**Rationale**: this is the resolution of the `SKILL.md:19` vs `berlin_school_generative_sequencer_proposal.md:1000-1002` conflict, and it is not a compromise — a ValueTree *serialized as XML* satisfies **both** constraints literally: one versioned ValueTree, and human-readable text. The explicit-String rule is what makes it correct rather than merely plausible: it removes JUCE's lossy `var::toString()` from the path entirely and makes the on-disk types and the in-memory types identical, so a round-trip is exactly comparable. 9 decimal places against a max magnitude of `kMaxCutoffHz = 20000` yields ≥14 significant digits — far past the 9 a `float` needs to round-trip exactly.

### Decision 2 — Storage: one file per preset in `userApplicationDataDirectory/Berlin/Presets/*.xml`

**Choice**: a directory of files, enumerated with `findChildFiles (File::findFiles, false, "*.xml")`. The display name lives **inside** the file as a property; the filename is a sanitized derivative.

**Alternatives**: a single file holding a list (rejected — corruption blast radius is the whole bank, every save is a load-modify-write of everything, and it buys only the avoidance of filename sanitization); `juce::PropertiesFile` (rejected — key/value flat, no natural place for the two-section tree of Decision 4); a `FileChooser` per save/load reusing `launchExportChooser`'s precedent (rejected — that precedent is right for *exports*, which are user artifacts deliberately placed; presets are named recall and must not cost an OS dialog per action).

**Rationale**: enumeration is a directory listing, blast radius is one preset, and — decisively — the overwrite check collapses to `targetFile.existsAsFile()`, one deterministic predicate with no in-memory index to keep in sync. `userApplicationDataDirectory` over the export precedent's `userDocumentsDirectory` because the skill classifies presets as *persisted app state*. Rejected alternative recorded: `Documents/Berlin/Presets` would be more discoverable for hand-editing, which the human-readable goal mildly favours.

**Consequences**: (a) sanitization is lossy, so two display names can collide onto one file — acceptable, because the file-existence check surfaces it as the *same* overwrite prompt, deterministically; (b) `listPresetNames()` parses each file to read its `name`, so every ComboBox entry is guaranteed loadable and unparseable files are simply absent from the list; (c) the directory must be `createDirectory()`-guarded, mirroring `MidiFileWriteResult::pathUnavailable`.

### Decision 3 — Schema: `schemaVersion` on the root; two named sections; enums as names

```xml
<BerlinPreset schemaVersion="1" name="Acid Bass">
  <Synth waveform="pulse" cutoffHz="850.000000000" resonance="4.500000000"
         pulseWidth="0.500000000" attack="0.010000000" decay="0.150000000"
         sustain="0.700000000" release="0.200000000"
         lfoRateHz="4.000000000" lfoDepth="0.000000000" lfoDestination="cutoff"/>
  <Generation seed="-9223372036854775808"/>
</BerlinPreset>
```

| Aspect | Decision |
|---|---|
| Version field | `schemaVersion` attribute on the **root**, `PresetManager::kSchemaVersion = 1` |
| Missing / unparseable version | Reject (`parseFailed`) |
| `schemaVersion > kSchemaVersion` | **Reject** (`unsupportedVersion`) — unknown future field semantics cannot be guessed. Testable now |
| `schemaVersion < kSchemaVersion` | **Accept and migrate forward**, absent fields filled from `kDefaultPatch`. Unreachable at v1; declared now as policy, no code this phase |
| Sectioning | `<Synth>` and `<Generation>` children, not a flat blob — directly mitigates the proposal's "coupling two subsystems into one format" risk |
| Enums | Lowercase **names** (`saw`/`square`/`pulse`/`triangle`, `pitch`/`cutoff`/`amplitude`/`pulseWidth`) via a table in `PresetManager` |

**Enum alternative rejected**: the raw `static_cast<int>` index used for ComboBox IDs. Rejected because it is opaque in a format whose entire justification is human readability, and because `Waveform`'s declaration order is *already* load-bearing for ComboBox IDs — an index on disk would be a second, silent dependency on that order, reinterpreting every stored preset if it ever changed.

### Decision 4 — Malformed input: reject structurally, clamp continuously

| Failure class | Policy | Rationale |
|---|---|---|
| Not XML / wrong root tag / missing `<Synth>` or `<Generation>` / missing required attribute / bad version | **Reject whole preset.** Nothing applied, synth untouched, `statusLabel` reports why | Partial application leaves the instrument in a state with no name |
| Continuous value out of documented range (`cutoffHz=99999`) | **Clamp** to `kMin*`/`kMax*` via the existing `clampParameter`, then load | The 11 setters *already* clamp (`SynthPatch.h:33-35`), so an out-of-range value can never reach the voice regardless; rejecting would be strictly *stricter than the live UI*, where a slider physically cannot leave its range |
| Enum name not in the table | **Reject whole preset** | Unlike a continuous parameter there is no defensible "nearest valid" answer — silently substituting `saw` would misrepresent the saved sound without telling anyone. Forward-compatibility for genuinely new enum members is handled by Decision 3's version gate, not here |

### Decision 5 — `PresetManager` owns all I/O; `MainComponent` reuses `pushAllParametersToSynth()` verbatim; `regenerate()` is untouched

**Choice**: new `Source/preset/` layer. `Preset` is JUCE-aware (it holds a `juce::String` name and a `juce::int64` seed); `SynthPatch.h` is **reused unmodified** and stays JUCE-free per its documented convention exception. The result-enum + out-param shape mirrors `MidiFileWriter`/`buildMidiExportTimeline` exactly, and `describePresetFailure` mirrors `describeWriteFailure`.

**The load path needs no new synth plumbing at all**: restore the 11 widgets with `dontSendNotification`, then call the **existing** `pushAllParametersToSynth()`. Because that method's contract is already "read the 11 widgets, push to `synth`", widgets and voice cannot drift, the 11 `onValueChange` callbacks do not fire redundantly mid-update, and not one line of `SynthEngine`/`SynthVoice`/`SynthEffects` changes.

**Lock Seed falls out for free**: preset load calls `regenerate (false)`, and `regenerate`'s only Lock-Seed branch is guarded by `drawNewSeed` (`MainComponent.cpp:381`). So "loading applies its seed even if Lock Seed is on" requires **zero change to `regenerate()`** — only a `generation-live-control` spec delta clarifying that Lock Seed governs *reseeding actions*, not explicit recall. Loading while playing therefore restarts from step 1 through the identical Phase 10 publish/adopt handoff; no new audio-thread mechanism exists in this phase.

**The 8 effects fields are structurally excluded**, not merely skipped: the serializer never writes them and `fromValueTree` never touches them, so a loaded `Preset::patch` carries exactly `kDefaultPatch`'s effects values by default member initialization.

### Decision 6 — One full-width `PRESETS` row (34px), deliberately compressing the section-label pattern

Placed after `generationButtonRow`, before the two-column split — the global-actions block, same slot GENERATION occupies.

```
presetLabel 96 │ presetNameEditor 180 │ 6 │ Save 140 │ 6 │ presetBox 180 │ 6 │ Load 140  = 754 ≤ 776
```

**Alternative rejected**: the house pattern of an all-caps section-label row plus a control row = 68px, leaving **10px** of residual column slack. Rejected as too fragile with Mutate (§14) still ahead. **Rationale**: this is the same call Phase 9's Decision 7 made when it merged `synthToggle`+`fxToggle` onto one shared row specifically to free 34px. The all-caps `PRESETS` label takes the leading `kLabelWidth` slot, so section identity survives; the seed row is already precedent for a label sharing its control's row. `setSize (800, 600)` is unchanged; residual right-column slack **44px**.

**Selection never loads** — browsing the ComboBox is non-destructive; `Load` is explicit. Empty state: empty ComboBox, `Load` disabled while nothing is selected; `Save` disabled while the name sanitizes to empty. After a save the list refreshes and selects the saved name; after a load the name editor is populated with the loaded name, so re-saving naturally targets the same preset.

### Decision 7 — Overwrite confirmation: async `NativeMessageBox::showOkCancelBox`

**Choice**: `juce::NativeMessageBox::showOkCancelBox (QuestionIcon, "Overwrite Preset?", …, this, ModalCallbackFunction::create (…))`, guarded by a `juce::Component::SafePointer<MainComponent>` and by `saveButton.setEnabled (false)` for the dialog's lifetime.

**Alternatives**: `AlertWindow::showOkCancelBox` (rejected — `NativeMessageBox` is the *existing* precedent at `MainComponent.cpp:457,471`); any `runModalLoop` variant (rejected — blocking modal loops are discouraged by JUCE and unavailable on some platforms); an inline two-state "Save → Confirm?" button (rejected — invents a control idiom this UI has none of, for the project's first-ever confirmation).

**Rationale**: reuses the one async-dialog precedent the codebase has, including its re-entrancy guard (`exportButton.setEnabled(false)` at `MainComponent.cpp:424`). `SafePointer` is required because, unlike `exportChooser`, a message box is not an owned member reset in the destructor.

## Data Flow

```
MESSAGE THREAD ONLY — no audio-thread work is added by this phase.

SAVE                                          LOAD
 Save click                                    Load click
   │                                             │
   ├─ name = editor text, trimmed                ├─ presetManager.load (name, preset)
   │    empty → status, stop                     │     ├─ file → parseXML → ValueTree
   ├─ preset.patch = currentPatchFromWidgets()   │     ├─ version gate (Decision 3)
   │    (11 fields; 8 effects stay kDefaultPatch)│     ├─ enum names → reject if unknown
   ├─ preset.seed = currentSeed                  │     └─ floats → clampParameter
   ├─ file = presetManager.fileForName (name)    │   failure → statusLabel red, SYNTH UNTOUCHED
   ├─ file.existsAsFile()?                       ├─ applyPatchToWidgets (dontSendNotification)
   │    yes → NativeMessageBox OkCancel ─┐       ├─ pushAllParametersToSynth()   ← EXISTING, unchanged
   │    no  ────────────────────────────┤       ├─ currentSeed = preset.seed; seedEditor.setText
   │                          OK ───────┘       └─ regenerate (false)   ← EXISTING, unchanged
   └─ presetManager.save → refreshPresetList          │                    (drawNewSeed=false ⇒
                                                       │                     Lock Seed not consulted)
                                                       ▼
                                    player.publishSequence  ──→ audio thread adopts at top of
                                                                process(): note-off @0, swap,
                                                                restart from step 1  (Phase 10)
```

## File Changes

| File | Action | Description |
|---|---|---|
| `Source/preset/Preset.h` | Create | `Preset` aggregate (name, `SynthPatch`, `juce::int64 seed`) + `PresetResult` enum. JUCE-aware; header only |
| `Source/preset/PresetManager.h` / `.cpp` | Create | Decisions 1–4: `toValueTree`/`fromValueTree` (pure, no I/O), enum name tables, `fileForName`, `listPresetNames`, `save`, `load`, `defaultPresetDirectory` |
| `Source/synth/SynthPatch.h` | **Unchanged** | Reused as-is; stays JUCE-free; 11 of 19 fields participate |
| `Source/synth/SynthEngine.*`, `SynthVoice.*`, `SynthEffects.*` | **Unchanged** | Decision 5 — recorded so apply does not "add preset setters" |
| `Source/playback/SequencePlayer.*` | **Unchanged** | Phase 10's handoff is reused verbatim |
| `Source/MainComponent.h` / `.cpp` | Modify | `PresetManager` member; 5 widgets; `PRESETS` row in `resized()`; `currentPatchFromWidgets`, `applyPatchToWidgets`, `savePreset`, `writePresetFile`, `loadSelectedPreset`, `refreshPresetList`, `describePresetFailure`. `regenerate()` and `pushAllParametersToSynth()` **unmodified** |
| `Tests/Source/PresetSerializationTests.cpp` | Create | Round-trip, precision, version gate, clamp/reject, path containment |
| `Tests/Source/PresetManagerFileTests.cpp` | Create | Save/load/enumerate against a temp directory |
| `Berlin.jucer`, `Tests/BerlinTests.jucer` | Modify | `<FILE>` registrations for every new `.h`/`.cpp` — a miss fails loudly as an unresolved external |

Specs: **new** `preset-persistence`; **modified** `internal-synth-voice` (its Purpose still claims "no preset save/load yet"), `generation-live-control` (Lock Seed governs reseeding actions, not explicit preset recall — the third seed-mutating path).

## Interfaces / Contracts

```cpp
// Source/preset/Preset.h
namespace berlin
{
struct Preset
{
    juce::String name;
    SynthPatch   patch;      // ONLY the 11 live fields are serialized; the 8 effects
                             // fields are never written or read → always kDefaultPatch
    juce::int64  seed = 0;
};

enum class PresetResult { ok, nameInvalid, directoryUnavailable, writeFailed,
                          fileNotFound, parseFailed, unsupportedVersion };
}

// Source/preset/PresetManager.h
class PresetManager
{
public:
    static constexpr int kSchemaVersion = 1;

    explicit PresetManager (juce::File presetDirectory = defaultPresetDirectory());
    static juce::File defaultPresetDirectory();   // <appData>/Berlin/Presets

    // Sanitised via createLegalFileName. Returns File() if the name is unusable OR if the
    // result would not be a DIRECT child of presetDirectory (containment guard, Threat Matrix).
    juce::File      fileForName (const juce::String& name) const;
    juce::StringArray listPresetNames() const;   // `name` property of each parseable *.xml, sorted

    PresetResult save (const Preset&) const;                        // unconditional; caller confirms
    PresetResult load (const juce::String& name, Preset& out) const; // `out` untouched unless ok

    // Pure, no I/O — the directly unit-testable core.
    static juce::ValueTree toValueTree   (const Preset&);
    static PresetResult    fromValueTree (const juce::ValueTree&, Preset& out);
};
```

**Serialization rule (binding)**: every property is written as a `juce::String` — floats via `juce::String (v, 9)`, the seed via `juce::String (juce::int64)`, enums via the name table. No `double`/`int64` var is ever placed in the tree (Decision 1).

## Testing Strategy

| Layer | What to Test | Approach |
|---|---|---|
| Unit — round trip | All 11 fields survive `toValueTree → toXmlString → fromXml → fromValueTree` **exactly** (`==` on floats, not epsilon); the 8 effects fields equal `kDefaultPatch` after load | `PresetSerializationTests.cpp`, RED first |
| Unit — precision regression | `resonance = 0.7071068f` and each range endpoint round-trip bit-exact — the direct guard against the 6-significant-digit `var::toString()` trap | Same file; would fail against a typed-var implementation |
| Unit — seed fidelity | `INT64_MIN`, `INT64_MAX`, `0`, `-1` round-trip exactly — the case that rules JSON out | Same file |
| Unit — version gate | `schemaVersion=2` → `unsupportedVersion`; missing/garbage version → `parseFailed`; `out` unmodified in both | Same file |
| Unit — malformed | Wrong root tag, missing `<Synth>`, missing attribute, unknown enum name → reject, `out` unmodified. Out-of-range continuous values → clamped to `kMin*`/`kMax*`, loads ok | Same file — Decision 4's two branches |
| Unit — containment | `fileForName` on `../../evil`, `a/b`, `..`, `""`, whitespace-only, and a name of illegal chars only → either `File()` or a direct child of the preset directory. Never escapes | Same file — Threat Matrix row |
| Integration — files | Save → `listPresetNames` contains it → load returns an equal `Preset`; save twice overwrites in place; unparseable file in the directory is omitted from the list, not fatal; missing directory is created; enumeration of an empty/absent directory returns empty | `PresetManagerFileTests.cpp` against `File::createTempFile`-scoped dir, cleaned up |
| Code review | No audio-thread path is touched; no allocation added to `getNextAudioBlock`/`render`; `SafePointer` guards the modal callback | RT-safety constitution review (formality here — Decision 5 adds zero audio-thread work) |
| Manual gate | Save, quit, relaunch, load → 11 widgets *and* the seed restored, audio matches; load while playing → no hung note, dropout, assert, or crash; overwrite prompt Cancel leaves the file byte-identical; Lock Seed on + Load → seed changes | Manual smoke gate, same accepted tradeoff as Phases 4/5/8/10 |

## Threat Matrix

Mostly `N/A`, but not entirely — this phase constructs a filesystem path from free-text UI input, so one row is genuinely applicable.

| Boundary | Applicable? | Expected behaviour / RED test |
|---|---|---|
| Path construction from untrusted input | **Applicable** | A preset name MUST NOT produce a write outside `presetDirectory`. `createLegalFileName` strips separators, but `File::getChildFile` *does* resolve `..` components, so sanitization alone is not relied upon: `fileForName` additionally asserts `result.getParentDirectory() == presetDirectory` and returns `File()` otherwise. RED test: the containment row above. Threat model is user error/accident (single-user standalone app), not a remote attacker — but an accidental `/` writing outside the preset directory is a real correctness defect |
| Reading attacker-controlled file content | **Applicable (low)** | A hand-edited or corrupt `*.xml` MUST NOT crash, assert, or push invalid values into the voice. Covered by Decision 4 and the malformed/clamp tests; `parseXML` failure is a value, never an exception path |
| Shell commands / subprocesses | N/A | No process is spawned |
| Routing / network | N/A | No network or IPC surface |
| VCS / PR automation | N/A | None |
| Executable-file classification | N/A | Only inert `*.xml` is written; nothing is ever executed or loaded as code |

## Migration / Rollout

No migration required — `kSchemaVersion = 1` is the first persisted format this project has ever had, so no schema exists in the wild and the `schemaVersion <` branch is unreachable today (its policy is declared in Decision 3 for the next version's benefit). Single-commit revert deletes `Source/preset/`, the `PRESETS` row, and the new specs; orphaned `*.xml` files on disk are inert because nothing else reads them.

## Open Questions

- [ ] Preset directory root: `userApplicationDataDirectory/Berlin/Presets` (chosen, per the skill's "persisted app state" framing) vs `userDocumentsDirectory/Berlin/Presets` (consistent with the export precedent and more discoverable for the hand-editing the human-readable format invites). Reversible in one line; flagged, not blocking.
- [ ] `listPresetNames()` parses every file to read its display name. Fine at bank scale; if a large factory bank ever ships, this becomes a startup cost worth caching. Recorded, not solved.
