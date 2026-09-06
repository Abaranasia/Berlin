# Exploration: Preset System (roadmap Phase 11)

## Current State

`Source/synth/SynthPatch.h` is a JUCE-free POD struct (confirmed still true, per its own header comment citing design.md's "scoped convention exception guardrail (a)") but it is NOT a clean 1:1 match for "the synth preset." It has **19 fields total**: waveform, cutoffHz, resonance, pulseWidth, attack, decay, sustain, release, lfoRateHz, lfoDepth, lfoDestination (11 fields) PLUS delayTimeSeconds, delayFeedback, delayMix, reverbRoomSize, reverbDamping, reverbWetLevel, reverbDryLevel, outputLevel (8 fields).

Only the first 11 fields are actually LIVE/user-adjustable: `SynthVoice::Parameters` (`Source/synth/SynthVoice.h:104-117`) holds them as atomics, `SynthVoice`/`SynthEngine` expose exactly **11 setters** (setWaveform, setCutoffHz, setResonance, setPulseWidth, setAttackSeconds, setDecaySeconds, setSustain, setReleaseSeconds, setLfoRateHz, setLfoDepth, setLfoDestination — verified directly in `Source/synth/SynthEngine.h:83-93`), and `MainComponent` has exactly 11 matching widgets.

The other 8 fields (delay/reverb/outputLevel) are read ONCE by `SynthEngine::prepare()` -> `effects.prepare(spec, kDefaultPatch)` (`Source/synth/SynthEngine.cpp:15-22`) and never again — no setters exist on `SynthEngine`/`SynthEffects` for them, no UI widgets exist. They're permanently pinned to `kDefaultPatch`'s hardcoded values for the whole session.

**Scoping decision (confirmed by user)**: presets scope to exactly the 11 live fields; the 8 effects fields are explicitly excluded, staying pinned to `kDefaultPatch`, unaffected by presets.

No message-thread-side shadow copy of the 11 current values exists on `MainComponent` — the JUCE widgets themselves are the canonical last-set-value holders (`cutoffSlider.getValue()`, `waveformBox.getSelectedId()`, etc.), confirmed by the existing `pushAllParametersToSynth()` method, which already reads all 11 widgets to re-push values to `synth` after a device restart. "Save current state as preset" can reuse this exact pattern in reverse (read 11 widgets -> build a `SynthPatch` -> serialize), with **zero atomic readback needed**.

"Load a preset" = call the same 11 existing setters again (exactly like `pushAllParametersToSynth()` does) plus update the 11 widgets' displayed values — no audio-thread cooperation needed at all, unlike Phase 10's live-regeneration (which needed a stop-then-swap `SequencePlayer` scheme). Preset save/load's RT profile is strictly simpler than Phase 10's.

**Generation seed (confirmed in scope, combined with synth params)**: `currentSeed` (`juce::int64`, `MainComponent.h:56`) plus `regenerate(bool drawNewSeed)` (`MainComponent.cpp`) already established in Phase 10; a seed preset sets `currentSeed` + `seedEditor` text + calls `regenerate(false)` — fully reuses existing plumbing. `numSteps`/`activeSteps` remain hardcoded constants, not UI-exposed, so out of scope unless this phase also exposes them (not requested).

No persistence mechanism exists anywhere in the codebase — grep for ValueTree/PropertiesFile/JSON/XML across `Source/`: zero hits. Only file-I/O precedent is `Source/export/MidiFileWriter.cpp` (raw `FileOutputStream`) and `MainComponent::launchExportChooser()`/`exportSequenceTo()` using `juce::FileChooser::launchAsync` (async OS file-picker) — reusable IF presets use one-file-per-preset via OS dialogs; a single-file named-list-plus-dropdown UX (closer to what the proposal implies) would instead need `juce::PropertiesFile` or a hand-rolled ValueTree/JSON file.

`.claude/skills/juce-app-dev/SKILL.md`'s Decision Gate table: "Persisted app state (settings, presets): serialize via one `juce::ValueTree`, not scattered member variables, with a schema version int — not `AudioProcessorValueTreeState` (no `AudioProcessor` exists here)." This is the project's only established convention here, but prescriptive guidance, not precedent code (greenfield).

Serialization should live in a NEW JUCE-aware layer (e.g. a `PresetManager`/`PresetStore` class) converting the combined synth+seed data <-> `juce::ValueTree`/JSON, keeping `SynthPatch.h` itself JUCE-free per its documented exception.

Layout: window is 800x600. Verified via `resized()` (`MainComponent.cpp:311-377`) row math: top rows (export/status/toggles/generation) consume 204px of 576 usable height; remaining 372px splits into two columns — left uses 192px (180px slack), right (ENVELOPE+LFO) uses 294px (~78px slack) — right column is the binding constraint. This is close to Phase 9 design doc's "~84px slack" claim (small rounding variance) — directionally confirmed still accurate. A new preset UI section needs ~34px (single compact row) to ~68px (two rows) — a single-row design just fits the 78px slack; a two-row design barely fits (10px margin) or may need the window to grow.

Proposal doc's only "Presets" mention (`docs/proposal/berlin_school_generative_sequencer_proposal.md:1518`) is a bare one-word list item under the original "Phase 7 — User Interface" (roadmap has since split this into parameter-controls/generation-randomize/preset-system) alongside "Parameter controls. Generate. Randomize. Mutate." — genuinely no scope detail; did not imply synth-only, seed-only, or combined. User has now decided: combined.

## Affected Areas

- `Source/synth/SynthPatch.h` — natural unit for the synth-parameter half of preset data; scoped to 11 fields, 8 effects fields excluded.
- `Source/synth/SynthEngine.h/.cpp`, `Source/synth/SynthVoice.h/.cpp` — no changes needed; setters already sufficient for "load."
- `Source/MainComponent.h/.cpp` — needs new preset UI (name entry, save/load controls, browse list); `pushAllParametersToSynth()` is the direct precedent for both "read all 11 widgets" (save) and "push all 11 values" (load — widget-value-restore-on-load is new). Also needs to fold `currentSeed`/`regenerate()` into the same save/load actions.
- A new `PresetManager`/`PresetStore`-type class (does not exist yet) — needed to own serialization (ValueTree/JSON), file or `PropertiesFile` I/O, and named-preset enumeration; keeps `SynthPatch.h` JUCE-free.
- `openspec/specs/` — no existing spec covers persistence; a new capability spec (e.g. `preset-persistence`) is needed.
- No RT-safety changes needed anywhere — first phase in this project's history that is 100% message-thread work with zero audio-thread involvement.

## Approaches (superseded by user's scope decision — kept for record)

1. **Synth-patch-only presets** — smallest, cleanest scope; NOT chosen.
2. **Seed-only presets** — smallest possible scope; NOT chosen.
3. **Combined "full patch" presets (synth params + seed together)** — CHOSEN by user. Closest to typical user expectation of "preset" in a generative instrument; one action covers the whole musical state. Larger scope than option 1, couples two independently-evolved subsystems, inherits the 11-vs-19-field scoping question (resolved: exclude the 8 effects fields).

## Risks

- No persistence precedent exists in this codebase — first phase needing file/ValueTree/JSON I/O; the juce-app-dev skill's ValueTree+schema-version guidance is untested guidance, not proven precedent.
- Layout has only ~78px slack in the tighter (right) column, verified via row-math — a two-row preset UI barely fits or the window may need to grow. sdd-design must address this explicitly.
- Combined synth+seed presets couple two independently-evolved subsystems (Phase 9 and Phase 10) into one save/load action and one file format — sdd-design should decide whether this is one flat schema or two nested sections within one preset file (recommend addressing explicitly, not silently).
- Preset naming/collision handling (overwrite vs. rename vs. reject) is undecided — a real UX decision for sdd-propose/sdd-design.

## Ready for Proposal

Yes — investigation surfaced one genuine architectural scoping trap (11-vs-19 SynthPatch fields, now resolved: exclude the 8), one hard layout constraint (~78px slack), and confirmed no RT-safety novelty (first phase that's pure message-thread work). Scope decided by user: combined synth+seed presets, effects fields excluded.
