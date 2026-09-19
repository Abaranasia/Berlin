/*
  ==============================================================================

   BerlinAudioProcessorEditor - out-of-line definitions (roadmap Phase 11 /
   vst3-au-plugin, plugin-host-integration spec, design.md D1-D3).

  ==============================================================================
*/

#include "BerlinAudioProcessorEditor.h"

namespace
{
    constexpr int   kNumSteps               = 16;
    constexpr int   kDefaultPulses          = 5;
    constexpr int   kDefaultRotation        = 0;
    constexpr float kDefaultStepProbability = 0.5f;

    constexpr int kMargin = 12, kControlHeight = 28, kButtonWidth = 140, kLabelWidth = 96;

    const char* kExportFileName = "berlin-export.mid";

    juce::File defaultExportFile()
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
            .getChildFile ("Berlin")
            .getChildFile (kExportFileName);
    }
}

namespace berlin
{

BerlinAudioProcessorEditor::BerlinAudioProcessorEditor (BerlinAudioProcessor& processorToEdit)
    : juce::AudioProcessorEditor (processorToEdit),
      owner (processorToEdit)
{
    addAndMakeVisible (exportButton);
    addAndMakeVisible (statusLabel);
    exportButton.onClick = [this] { launchExportChooser(); };

    addAndMakeVisible (synthToggle);
    synthToggle.setToggleState (true, juce::dontSendNotification);
    synthToggle.onClick = [this] { owner.setSynthEnabled (synthToggle.getToggleState()); };

    addAndMakeVisible (fxToggle);
    fxToggle.onClick = [this] { owner.setEffectsEnabled (fxToggle.getToggleState()); };

    // ---- Generation controls ----
    addAndMakeVisible (generationSectionLabel);
    generationSectionLabel.setText ("GENERATION", juce::dontSendNotification);

    auto validateSeedField = [this]
    {
        const juce::String text = seedEditor.getText().trim();

        bool valid = ! text.isEmpty();
        int  index = 0;

        if (valid && text[0] == '-')
        {
            ++index;
            valid = text.length() > 1;
        }

        for (; valid && index < text.length(); ++index)
            if (! juce::CharacterFunctions::isDigit (text[index]))
                valid = false;

        if (valid)
        {
            owner.setSeed (text.getLargeIntValue());
        }
        else
        {
            seedEditor.setText (juce::String (owner.getSeed()), juce::dontSendNotification);
            statusLabel.setColour (juce::Label::textColourId, juce::Colours::red);
            statusLabel.setText ("Seed must be a whole number.", juce::dontSendNotification);
        }
    };

    addAndMakeVisible (seedLabel);
    seedLabel.setText ("Seed", juce::dontSendNotification);
    seedLabel.setJustificationType (juce::Justification::centredLeft);

    addAndMakeVisible (seedEditor);
    seedEditor.setText (juce::String (owner.getSeed()), juce::dontSendNotification);
    seedEditor.onFocusLost = validateSeedField;
    seedEditor.onReturnKey = validateSeedField;

    addAndMakeVisible (generateButton);
    generateButton.onClick = [this]
    {
        pushGenerationParamsFromWidgets();
        if (! owner.regenerate (false))
        {
            statusLabel.setColour (juce::Label::textColourId, juce::Colours::red);
            statusLabel.setText ("Busy, try again", juce::dontSendNotification);
            return;
        }
        seedEditor.setText (juce::String (owner.getSeed()), juce::dontSendNotification);
        statusLabel.removeColour (juce::Label::textColourId);
        statusLabel.setText ("Generated.", juce::dontSendNotification);
    };

    addAndMakeVisible (randomizeButton);
    randomizeButton.onClick = [this]
    {
        pushGenerationParamsFromWidgets();
        if (! owner.regenerate (true))
        {
            statusLabel.setColour (juce::Label::textColourId, juce::Colours::red);
            statusLabel.setText ("Busy, try again", juce::dontSendNotification);
            return;
        }
        seedEditor.setText (juce::String (owner.getSeed()), juce::dontSendNotification);
        statusLabel.removeColour (juce::Label::textColourId);
        statusLabel.setText ("Randomized.", juce::dontSendNotification);
    };

    addAndMakeVisible (mutateButton);
    mutateButton.onClick = [this]
    {
        if (! owner.mutate())
        {
            statusLabel.setColour (juce::Label::textColourId, juce::Colours::red);
            statusLabel.setText ("Busy, try again", juce::dontSendNotification);
            return;
        }
        statusLabel.removeColour (juce::Label::textColourId);
        statusLabel.setText ("Mutated.", juce::dontSendNotification);
    };

    addAndMakeVisible (lockSeedToggle);
    lockSeedToggle.setToggleState (false, juce::dontSendNotification);
    lockSeedToggle.onClick = [this]
    {
        randomizeButton.setEnabled (! lockSeedToggle.getToggleState());
        pushGenerationParamsFromWidgets();
    };

    // ---- Preset controls ----
    addAndMakeVisible (presetSectionLabel);
    presetSectionLabel.setText ("PRESETS", juce::dontSendNotification);

    addAndMakeVisible (presetNameEditor);
    presetNameEditor.onTextChange = [this]
    {
        savePresetButton.setEnabled (! presetNameEditor.getText().trim().isEmpty());
    };

    addAndMakeVisible (savePresetButton);
    savePresetButton.setEnabled (false);
    savePresetButton.onClick = [this] { savePreset(); };

    addAndMakeVisible (presetBox);
    presetBox.onChange = [this]
    {
        loadPresetButton.setEnabled (presetBox.getSelectedId() != 0);
    };

    addAndMakeVisible (loadPresetButton);
    loadPresetButton.setEnabled (false);
    loadPresetButton.onClick = [this] { loadSelectedPreset(); };

    refreshPresetList();

    // ---- Auto-Evolve controls ----
    addAndMakeVisible (evolutionSectionLabel);
    evolutionSectionLabel.setText ("EVOLUTION", juce::dontSendNotification);

    addAndMakeVisible (autoEvolveToggle);
    autoEvolveToggle.setToggleState (false, juce::dontSendNotification);
    autoEvolveToggle.onClick = [this] { owner.setAutoEvolveEnabled (autoEvolveToggle.getToggleState()); };

    addAndMakeVisible (evolveRateLabel);
    evolveRateLabel.setText ("Every", juce::dontSendNotification);
    evolveRateLabel.setJustificationType (juce::Justification::centredLeft);

    addAndMakeVisible (evolveRateBox);
    evolveRateBox.addItem ("Every 1 loops",  1);
    evolveRateBox.addItem ("Every 2 loops",  2);
    evolveRateBox.addItem ("Every 4 loops",  4);
    evolveRateBox.addItem ("Every 8 loops",  8);
    evolveRateBox.addItem ("Every 16 loops", 16);
    evolveRateBox.setSelectedId (4, juce::dontSendNotification);
    evolveRateBox.onChange = [this] { owner.setAutoEvolveRate (evolveRateBox.getSelectedId()); };
    owner.setAutoEvolveRate (4);

    // ---- Euclidean Rhythms controls (staged, not live - mirrors the seed-field precedent) ----
    addAndMakeVisible (rhythmModeLabel);
    rhythmModeLabel.setText ("Rhythm", juce::dontSendNotification);
    rhythmModeLabel.setJustificationType (juce::Justification::centredLeft);

    addAndMakeVisible (rhythmModeBox);
    rhythmModeBox.addItem ("Random",      static_cast<int> (RhythmMode::random) + 1);
    rhythmModeBox.addItem ("Euclidean",   static_cast<int> (RhythmMode::euclidean) + 1);
    rhythmModeBox.addItem ("Probability", static_cast<int> (RhythmMode::probability) + 1);
    rhythmModeBox.setSelectedId (static_cast<int> (RhythmMode::random) + 1, juce::dontSendNotification);

    addAndMakeVisible (pulsesLabel);
    pulsesLabel.setText ("Pulses", juce::dontSendNotification);
    pulsesLabel.setJustificationType (juce::Justification::centredLeft);

    addAndMakeVisible (pulsesSlider);
    pulsesSlider.setRange (0, kNumSteps, 1);
    pulsesSlider.setValue (kDefaultPulses, juce::dontSendNotification);

    addAndMakeVisible (rotationLabel);
    rotationLabel.setText ("Rotation", juce::dontSendNotification);
    rotationLabel.setJustificationType (juce::Justification::centredLeft);

    addAndMakeVisible (rotationSlider);
    rotationSlider.setRange (0, kNumSteps - 1, 1);
    rotationSlider.setValue (kDefaultRotation, juce::dontSendNotification);

    // ---- Probability Matrices controls (staged, not live) ----
    addAndMakeVisible (stepProbabilityLabel);
    stepProbabilityLabel.setText ("Chance %", juce::dontSendNotification);
    stepProbabilityLabel.setJustificationType (juce::Justification::centredLeft);

    addAndMakeVisible (stepProbabilitySlider);
    stepProbabilitySlider.setRange (0.0, 100.0, 1.0);
    stepProbabilitySlider.setValue (kDefaultStepProbability * 100.0, juce::dontSendNotification);

    // ---- Parameter controls ----
    auto configureSlider = [this] (juce::Slider& slider, juce::Label& label, const juce::String& name,
                                    double min, double max, double initial, double midPoint)
    {
        addAndMakeVisible (slider);
        slider.setRange (min, max);
        if (midPoint > 0.0)
            slider.setSkewFactorFromMidPoint (midPoint);
        slider.setValue (initial, juce::dontSendNotification);

        addAndMakeVisible (label);
        label.setText (name, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centredLeft);
    };

    addAndMakeVisible (oscillatorSectionLabel);
    oscillatorSectionLabel.setText ("OSCILLATOR", juce::dontSendNotification);
    addAndMakeVisible (filterSectionLabel);
    filterSectionLabel.setText ("FILTER", juce::dontSendNotification);
    addAndMakeVisible (envelopeSectionLabel);
    envelopeSectionLabel.setText ("ENVELOPE", juce::dontSendNotification);
    addAndMakeVisible (lfoSectionLabel);
    lfoSectionLabel.setText ("LFO", juce::dontSendNotification);

    addAndMakeVisible (waveformBox);
    waveformBox.addItem ("Saw",      static_cast<int> (berlin::Waveform::saw) + 1);
    waveformBox.addItem ("Square",   static_cast<int> (berlin::Waveform::square) + 1);
    waveformBox.addItem ("Pulse",    static_cast<int> (berlin::Waveform::pulse) + 1);
    waveformBox.addItem ("Triangle", static_cast<int> (berlin::Waveform::triangle) + 1);
    waveformBox.setSelectedId (static_cast<int> (berlin::kDefaultPatch.waveform) + 1, juce::dontSendNotification);
    waveformBox.onChange = [this] { pushPatchFromWidgets(); };
    addAndMakeVisible (waveformLabel);
    waveformLabel.setText ("Waveform", juce::dontSendNotification);
    waveformLabel.setJustificationType (juce::Justification::centredLeft);

    configureSlider (pulseWidthSlider, pulseWidthLabel, "Pulse Width",
                      berlin::kMinPulseWidth, berlin::kMaxPulseWidth, berlin::kDefaultPatch.pulseWidth, 0.0);
    pulseWidthSlider.onValueChange = [this] { pushPatchFromWidgets(); };

    configureSlider (cutoffSlider, cutoffLabel, "Cutoff",
                      berlin::kMinCutoffHz, berlin::kMaxCutoffHz, berlin::kDefaultPatch.cutoffHz, 1000.0);
    cutoffSlider.onValueChange = [this] { pushPatchFromWidgets(); };

    configureSlider (resonanceSlider, resonanceLabel, "Resonance",
                      berlin::kMinResonance, berlin::kMaxResonance, berlin::kDefaultPatch.resonance, 2.0);
    resonanceSlider.onValueChange = [this] { pushPatchFromWidgets(); };

    configureSlider (attackSlider, attackLabel, "Attack",
                      berlin::kMinAttackSeconds, berlin::kMaxAttackSeconds, berlin::kDefaultPatch.attack, 0.2);
    attackSlider.onValueChange = [this] { pushPatchFromWidgets(); };

    configureSlider (decaySlider, decayLabel, "Decay",
                      berlin::kMinDecaySeconds, berlin::kMaxDecaySeconds, berlin::kDefaultPatch.decay, 0.3);
    decaySlider.onValueChange = [this] { pushPatchFromWidgets(); };

    configureSlider (sustainSlider, sustainLabel, "Sustain",
                      berlin::kMinSustain, berlin::kMaxSustain, berlin::kDefaultPatch.sustain, 0.0);
    sustainSlider.onValueChange = [this] { pushPatchFromWidgets(); };

    configureSlider (releaseSlider, releaseLabel, "Release",
                      berlin::kMinReleaseSeconds, berlin::kMaxReleaseSeconds, berlin::kDefaultPatch.release, 0.5);
    releaseSlider.onValueChange = [this] { pushPatchFromWidgets(); };

    addAndMakeVisible (lfoDestinationBox);
    lfoDestinationBox.addItem ("Pitch",       static_cast<int> (berlin::LfoDestination::pitch) + 1);
    lfoDestinationBox.addItem ("Cutoff",      static_cast<int> (berlin::LfoDestination::cutoff) + 1);
    lfoDestinationBox.addItem ("Amplitude",   static_cast<int> (berlin::LfoDestination::amplitude) + 1);
    lfoDestinationBox.addItem ("Pulse Width", static_cast<int> (berlin::LfoDestination::pulseWidth) + 1);
    lfoDestinationBox.setSelectedId (static_cast<int> (berlin::kDefaultPatch.lfoDestination) + 1, juce::dontSendNotification);
    lfoDestinationBox.onChange = [this] { pushPatchFromWidgets(); };
    addAndMakeVisible (lfoDestinationLabel);
    lfoDestinationLabel.setText ("LFO Dest", juce::dontSendNotification);
    lfoDestinationLabel.setJustificationType (juce::Justification::centredLeft);

    configureSlider (lfoRateSlider, lfoRateLabel, "LFO Rate",
                      berlin::kMinLfoRateHz, berlin::kMaxLfoRateHz, berlin::kDefaultPatch.lfoRateHz, 2.0);
    lfoRateSlider.onValueChange = [this] { pushPatchFromWidgets(); };

    configureSlider (lfoDepthSlider, lfoDepthLabel, "LFO Depth",
                      berlin::kMinLfoDepth, berlin::kMaxLfoDepth, berlin::kDefaultPatch.lfoDepth, 0.0);
    lfoDepthSlider.onValueChange = [this] { pushPatchFromWidgets(); };

    refreshFromProcessor();   // pull owner's CURRENT patch/seed (may already differ from kDefaultPatch/kDefaultSeed)
    owner.addChangeListener (this);

    setSize (800, 680);
}

BerlinAudioProcessorEditor::~BerlinAudioProcessorEditor()
{
    owner.removeChangeListener (this);
    exportChooser.reset();
}

//==============================================================================
void BerlinAudioProcessorEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refreshFromProcessor();
}

void BerlinAudioProcessorEditor::refreshFromProcessor()
{
    applyPatchToWidgets (owner.getPatch());
    seedEditor.setText (juce::String (owner.getSeed()), juce::dontSendNotification);
}

//==============================================================================
void BerlinAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void BerlinAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (kMargin);
    exportButton.setBounds (area.removeFromTop (kControlHeight).removeFromLeft (kButtonWidth));
    area.removeFromTop (kMargin / 2);
    statusLabel .setBounds (area.removeFromTop (kControlHeight));
    area.removeFromTop (kMargin / 2);

    auto toggleRow = area.removeFromTop (kControlHeight);
    synthToggle.setBounds (toggleRow.removeFromLeft (kButtonWidth));
    fxToggle   .setBounds (toggleRow.removeFromLeft (kButtonWidth));
    area.removeFromTop (kMargin / 2);

    generationSectionLabel.setBounds (area.removeFromTop (kControlHeight));
    area.removeFromTop (kMargin / 2);

    auto seedRow = area.removeFromTop (kControlHeight);
    seedLabel .setBounds (seedRow.removeFromLeft (kLabelWidth));
    seedEditor.setBounds (seedRow);
    area.removeFromTop (kMargin / 2);

    auto generationButtonRow = area.removeFromTop (kControlHeight);
    generateButton  .setBounds (generationButtonRow.removeFromLeft (kButtonWidth));
    randomizeButton .setBounds (generationButtonRow.removeFromLeft (kButtonWidth));
    lockSeedToggle  .setBounds (generationButtonRow.removeFromLeft (kButtonWidth));
    mutateButton    .setBounds (generationButtonRow.removeFromLeft (kButtonWidth));
    area.removeFromTop (kMargin / 2);

    auto euclideanRow = area.removeFromTop (kControlHeight);
    rhythmModeLabel.setBounds (euclideanRow.removeFromLeft (kLabelWidth));
    rhythmModeBox  .setBounds (euclideanRow.removeFromLeft (kButtonWidth));
    euclideanRow.removeFromLeft (6);
    pulsesLabel    .setBounds (euclideanRow.removeFromLeft (kLabelWidth));
    pulsesSlider   .setBounds (euclideanRow.removeFromLeft (kButtonWidth));
    euclideanRow.removeFromLeft (6);
    rotationLabel  .setBounds (euclideanRow.removeFromLeft (kLabelWidth));
    rotationSlider .setBounds (euclideanRow.removeFromLeft (kButtonWidth));
    area.removeFromTop (kMargin / 2);

    auto probabilityRow = area.removeFromTop (kControlHeight);
    stepProbabilityLabel .setBounds (probabilityRow.removeFromLeft (kLabelWidth));
    stepProbabilitySlider.setBounds (probabilityRow.removeFromLeft (kButtonWidth));
    area.removeFromTop (kMargin / 2);

    auto evolutionRow = area.removeFromTop (kControlHeight);
    evolutionSectionLabel.setBounds (evolutionRow.removeFromLeft (kLabelWidth));
    autoEvolveToggle     .setBounds (evolutionRow.removeFromLeft (kButtonWidth));
    evolutionRow.removeFromLeft (6);
    evolveRateLabel      .setBounds (evolutionRow.removeFromLeft (kLabelWidth));
    evolveRateBox        .setBounds (evolutionRow.removeFromLeft (kButtonWidth));
    area.removeFromTop (kMargin / 2);

    auto presetRow = area.removeFromTop (kControlHeight);
    presetSectionLabel.setBounds (presetRow.removeFromLeft (kLabelWidth));
    presetNameEditor  .setBounds (presetRow.removeFromLeft (180));
    presetRow.removeFromLeft (6);
    savePresetButton  .setBounds (presetRow.removeFromLeft (kButtonWidth));
    presetRow.removeFromLeft (6);
    presetBox         .setBounds (presetRow.removeFromLeft (180));
    presetRow.removeFromLeft (6);
    loadPresetButton  .setBounds (presetRow.removeFromLeft (kButtonWidth));
    area.removeFromTop (kMargin / 2);

    auto placeLabelled = [] (juce::Rectangle<int>& column, juce::Component& label, juce::Component& control)
    {
        auto row = column.removeFromTop (kControlHeight);
        label  .setBounds (row.removeFromLeft (kLabelWidth));
        control.setBounds (row);
        column .removeFromTop (kMargin / 2);
    };

    auto left  = area.removeFromLeft (area.getWidth() / 2 - kMargin / 2);
    auto right = area;

    oscillatorSectionLabel.setBounds (left.removeFromTop (kControlHeight));
    placeLabelled (left, waveformLabel, waveformBox);
    placeLabelled (left, pulseWidthLabel, pulseWidthSlider);

    filterSectionLabel.setBounds (left.removeFromTop (kControlHeight));
    placeLabelled (left, cutoffLabel, cutoffSlider);
    placeLabelled (left, resonanceLabel, resonanceSlider);

    envelopeSectionLabel.setBounds (right.removeFromTop (kControlHeight));
    placeLabelled (right, attackLabel, attackSlider);
    placeLabelled (right, decayLabel, decaySlider);
    placeLabelled (right, sustainLabel, sustainSlider);
    placeLabelled (right, releaseLabel, releaseSlider);

    lfoSectionLabel.setBounds (right.removeFromTop (kControlHeight));
    placeLabelled (right, lfoDestinationLabel, lfoDestinationBox);
    placeLabelled (right, lfoRateLabel, lfoRateSlider);
    placeLabelled (right, lfoDepthLabel, lfoDepthSlider);
}

//==============================================================================
berlin::SynthPatch BerlinAudioProcessorEditor::currentPatchFromWidgets() const
{
    berlin::SynthPatch patch;   // 8 effects fields stay kDefaultPatch - never touched here

    patch.waveform       = static_cast<berlin::Waveform> (waveformBox.getSelectedId() - 1);
    patch.cutoffHz       = (float) cutoffSlider.getValue();
    patch.resonance      = (float) resonanceSlider.getValue();
    patch.pulseWidth     = (float) pulseWidthSlider.getValue();
    patch.attack         = (float) attackSlider.getValue();
    patch.decay          = (float) decaySlider.getValue();
    patch.sustain        = (float) sustainSlider.getValue();
    patch.release        = (float) releaseSlider.getValue();
    patch.lfoRateHz      = (float) lfoRateSlider.getValue();
    patch.lfoDepth       = (float) lfoDepthSlider.getValue();
    patch.lfoDestination = static_cast<berlin::LfoDestination> (lfoDestinationBox.getSelectedId() - 1);

    return patch;
}

void BerlinAudioProcessorEditor::applyPatchToWidgets (const berlin::SynthPatch& patch)
{
    waveformBox.setSelectedId (static_cast<int> (patch.waveform) + 1, juce::dontSendNotification);
    cutoffSlider.setValue (patch.cutoffHz, juce::dontSendNotification);
    resonanceSlider.setValue (patch.resonance, juce::dontSendNotification);
    pulseWidthSlider.setValue (patch.pulseWidth, juce::dontSendNotification);
    attackSlider.setValue (patch.attack, juce::dontSendNotification);
    decaySlider.setValue (patch.decay, juce::dontSendNotification);
    sustainSlider.setValue (patch.sustain, juce::dontSendNotification);
    releaseSlider.setValue (patch.release, juce::dontSendNotification);
    lfoRateSlider.setValue (patch.lfoRateHz, juce::dontSendNotification);
    lfoDepthSlider.setValue (patch.lfoDepth, juce::dontSendNotification);
    lfoDestinationBox.setSelectedId (static_cast<int> (patch.lfoDestination) + 1, juce::dontSendNotification);
}

void BerlinAudioProcessorEditor::pushPatchFromWidgets()
{
    owner.setPatch (currentPatchFromWidgets());
}

void BerlinAudioProcessorEditor::pushGenerationParamsFromWidgets()
{
    berlin::GenerationParams params;
    params.mode            = static_cast<RhythmMode> (rhythmModeBox.getSelectedId() - 1);
    params.pulses          = (int) pulsesSlider.getValue();
    params.rotation        = (int) rotationSlider.getValue();
    params.stepProbability = (float) stepProbabilitySlider.getValue() / 100.0f;   // 0..100 -> 0.0..1.0 exactly
    params.lockSeed        = lockSeedToggle.getToggleState();
    owner.setGenerationParams (params);
}

//==============================================================================
void BerlinAudioProcessorEditor::savePreset()
{
    const juce::String name = presetNameEditor.getText().trim();

    if (name.isEmpty())
    {
        statusLabel.setColour (juce::Label::textColourId, juce::Colours::red);
        statusLabel.setText ("Enter a preset name first.", juce::dontSendNotification);
        return;
    }

    if (owner.presetExists (name))
    {
        savePresetButton.setEnabled (false);   // guard against re-entrant clicks while the dialog is open

        juce::Component::SafePointer<BerlinAudioProcessorEditor> safeThis (this);

        juce::NativeMessageBox::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon,
                                                 "Overwrite Preset?",
                                                 "A preset named \"" + name + "\" already exists. Overwrite it?",
                                                 this,
                                                 juce::ModalCallbackFunction::create ([safeThis, name] (int result)
        {
            if (safeThis == nullptr)
                return;   // editor destroyed while the dialog was open

            if (result != 0)   // AlertWindow-style mapping: OK == 1, Cancel == 0
                safeThis->writePresetFile (name);
            else
                safeThis->savePresetButton.setEnabled (! safeThis->presetNameEditor.getText().trim().isEmpty());
        }));

        return;
    }

    writePresetFile (name);
}

void BerlinAudioProcessorEditor::writePresetFile (const juce::String& name)
{
    const auto result = owner.save (name);
    savePresetButton.setEnabled (! presetNameEditor.getText().trim().isEmpty());

    if (result != berlin::PresetResult::ok)
    {
        statusLabel.setColour (juce::Label::textColourId, juce::Colours::red);
        statusLabel.setText (describePresetFailure (result), juce::dontSendNotification);
        return;
    }

    statusLabel.removeColour (juce::Label::textColourId);
    statusLabel.setText ("Saved \"" + name + "\".", juce::dontSendNotification);
    refreshPresetList (name);
}

void BerlinAudioProcessorEditor::loadSelectedPreset()
{
    const juce::String name = presetBox.getText();
    const auto result = owner.loadPreset (name);

    if (result != berlin::PresetResult::ok)
    {
        statusLabel.setColour (juce::Label::textColourId, juce::Colours::red);
        statusLabel.setText (describePresetFailure (result), juce::dontSendNotification);
        return;   // synth and sequence left untouched
    }

    refreshFromProcessor();
    presetNameEditor.setText (name, juce::dontSendNotification);   // re-saving targets the same preset

    statusLabel.removeColour (juce::Label::textColourId);
    statusLabel.setText ("Loaded \"" + name + "\".", juce::dontSendNotification);
}

void BerlinAudioProcessorEditor::refreshPresetList (const juce::String& nameToSelect)
{
    const auto names = owner.listPresetNames();

    presetBox.clear (juce::dontSendNotification);
    for (int i = 0; i < names.size(); ++i)
        presetBox.addItem (names[i], i + 1);

    const int idToSelect = nameToSelect.isNotEmpty() ? names.indexOf (nameToSelect) + 1 : 0;
    presetBox.setSelectedId (idToSelect, juce::dontSendNotification);
    loadPresetButton.setEnabled (presetBox.getSelectedId() != 0);
}

juce::String BerlinAudioProcessorEditor::describePresetFailure (berlin::PresetResult result) const
{
    switch (result)
    {
        case berlin::PresetResult::nameInvalid:
            return "Preset name is invalid.";
        case berlin::PresetResult::directoryUnavailable:
            return "Preset folder unavailable.";
        case berlin::PresetResult::writeFailed:
            return "Could not write the preset file.";
        case berlin::PresetResult::fileNotFound:
            return "Preset not found.";
        case berlin::PresetResult::parseFailed:
            return "Preset file is invalid or corrupted.";
        case berlin::PresetResult::unsupportedVersion:
            return "Preset was saved by a newer version of Berlin.";
        case berlin::PresetResult::ok:
        default:
            return {};
    }
}

//==============================================================================
void BerlinAudioProcessorEditor::launchExportChooser()
{
    exportButton.setEnabled (false);   // guard against re-entrant clicks while the dialog is open

    exportChooser = std::make_unique<juce::FileChooser> ("Export MIDI...",
                                                          defaultExportFile(),
                                                          "*.mid");

    const int chooserFlags = juce::FileBrowserComponent::saveMode
                            | juce::FileBrowserComponent::canSelectFiles
                            | juce::FileBrowserComponent::warnAboutOverwriting;

    exportChooser->launchAsync (chooserFlags, [this] (const juce::FileChooser& chooser)
    {
        const juce::File destination = chooser.getResult();

        if (destination != juce::File())   // cancelled: silent no-op, NOT a failure
            exportSequenceTo (destination);

        exportButton.setEnabled (true);
    });
}

void BerlinAudioProcessorEditor::exportSequenceTo (const juce::File& destination)
{
    const auto writeResult = owner.exportMidiTo (destination);

    if (writeResult != berlin::MidiFileWriteResult::ok)
    {
        const juce::String message = describeWriteFailure (writeResult);
        statusLabel.setColour (juce::Label::textColourId, juce::Colours::red);
        statusLabel.setText (message, juce::dontSendNotification);
        juce::Logger::writeToLog ("Berlin: MIDI export failed, result = " + juce::String ((int) writeResult));
        juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                     "MIDI Export Failed", message, this);
        return;
    }

    statusLabel.removeColour (juce::Label::textColourId);
    statusLabel.setText ("Exported to " + destination.getFileName(), juce::dontSendNotification);
}

juce::String BerlinAudioProcessorEditor::describeWriteFailure (berlin::MidiFileWriteResult result) const
{
    switch (result)
    {
        case berlin::MidiFileWriteResult::invalidTimeline:
            return "Export failed: the timeline was invalid.";
        case berlin::MidiFileWriteResult::pathUnavailable:
            return "Export failed: destination folder unavailable.";
        case berlin::MidiFileWriteResult::writeFailed:
            return "Export failed: could not write the file.";
        case berlin::MidiFileWriteResult::ok:
        default:
            return {};
    }
}

} // namespace berlin
