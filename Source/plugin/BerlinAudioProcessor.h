/*
  ==============================================================================

   BerlinAudioProcessor - owns all engine state independent of any editor
   (roadmap Phase 11 / vst3-au-plugin, plugin-host-integration spec,
   design.md's Architecture Decisions D1-D6 + Interfaces/Contracts block).

   JUCE-aware (juce_audio_processors_headless, juce_events for
   ChangeBroadcaster/Timer): links into BOTH the headless test target
   (BERLIN_HEADLESS=1) and the real plugin/standalone targets. MUST NOT
   include juce_gui_basics or juce_audio_devices anywhere in this header or
   its .cpp - that is what keeps this file reachable from the console test
   target (unit-test-harness spec).

   Owns SequencePlayer, Transport (via SequencePlayer), SynthEngine,
   MidiEventTranslator, PresetManager, the current sequence/seed/mutation
   state, GenerationParams, and the auto-evolve Timer (D1) - none of it is
   editor-owned, so attach/detach of a BerlinAudioProcessorEditor never
   touches this state (plugin-host-integration's "Editor Attach/Detach Does
   Not Affect Engine State" requirement).

   Commands (regenerate/mutate/save/loadPreset/exportMidiTo) return
   bool/berlin::PresetResult/berlin::MidiFileWriteResult rather than setting a
   status string (D2) - describePresetFailure/describeWriteFailure/
   statusLabel/the overwrite NativeMessageBox/the FileChooser all live in the
   editor (Phase 5), keeping this file free of juce_gui_basics.

   ChangeBroadcaster (D3): broadcasts after a SUCCESSFUL auto-evolve
   Timer-driven mutate() and after setStateInformation - NOT after a
   manually-invoked regenerate()/mutate()/loadPreset(), since the caller
   (editor) already has the boolean/enum result synchronously in those cases.

   Generation params are NOT persisted (D4): getStateInformation/
   setStateInformation reuse PresetManager::toValueTree/fromValueTree
   verbatim - patch + seed only. A malformed/garbage/truncated/empty
   setStateInformation block leaves state completely untouched (Threat
   Matrix row, tasks.md 3.4) - this is a deliberate reconciliation in favor
   of design.md's Threat Matrix table over one scenario's literal wording in
   the plugin-state-recall spec ("falls back to default patch and seed"),
   because tasks.md's RED-test directive for this exact row explicitly says
   "leaves state untouched, no crash".

   createEditor()/hasEditor() are compile-gated behind BERLIN_HEADLESS (D5),
   not runtime-gated: createEditor() is pure virtual so it must be defined in
   every build; under BERLIN_HEADLESS it returns nullptr/false with no editor
   #include at all, keeping juce_gui_basics out of the headless link
   entirely.

  ==============================================================================
*/

#pragma once

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>
#include <juce_events/juce_events.h>

#include "core/Sequence.h"
#include "generation/AutoEvolveSchedule.h"
#include "generation/GenerationParams.h"
#include "midi/MidiChannel.h"
#include "midi/MidiEventTranslator.h"
#include "playback/SequencePlayer.h"
#include "playback/StepEventBuffer.h"
#include "playback/Transport.h"
#include "preset/Preset.h"
#include "preset/PresetManager.h"
#include "export/MidiExportTimeline.h"
#include "export/MidiFileWriter.h"
#include "synth/SynthEngine.h"
#include "synth/SynthPatch.h"

namespace berlin
{

class BerlinAudioProcessor final : public juce::AudioProcessor,
                                    public juce::ChangeBroadcaster,
                                    private juce::Timer
{
public:
    // presetDirectory defaults to PresetManager::defaultPresetDirectory();
    // tests inject a temp directory (preset-persistence's TempPresetDir
    // precedent) so no automated test ever touches the real user data dir.
    explicit BerlinAudioProcessor (juce::File presetDirectory = PresetManager::defaultPresetDirectory());
    ~BerlinAudioProcessor() override;

    // ---- AudioProcessor ----
    const juce::String getName() const override { return "Berlin"; }

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;
    // D8 (post-manual-gate correction): the VST3 SDK requires an event input
    // bus for any Instrument-category plugin, or the host rejects the plugin
    // outright (Ableton Live's own log: "plugin has instrument category, but
    // no valid event input bus"). Declaring the bus does NOT change the
    // ignore-content rule below - processBlock's unconditional
    // midiMessages.clear() still stands; only the bus's existence changed.
    bool acceptsMidi()  const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    // ---- Commands: MESSAGE THREAD ONLY (MutationEngine allocates) ----
    bool regenerate (bool drawNewSeed);
    bool mutate();
    PresetResult save (const juce::String& name);
    PresetResult loadPreset (const juce::String& name);
    bool presetExists (const juce::String& name) const;
    juce::StringArray listPresetNames() const;
    MidiFileWriteResult exportMidiTo (const juce::File& destination) const;

    // ---- State in / out (message thread) ----
    void setPatch (const SynthPatch& patch);
    const SynthPatch& getPatch() const { return currentPatch; }
    void setGenerationParams (const GenerationParams& params) { generationParams = params; }
    const GenerationParams& getGenerationParams() const { return generationParams; }
    void setSeed (juce::int64 seed) { currentSeed = seed; }
    juce::int64 getSeed() const { return currentSeed; }
    void setSynthEnabled (bool shouldBeEnabled) { synth.setEnabled (shouldBeEnabled); }
    void setEffectsEnabled (bool shouldBeEnabled) { synth.setEffectsEnabled (shouldBeEnabled); }

    // currentBpm is the SINGLE SOURCE OF TRUTH for tempo (design.md's
    // Ownership note): the editor is a view, Transport is an audio-thread-
    // only consumer, MIDI export reads currentBpm - none of them originate a
    // BPM value. setBpm clamps to [berlin::kMinBpm, berlin::kMaxBpm]
    // (SynthPatch.h, Phase 7's canonical home) and forwards to player.setBpm
    // (message thread; adopted by the audio thread at the top of its next
    // process() call, design.md Decision 4).
    void   setBpm (double newBpm);
    double getBpm() const { return currentBpm; }
    void setAutoEvolveEnabled (bool shouldBeEnabled);
    void setAutoEvolveRate (int loops) { autoEvolveRate = loops; }
    int getMutationCount() const { return mutationCount; }
    const Sequence& getCurrentSequence() const { return currentSequence; }

    // ---- Standalone-only seam: lets MainComponent keep today's OS note-off flush ----
    bool flushPendingNoteOff (juce::MidiBuffer& out) noexcept;

private:
    void timerCallback() override;
    void pushPatchToSynth();

    // kDefaultBpm/kMinBpm/kMaxBpm now live in SynthPatch.h (design.md D7,
    // Phase 7's canonical home) - referenced unqualified below since this
    // class is nested in `namespace berlin`. Previously local static
    // constexpr members of this class (Phase 4 interim measure); removed in
    // favor of the single shared definition, deduping the two copies.
    static constexpr int    kStepsPerBeat   = 4;
    static constexpr int    kExportRepeats  = 4;
    static constexpr juce::int64 kDefaultSeed = 12345;

    GenerationParams generationParams;
    SynthPatch       currentPatch;
    double           currentBpm { kDefaultBpm };   // single source of truth (design.md Ownership note)

    juce::int64 currentSeed;
    Sequence    currentSequence;   // MUST precede `player`; audio-thread-exclusive once published

    juce::int64 sequenceSeed;
    int         mutationCount { 0 };

    SequencePlayer      player;
    StepEventBuffer     blockEvents;
    MidiEventTranslator midiTranslator;
    SynthEngine         synth;
    PresetManager       presetManager;

    bool               autoEvolveEnabled { false };
    int                autoEvolveRate { 4 };
    AutoEvolveSchedule autoEvolveSchedule;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BerlinAudioProcessor)
};

} // namespace berlin
