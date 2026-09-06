#pragma once

#include <JuceHeader.h>
#include "playback/SequencePlayer.h"
#include "playback/StepEventBuffer.h"
#include "midi/MidiEventTranslator.h"
#include "midi/MidiOutputSink.h"
#include "export/MidiExportTimeline.h"
#include "export/MidiFileWriter.h"
#include "synth/SynthEngine.h"
#include "preset/Preset.h"
#include "preset/PresetManager.h"

//==============================================================================
/*
    This component lives inside our window, and this is where you should put all
    your controls and content.
*/
class MainComponent  : public juce::AudioAppComponent
{
public:
    //==============================================================================
    MainComponent();
    ~MainComponent() override;

    //==============================================================================
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    //==============================================================================
    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    //==============================================================================
    // Your private member variables go here...
    static berlin::Sequence buildSeededSequence (juce::int64 seed);
    static juce::File       defaultExportFile();     // default destination seeded into the save dialog

    void launchExportChooser();
    void exportSequenceTo (const juce::File& destination);

    // Re-pushes every control's current value to `synth` (design.md Decision 6).
    // Called once from prepareToPlay, immediately after synth.prepare(spec) -
    // SynthEngine::prepare seeds the voice from kDefaultPatch, so without this a
    // device/sample-rate change would silently snap parameters back to default
    // while the sliders still showed the user's values.
    void pushAllParametersToSynth();

    // Rebuilds a Sequence from the current (or freshly drawn) seed and
    // publishes it to `player` via the audio-thread-safe handoff
    // (generation-live-control spec, design.md Decision 1). On success,
    // `currentSequence` is updated too so Export always reflects the CURRENT
    // pattern, never a stale one.
    void regenerate (bool drawNewSeed);

    // ---- Preset controls (roadmap Phase 11 / preset-system, design.md
    // Decision 5) - `regenerate()` and `pushAllParametersToSynth()` above are
    // reused VERBATIM by the load path; neither is modified for presets.
    berlin::SynthPatch currentPatchFromWidgets() const;
    void                applyPatchToWidgets (const berlin::SynthPatch& patch);
    void                savePreset();
    void                writePresetFile (const berlin::Preset& preset);
    void                loadSelectedPreset();
    void                refreshPresetList (const juce::String& nameToSelect = {});
    juce::String        describePresetFailure (berlin::PresetResult result) const;

    juce::int64                 currentSeed;
    berlin::Sequence            currentSequence;      // MUST precede `player` (Decision 2); audio-thread-exclusive once published
    berlin::SequencePlayer      player;
    berlin::StepEventBuffer     blockEvents;
    berlin::MidiEventTranslator midiTranslator;
    berlin::MidiOutputSink      midiSink;
    juce::MidiBuffer            midiBlock;
    berlin::SynthEngine         synth;
    berlin::PresetManager       presetManager;

    juce::TextButton   exportButton { "Export MIDI..." };
    juce::Label        statusLabel;
    juce::ToggleButton synthToggle { "Synth" };
    juce::ToggleButton fxToggle { "FX" };
    std::unique_ptr<juce::FileChooser> exportChooser;

    // ---- Parameter controls (roadmap Phase 9 / parameter-controls) ----
    // Left column: OSCILLATOR (waveform, pulse width), FILTER (cutoff, resonance).
    // Right column: ENVELOPE (attack, decay, sustain, release), LFO (destination, rate, depth).
    juce::Label oscillatorSectionLabel, filterSectionLabel, envelopeSectionLabel, lfoSectionLabel;

    juce::ComboBox waveformBox, lfoDestinationBox;
    juce::Label    waveformLabel, lfoDestinationLabel;

    juce::Slider cutoffSlider, resonanceSlider, pulseWidthSlider;
    juce::Slider attackSlider, decaySlider, sustainSlider, releaseSlider;
    juce::Slider lfoRateSlider, lfoDepthSlider;

    juce::Label cutoffLabel, resonanceLabel, pulseWidthLabel;
    juce::Label attackLabel, decayLabel, sustainLabel, releaseLabel;
    juce::Label lfoRateLabel, lfoDepthLabel;

    // ---- Generation controls (roadmap Phase 10 / generation-randomize) ----
    juce::Label        generationSectionLabel;
    juce::Label        seedLabel;
    juce::TextEditor   seedEditor;
    juce::TextButton   generateButton { "Generate" };
    juce::TextButton   randomizeButton { "Randomize" };
    juce::ToggleButton lockSeedToggle { "Lock Seed" };

    // ---- Preset controls (roadmap Phase 11 / preset-system) ----
    // One full-width row (design.md Decision 6): PRESETS label | name editor |
    // Save | preset selector | Load.
    juce::Label      presetSectionLabel;
    juce::TextEditor presetNameEditor;
    juce::TextButton savePresetButton { "Save" };
    juce::ComboBox   presetBox;
    juce::TextButton loadPresetButton { "Load" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
