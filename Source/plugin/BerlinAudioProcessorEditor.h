/*
  ==============================================================================

   BerlinAudioProcessorEditor - the ~40-widget GUI, ported from MainComponent
   (roadmap Phase 11 / vst3-au-plugin, plugin-host-integration spec,
   design.md D1-D3).

   JUCE-aware (juce_gui_basics): NOT reachable from the headless test target
   by design (unit-test-harness spec explicitly excludes this file) - no
   automated test coverage exists or is expected for this file, matching the
   project's accepted GUI-coverage gap precedent (MidiOutputSink,
   MidiEventTranslator's device-facing use, etc).

   Holds NO independent engine state (plugin-host-integration's "Editor
   Attach/Detach Does Not Affect Engine State" requirement) - every widget
   callback forwards to `owner` (the BerlinAudioProcessor), and every value
   shown is read back FROM `owner`, never cached locally across the editor's
   lifetime except transient in-flight widget values being staged before a
   command call. Registers as a ChangeListener in the ctor, deregisters in
   the dtor (D3) - changeListenerCallback() re-pulls patch/seed from `owner`
   so a background auto-evolve mutate() or a host setStateInformation() is
   reflected the next time this exact editor instance (or a freshly
   recreated one) is shown.

   Owns the overwrite NativeMessageBox and the export FileChooser (D2) -
   BerlinAudioProcessor.cpp stays free of juce_gui_basics.

  ==============================================================================
*/

#pragma once

// The FULL juce_audio_processors module (not juce_audio_processors_headless)
// is required here: juce::AudioProcessorEditor's complete class definition
// (with its juce::Component base) lives in juce_audio_processors, which
// depends on juce_gui_extra/juce_gui_basics. juce_audio_processors_headless
// only forward-declares "class AudioProcessorEditor;" - just enough for
// BerlinAudioProcessor's pure-virtual pointer-return signature to compile
// headlessly. This header is therefore only ever reachable from the
// non-headless #else branch in BerlinAudioProcessor.cpp - never from
// Tests/BerlinTests.jucer.
#include <juce_audio_processors/juce_audio_processors.h>

#include "plugin/BerlinAudioProcessor.h"

namespace berlin
{

class BerlinAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                          private juce::ChangeListener
{
public:
    explicit BerlinAudioProcessorEditor (BerlinAudioProcessor& processorToEdit);
    ~BerlinAudioProcessorEditor() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    // Pulls owner's current patch/seed into the widgets (ctor + change notifications).
    void refreshFromProcessor();

    void launchExportChooser();
    void exportSequenceTo (const juce::File& destination);

    berlin::SynthPatch currentPatchFromWidgets() const;
    void                applyPatchToWidgets (const berlin::SynthPatch& patch);
    void                pushPatchFromWidgets();          // currentPatchFromWidgets() -> owner.setPatch()
    void                pushGenerationParamsFromWidgets(); // stages mode/pulses/rotation/probability/lockSeed

    void         savePreset();
    void         writePresetFile (const juce::String& name);
    void         loadSelectedPreset();
    void         refreshPresetList (const juce::String& nameToSelect = {});
    juce::String describePresetFailure (berlin::PresetResult result) const;
    juce::String describeWriteFailure (berlin::MidiFileWriteResult result) const;

    BerlinAudioProcessor& owner;

    juce::TextButton   exportButton { "Export MIDI..." };
    juce::Label        statusLabel;
    juce::ToggleButton synthToggle { "Synth" };
    juce::ToggleButton fxToggle { "FX" };
    std::unique_ptr<juce::FileChooser> exportChooser;

    // ---- Generation controls ----
    juce::Label        generationSectionLabel;
    juce::Label        seedLabel;
    juce::TextEditor   seedEditor;
    juce::TextButton   generateButton { "Generate" };
    juce::TextButton   randomizeButton { "Randomize" };
    juce::TextButton   mutateButton { "Mutate" };
    juce::ToggleButton lockSeedToggle { "Lock Seed" };

    // ---- Preset controls ----
    juce::Label      presetSectionLabel;
    juce::TextEditor presetNameEditor;
    juce::TextButton savePresetButton { "Save" };
    juce::ComboBox   presetBox;
    juce::TextButton loadPresetButton { "Load" };

    // ---- Auto-Evolve controls ----
    juce::Label        evolutionSectionLabel;
    juce::ToggleButton autoEvolveToggle { "Auto-Evolve" };
    juce::Label        evolveRateLabel;
    juce::ComboBox     evolveRateBox;

    // ---- Euclidean Rhythms controls ----
    juce::Label      rhythmModeLabel;
    juce::ComboBox   rhythmModeBox;
    juce::Label      pulsesLabel;
    juce::Slider     pulsesSlider;
    juce::Label      rotationLabel;
    juce::Slider     rotationSlider;

    // ---- Probability Matrices controls ----
    juce::Label  stepProbabilityLabel;
    juce::Slider stepProbabilitySlider;

    // ---- Parameter controls ----
    juce::Label oscillatorSectionLabel, filterSectionLabel, envelopeSectionLabel, lfoSectionLabel;

    juce::ComboBox waveformBox, lfoDestinationBox;
    juce::Label    waveformLabel, lfoDestinationLabel;

    juce::Slider cutoffSlider, resonanceSlider, pulseWidthSlider;
    juce::Slider attackSlider, decaySlider, sustainSlider, releaseSlider;
    juce::Slider lfoRateSlider, lfoDepthSlider;

    juce::Label cutoffLabel, resonanceLabel, pulseWidthLabel;
    juce::Label attackLabel, decayLabel, sustainLabel, releaseLabel;
    juce::Label lfoRateLabel, lfoDepthLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BerlinAudioProcessorEditor)
};

} // namespace berlin
