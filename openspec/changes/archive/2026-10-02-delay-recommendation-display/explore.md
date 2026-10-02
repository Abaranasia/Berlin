## Exploration: delay-recommendation-display (Slice 3 of 5, Berlin School authenticity)

### Current State
- `Source/core/TempoSync.h/.cpp` is JUCE-free: `enum class SyncDivision { half, quarter, dottedEighth, eighth, eighthTriplet, sixteenth }` (6 divisions, `kNumSyncDivisions = 6`), `factorFor(division)` (constexpr beats-per-repeat factor), `delaySecondsFor(bpm, division) noexcept` = `(60/bpm) * factorFor(division)`, bpm<=0 -> 0. Fully covered by `Tests/Source/TempoSyncTests.cpp` (exact-value tests, triplet ratio, bpm<=0, and a kMaxDelaySeconds-bound test at kMinBpm).
- `SynthPatch.h` owns `kMinBpm=40.0, kMaxBpm=240.0, kDefaultBpm=120.0, kMinDelayTimeSeconds=0.0f, kMaxDelaySeconds=3.0f`. At kMinBpm the `half` division exactly reaches 3.0s (the ceiling was deliberately raised 2.0f->3.0f in Slice 2 so every division is reachable).
- BPM ownership: `BerlinAudioProcessor::currentBpm` (plain double, NOT atomic) is "the SINGLE SOURCE OF TRUTH for tempo... editor is a view" (header comment). `setBpm()` is called ONLY from the editor's `tempoSlider.onValueChange -> pushTempoFromWidgets()`, i.e. message-thread only in both directions today — there is no host-tempo-sync feature and no other thread ever calls `setBpm`. `player.setBpm()` hands the value to the audio thread via a `std::atomic<double> pendingBpm` on `SequencePlayer`, consumed once per block by `Transport::setBpm` (phase-preserving rebase). The GUI reads `owner.getBpm()` directly (same thread, no polling/Timer) in the constructor and in `recomputeSyncedDelayTime()`.
- Delay/Sync UI (`BerlinAudioProcessorEditor`, Slice 2 "tempo-delay-ui"): `delaySyncToggle` (Sync/Free) + `delayDivisionBox` (6 items, labels `kDivisionNames[] = {"1/2","1/4","1/8.","1/8","1/8T","1/16"}`, parallel order to `SyncDivision`) + `delayTimeSlider` (seconds, kMinDelayTimeSeconds..kMaxDelaySeconds). `recomputeSyncedDelayTime()` is the single recompute path: in Sync mode it sets `delayTimeSlider` to `delaySecondsFor(owner.getBpm(), selectedDivision)`; in Free mode it restores `lastManualDelayTimeSeconds`. It is called from `pushTempoFromWidgets()` (BPM slider changed), `delaySyncToggle.onClick`, and `delayDivisionBox.onChange` — so BPM-driven recompute is already fully event-driven, no Timer anywhere in this flow.
- Layout is a fixed-size window: `setSize(800, 680 + 7*(kControlHeight+kMargin/2))`, built with `juce::Rectangle<int>::removeFromTop/removeFromLeft` row carving in `resized()` (`kMargin=12, kControlHeight=28, kButtonWidth=140, kLabelWidth=96`). No scroll view exists; every new row needs a `setSize` height-formula bump and an explicit new block in `resized()`, consistent with ~15 existing section blocks (TEMPO, DELAY rows 1-2, REVERB rows 1-2, GENERATION, EUCLIDEAN, PROBABILITY, SCALE/ROOT, RANGE, EVOLUTION, PRESETS, then the two-column synth section).
- `docs/research/berlin_school_sequence_conventions.md` Finding 1 (sourced quote, T. Coppens): "a dotted-eighth delay... as time setting in your delay, then you get that typical Berlin-School feeling when you have a 16 steps sequence" — frames delay time as a *compositional parameter picked relative to tempo*, and separates it from the internal synth's own FX chain. This is the sourced rationale behind the initiative's "designed for external synths/delay units" framing and supports showing several divisions at once rather than just the one currently dialed into `delayDivisionBox`.
- `fxToggle`/`updateDelayReverbEnablement()` greys out the internal delay/reverb widget group (`delayReverbWidgets`) when internal FX is off — this gate is specific to the internal `SynthEffects` chain, not to BPM or tempo-sync math.

### Affected Areas
- `Source/plugin/BerlinAudioProcessorEditor.h` — new member widgets (labels) for the recommendation display, declared alongside the existing DELAY section members.
- `Source/plugin/BerlinAudioProcessorEditor.cpp` — new construction block (near the existing DELAY section, ~line 84-98), a new `resized()` row/block, a `setSize` height bump, and a new private helper (e.g. `updateDelayRecommendations()`) wired into the same call sites as `recomputeSyncedDelayTime()` (constructor init + `pushTempoFromWidgets()`), NOT gated by `fxToggle`/`updateDelayReverbEnablement()` since the recommendation is for an external unit and must stay meaningful even with internal FX off.
- `Source/core/TempoSync.h/.cpp` — no production change expected to existing math; this slice consumes `delaySecondsFor`/`SyncDivision`/`kNumSyncDivisions`.
- Possibly a small new pure-formatting helper (e.g. seconds->ms rounding / recommendation string); natural home is `Source/core/TempoSync.h` or a small adjacent header if `juce::String` is needed.
- `Tests/Source/TempoSyncTests.cpp` or a new test file — direct-value tests for any formatting helper.
- No `PresetManager`/schema change expected — read-only display derived from already-persisted `bpm`.

### Approaches
1. **Single multi-division read-only Label, event-driven** — one `juce::Label` showing all 6 divisions' times in one formatted string (e.g. "1/2 750ms  1/4 375ms  1/8. 281ms  1/8 188ms  1/8T 125ms  1/16 94ms" at BPM=160), recomputed from the same places `recomputeSyncedDelayTime()` is called. Pure string-building logic in a free function, unit-testable.
   - Pros: smallest footprint (1 widget, 1 row), reuses event-driven BPM path (no Timer, no new thread-safety surface), trivially testable.
   - Cons: less scannable than columns; long string needs width tuning in the fixed 800px window; harder to highlight the currently-synced division.
   - Effort: Low.
2. **Per-division label grid (6 Label pairs, 2 rows of 3)** — mirrors the "label + control" row convention.
   - Pros: best glanceability, easy to highlight selected division later, consistent idiom.
   - Cons: ~12 new components, more `resized()` code, 2 new rows in a dense fixed window.
   - Effort: Medium.
3. **Live ms readout next to the single selected division only.**
   - Pros: zero new rows, trivial diff.
   - Cons: does not deliver the comparative core value (user must flip the combo one division at a time). Not recommended.
   - Effort: Low, low value.

### Recommendation
Approach 1 for the fast-follow MVP, with Approach 2 as possible follow-up polish. Reuses proven event-driven BPM wiring on the message thread, small diff, pure testable formatter. The display must NOT be gated by `fxToggle` — it serves external delay units and supports Slice 5's synth-optional direction.

### Risks
- Fixed-size, non-scrolling window: new row requires a `setSize` height-formula bump (easy to forget, would clip).
- No canonical Berlin School delay division — display must expose several, not pick one as "correct".
- `currentBpm` is a plain double (pre-existing, message-thread-only by design); display must read BPM only on the message thread, no Timer/background poll.
- ms vs seconds formatting and rounding precision must be decided explicitly in proposal/design.

### Ready for Proposal
Yes. Open decisions: Approach 1 vs 2 (recommend 1), ms rounding/format convention.
