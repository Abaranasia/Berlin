/*
  ==============================================================================

   BerlinAudioProcessorEditor - out-of-line definitions (roadmap Phase 11 /
   vst3-au-plugin, plugin-host-integration spec, design.md D1-D3).

  ==============================================================================
*/

#include "BerlinAudioProcessorEditor.h"

#include "core/TempoSync.h"
#include "generation/SequenceBuilder.h"   // normalizePitchRange - interactive range-slider constraint

namespace
{
    // Widget-seeding defaults for pulses/rotation/stepProbability come from a
    // default-constructed GenerationParams rather than redeclared constants
    // (vst3-au-plugin followup-fixes cleanup) - GenerationParams.h's own
    // default member initializers are the single source of truth; this local
    // instance exists only so widget setup below stays a one-line read.
    const berlin::GenerationParams kDefaultGenerationParams {};

    constexpr int kMargin = 12, kControlHeight = 28, kButtonWidth = 140, kLabelWidth = 96;

    const char* kExportFileName = "berlin-export.mid";

    // scale-aware-generation: UI display names. Order matches
    // berlin::ScaleType's declaration order / PresetManager::scaleNames().
    const char* const kScaleNames[] = { "Minor", "Major", "Dorian", "Phrygian", "Mixolydian", "Harmonic Minor" };
    const char* const kRootNames[]  = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    // tempo-delay-ui Phase 10: UI display names for berlin::SyncDivision.
    // Order MUST match SyncDivision's declaration order (TempoSync.h) - the
    // SAME "name string, not a raw ordinal" precedent as kScaleNames/
    // waveformNames() (PresetManager.cpp's divisionNames(), Phase 11).
    const char* const kDivisionNames[] = { "1/2", "1/4", "1/8.", "1/8", "1/8T", "1/16" };

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
    fxToggle.onClick = [this]
    {
        owner.setEffectsEnabled (fxToggle.getToggleState());
        updateDelayReverbEnablement();   // D8: delay/reverb section greys out while FX is off
    };

    // ---- Tempo control (tempo-control spec, Phase 5) ----
    addAndMakeVisible (tempoSectionLabel);
    tempoSectionLabel.setText ("TEMPO", juce::dontSendNotification);

    addAndMakeVisible (tempoLabel);
    tempoLabel.setText ("BPM", juce::dontSendNotification);
    tempoLabel.setJustificationType (juce::Justification::centredLeft);

    addAndMakeVisible (tempoSlider);
    tempoSlider.setRange (berlin::kMinBpm, berlin::kMaxBpm, 0.1);
    tempoSlider.setValue (owner.getBpm(), juce::dontSendNotification);
    tempoSlider.onValueChange = [this] { pushTempoFromWidgets(); };

    // ---- Delay/reverb + tempo-sync (tempo-control, internal-synth-output
    // specs, Phase 10) ----
    addAndMakeVisible (delaySectionLabel);
    delaySectionLabel.setText ("DELAY", juce::dontSendNotification);

    addAndMakeVisible (delaySyncToggle);
    delaySyncToggle.onClick = [this] { recomputeSyncedDelayTime(); };

    addAndMakeVisible (delayDivisionLabel);
    delayDivisionLabel.setText ("Division", juce::dontSendNotification);
    delayDivisionLabel.setJustificationType (juce::Justification::centredLeft);

    addAndMakeVisible (delayDivisionBox);
    for (int i = 0; i < (int) (sizeof (kDivisionNames) / sizeof (kDivisionNames[0])); ++i)
        delayDivisionBox.addItem (kDivisionNames[i], i + 1);
    delayDivisionBox.setSelectedId (static_cast<int> (berlin::kDefaultPatch.delayDivision) + 1, juce::dontSendNotification);
    delayDivisionBox.onChange = [this] { recomputeSyncedDelayTime(); };

    auto configureSliderEarly = [this] (juce::Slider& slider, juce::Label& label, const juce::String& name,
                                         double min, double max, double initial)
    {
        addAndMakeVisible (slider);
        slider.setRange (min, max);
        slider.setValue (initial, juce::dontSendNotification);

        addAndMakeVisible (label);
        label.setText (name, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centredLeft);
    };

    configureSliderEarly (delayTimeSlider, delayTimeLabel, "Time",
                           berlin::kMinDelayTimeSeconds, berlin::kMaxDelaySeconds, berlin::kDefaultPatch.delayTimeSeconds);
    delayTimeSlider.onValueChange = [this]
    {
        if (! delaySyncToggle.getToggleState())
            lastManualDelayTimeSeconds = delayTimeSlider.getValue();
        pushPatchFromWidgets();
    };

    configureSliderEarly (delayFeedbackSlider, delayFeedbackLabel, "Feedback",
                           berlin::kMinDelayFeedback, berlin::kMaxDelayFeedback, berlin::kDefaultPatch.delayFeedback);
    delayFeedbackSlider.onValueChange = [this] { pushPatchFromWidgets(); };

    configureSliderEarly (delayMixSlider, delayMixLabel, "Mix",
                           berlin::kMinDelayMix, berlin::kMaxDelayMix, berlin::kDefaultPatch.delayMix);
    delayMixSlider.onValueChange = [this] { pushPatchFromWidgets(); };

    addAndMakeVisible (reverbSectionLabel);
    reverbSectionLabel.setText ("REVERB", juce::dontSendNotification);

    configureSliderEarly (reverbRoomSlider, reverbRoomLabel, "Room",
                           berlin::kMinReverbRoomSize, berlin::kMaxReverbRoomSize, berlin::kDefaultPatch.reverbRoomSize);
    reverbRoomSlider.onValueChange = [this] { pushPatchFromWidgets(); };

    configureSliderEarly (reverbDampingSlider, reverbDampingLabel, "Damping",
                           berlin::kMinReverbDamping, berlin::kMaxReverbDamping, berlin::kDefaultPatch.reverbDamping);
    reverbDampingSlider.onValueChange = [this] { pushPatchFromWidgets(); };

    configureSliderEarly (reverbWetSlider, reverbWetLabel, "Wet",
                           berlin::kMinReverbWetLevel, berlin::kMaxReverbWetLevel, berlin::kDefaultPatch.reverbWetLevel);
    reverbWetSlider.onValueChange = [this] { pushPatchFromWidgets(); };

    configureSliderEarly (reverbDrySlider, reverbDryLabel, "Dry",
                           berlin::kMinReverbDryLevel, berlin::kMaxReverbDryLevel, berlin::kDefaultPatch.reverbDryLevel);
    reverbDrySlider.onValueChange = [this] { pushPatchFromWidgets(); };

    // D8: the whole delay/reverb section greys out while fxToggle is off -
    // populated once here, applied by updateDelayReverbEnablement().
    delayReverbWidgets = { &delaySyncToggle, &delayDivisionBox, &delayTimeSlider, &delayFeedbackSlider, &delayMixSlider,
                           &reverbRoomSlider, &reverbDampingSlider, &reverbWetSlider, &reverbDrySlider };
    updateDelayReverbEnablement();

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
    pulsesSlider.setRange (0, berlin::kNumSteps, 1);
    pulsesSlider.setValue (kDefaultGenerationParams.pulses, juce::dontSendNotification);

    addAndMakeVisible (rotationLabel);
    rotationLabel.setText ("Rotation", juce::dontSendNotification);
    rotationLabel.setJustificationType (juce::Justification::centredLeft);

    addAndMakeVisible (rotationSlider);
    rotationSlider.setRange (0, berlin::kNumSteps - 1, 1);
    rotationSlider.setValue (kDefaultGenerationParams.rotation, juce::dontSendNotification);

    // ---- Probability Matrices controls (staged, not live) ----
    addAndMakeVisible (stepProbabilityLabel);
    stepProbabilityLabel.setText ("Chance %", juce::dontSendNotification);
    stepProbabilityLabel.setJustificationType (juce::Justification::centredLeft);

    addAndMakeVisible (stepProbabilitySlider);
    stepProbabilitySlider.setRange (0.0, 100.0, 1.0);
    stepProbabilitySlider.setValue (kDefaultGenerationParams.stepProbability * 100.0, juce::dontSendNotification);

    // ---- Scale-Aware Generation controls (staged, not live - mirrors the
    // Pulses/Rotation precedent) ----
    addAndMakeVisible (scaleLabel);
    scaleLabel.setText ("Scale", juce::dontSendNotification);
    scaleLabel.setJustificationType (juce::Justification::centredLeft);

    addAndMakeVisible (scaleBox);
    for (int i = 0; i < (int) (sizeof (kScaleNames) / sizeof (kScaleNames[0])); ++i)
        scaleBox.addItem (kScaleNames[i], i + 1);
    scaleBox.setSelectedId (static_cast<int> (kDefaultGenerationParams.scaleType) + 1, juce::dontSendNotification);

    addAndMakeVisible (rootLabel);
    rootLabel.setText ("Root", juce::dontSendNotification);
    rootLabel.setJustificationType (juce::Justification::centredLeft);

    addAndMakeVisible (rootBox);
    for (int i = 0; i < (int) (sizeof (kRootNames) / sizeof (kRootNames[0])); ++i)
        rootBox.addItem (kRootNames[i], i + 1);
    rootBox.setSelectedId (kDefaultGenerationParams.rootPitchClass + 1, juce::dontSendNotification);

    // Interactive constraint (design.md): normalizePitchRange runs again here
    // so the two sliders visually reflect the >= one-octave invariant while
    // dragging - buildSeededSequence's chokepoint enforces it regardless, so
    // this is UX feedback, not a correctness dependency.
    auto constrainRangeSliders = [this]
    {
        int low  = (int) rangeLowSlider.getValue();
        int high = (int) rangeHighSlider.getValue();
        berlin::normalizePitchRange (low, high);
        rangeLowSlider.setValue (low, juce::dontSendNotification);
        rangeHighSlider.setValue (high, juce::dontSendNotification);
    };

    addAndMakeVisible (rangeLowLabel);
    rangeLowLabel.setText ("Range Lo", juce::dontSendNotification);
    rangeLowLabel.setJustificationType (juce::Justification::centredLeft);

    addAndMakeVisible (rangeLowSlider);
    rangeLowSlider.setRange (berlin::kMinPitch, berlin::kMaxPitch, 1);
    rangeLowSlider.setValue (kDefaultGenerationParams.rangeLow, juce::dontSendNotification);
    rangeLowSlider.onValueChange = constrainRangeSliders;

    addAndMakeVisible (rangeHighLabel);
    rangeHighLabel.setText ("Range Hi", juce::dontSendNotification);
    rangeHighLabel.setJustificationType (juce::Justification::centredLeft);

    addAndMakeVisible (rangeHighSlider);
    rangeHighSlider.setRange (berlin::kMinPitch, berlin::kMaxPitch, 1);
    rangeHighSlider.setValue (kDefaultGenerationParams.rangeHigh, juce::dontSendNotification);
    rangeHighSlider.onValueChange = constrainRangeSliders;

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

    // +68px = 2 new rows (scale/root, range lo/hi), each kControlHeight (28) +
    // kMargin/2 (6) spacing - scale-aware-generation, resolving design.md's
    // open editor-height question against this file's live layout constants.
    // +1 more row (kControlHeight + kMargin/2) for the new TEMPO section
    // (tempo-delay-ui Phase 5). +4 more rows (delay row 1, delay row 2,
    // reverb row 1, reverb row 2) for the DELAY/REVERB sections (Phase 10).
    setSize (800, 680 + 7 * (kControlHeight + kMargin / 2));
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
    // BPM MUST be applied before applyPatchToWidgets(): applyPatchToWidgets
    // sets delayTimeSlider directly from the loaded patch's delayTimeSeconds
    // (not recomputed from BPM), so ordering here does not race Sync mode -
    // but tempoSlider itself must reflect owner's CURRENT BPM first so any
    // later live Sync recompute (via the tempo slider's own onValueChange)
    // starts from the right value.
    tempoSlider.setValue (owner.getBpm(), juce::dontSendNotification);
    applyPatchToWidgets (owner.getPatch());
    seedEditor.setText (juce::String (owner.getSeed()), juce::dontSendNotification);
    applyGenerationParamsToWidgets (owner.getGenerationParams());
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

    auto tempoRow = area.removeFromTop (kControlHeight);
    tempoSectionLabel.setBounds (tempoRow.removeFromLeft (kLabelWidth));
    tempoLabel       .setBounds (tempoRow.removeFromLeft (kLabelWidth));
    tempoSlider      .setBounds (tempoRow.removeFromLeft (kButtonWidth));
    area.removeFromTop (kMargin / 2);

    auto delayRow1 = area.removeFromTop (kControlHeight);
    delaySectionLabel .setBounds (delayRow1.removeFromLeft (kLabelWidth));
    delaySyncToggle   .setBounds (delayRow1.removeFromLeft (kLabelWidth));
    delayRow1.removeFromLeft (6);
    delayDivisionLabel.setBounds (delayRow1.removeFromLeft (kLabelWidth));
    delayDivisionBox  .setBounds (delayRow1.removeFromLeft (kButtonWidth));
    area.removeFromTop (kMargin / 2);

    auto delayRow2 = area.removeFromTop (kControlHeight);
    delayTimeLabel      .setBounds (delayRow2.removeFromLeft (kLabelWidth));
    delayTimeSlider     .setBounds (delayRow2.removeFromLeft (kButtonWidth));
    delayRow2.removeFromLeft (6);
    delayFeedbackLabel  .setBounds (delayRow2.removeFromLeft (kLabelWidth));
    delayFeedbackSlider .setBounds (delayRow2.removeFromLeft (kButtonWidth));
    delayRow2.removeFromLeft (6);
    delayMixLabel       .setBounds (delayRow2.removeFromLeft (kLabelWidth));
    delayMixSlider      .setBounds (delayRow2.removeFromLeft (kButtonWidth));
    area.removeFromTop (kMargin / 2);

    auto reverbRow1 = area.removeFromTop (kControlHeight);
    reverbSectionLabel.setBounds (reverbRow1.removeFromLeft (kLabelWidth));
    reverbRoomLabel   .setBounds (reverbRow1.removeFromLeft (kLabelWidth));
    reverbRoomSlider  .setBounds (reverbRow1.removeFromLeft (kButtonWidth));
    reverbRow1.removeFromLeft (6);
    reverbDampingLabel.setBounds (reverbRow1.removeFromLeft (kLabelWidth));
    reverbDampingSlider.setBounds (reverbRow1.removeFromLeft (kButtonWidth));
    area.removeFromTop (kMargin / 2);

    auto reverbRow2 = area.removeFromTop (kControlHeight);
    reverbWetLabel .setBounds (reverbRow2.removeFromLeft (kLabelWidth));
    reverbWetSlider.setBounds (reverbRow2.removeFromLeft (kButtonWidth));
    reverbRow2.removeFromLeft (6);
    reverbDryLabel .setBounds (reverbRow2.removeFromLeft (kLabelWidth));
    reverbDrySlider.setBounds (reverbRow2.removeFromLeft (kButtonWidth));
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

    auto scaleRootRow = area.removeFromTop (kControlHeight);
    scaleLabel.setBounds (scaleRootRow.removeFromLeft (kLabelWidth));
    scaleBox  .setBounds (scaleRootRow.removeFromLeft (kButtonWidth));
    scaleRootRow.removeFromLeft (6);
    rootLabel .setBounds (scaleRootRow.removeFromLeft (kLabelWidth));
    rootBox   .setBounds (scaleRootRow.removeFromLeft (kButtonWidth));
    area.removeFromTop (kMargin / 2);

    auto rangeRow = area.removeFromTop (kControlHeight);
    rangeLowLabel  .setBounds (rangeRow.removeFromLeft (kLabelWidth));
    rangeLowSlider .setBounds (rangeRow.removeFromLeft (kButtonWidth));
    rangeRow.removeFromLeft (6);
    rangeHighLabel .setBounds (rangeRow.removeFromLeft (kLabelWidth));
    rangeHighSlider.setBounds (rangeRow.removeFromLeft (kButtonWidth));
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
    berlin::SynthPatch patch;   // outputLevel has no widget in this slice - stays kDefaultPatch, same
                                // precedent as every other never-wired field before Phase 10.

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

    // ---- Delay/reverb + tempo-sync (Phase 10) ----
    patch.delayTimeSeconds = (float) delayTimeSlider.getValue();
    patch.delayFeedback    = (float) delayFeedbackSlider.getValue();
    patch.delayMix         = (float) delayMixSlider.getValue();
    patch.reverbRoomSize   = (float) reverbRoomSlider.getValue();
    patch.reverbDamping    = (float) reverbDampingSlider.getValue();
    patch.reverbWetLevel   = (float) reverbWetSlider.getValue();
    patch.reverbDryLevel   = (float) reverbDrySlider.getValue();
    patch.delaySynced      = delaySyncToggle.getToggleState();
    patch.delayDivision    = static_cast<berlin::SyncDivision> (delayDivisionBox.getSelectedId() - 1);

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

    // ---- Delay/reverb + tempo-sync (Phase 10): reflect owner's state into
    // widgets WITHOUT pushing back (no recomputeSyncedDelayTime()/
    // pushPatchFromWidgets() call here - this is a pure read-back, avoiding a
    // refresh -> push -> refresh loop). ----
    delayFeedbackSlider.setValue (patch.delayFeedback, juce::dontSendNotification);
    delayMixSlider.setValue (patch.delayMix, juce::dontSendNotification);
    reverbRoomSlider.setValue (patch.reverbRoomSize, juce::dontSendNotification);
    reverbDampingSlider.setValue (patch.reverbDamping, juce::dontSendNotification);
    reverbWetSlider.setValue (patch.reverbWetLevel, juce::dontSendNotification);
    reverbDrySlider.setValue (patch.reverbDryLevel, juce::dontSendNotification);
    delaySyncToggle.setToggleState (patch.delaySynced, juce::dontSendNotification);
    delayDivisionBox.setSelectedId (static_cast<int> (patch.delayDivision) + 1, juce::dontSendNotification);
    delayTimeSlider.setValue (patch.delayTimeSeconds, juce::dontSendNotification);

    if (! patch.delaySynced)
        lastManualDelayTimeSeconds = patch.delayTimeSeconds;   // seed Free-mode memory from the loaded value

    delayTimeSlider.setEnabled (fxToggle.getToggleState() && ! patch.delaySynced);
}

void BerlinAudioProcessorEditor::pushPatchFromWidgets()
{
    owner.setPatch (currentPatchFromWidgets());
}

void BerlinAudioProcessorEditor::pushTempoFromWidgets()
{
    owner.setBpm (tempoSlider.getValue());
    recomputeSyncedDelayTime();   // Sync mode tracks the live BPM (Phase 10)
}

void BerlinAudioProcessorEditor::recomputeSyncedDelayTime()
{
    if (delaySyncToggle.getToggleState())
    {
        const auto division = static_cast<berlin::SyncDivision> (delayDivisionBox.getSelectedId() - 1);
        const double seconds = berlin::delaySecondsFor (owner.getBpm(), division);
        delayTimeSlider.setValue (seconds, juce::dontSendNotification);
    }
    else
    {
        // Free mode: restore the last manually-entered value (design.md's
        // "retains last manual value across Sync-Free-Sync").
        delayTimeSlider.setValue (lastManualDelayTimeSeconds, juce::dontSendNotification);
    }

    updateDelayReverbEnablement();
    pushPatchFromWidgets();
}

void BerlinAudioProcessorEditor::updateDelayReverbEnablement()
{
    const bool fxOn = fxToggle.getToggleState();

    for (auto* widget : delayReverbWidgets)
        widget->setEnabled (fxOn);

    // D8's extra rule, on top of the fxOn gate above: the time slider is also
    // disabled while Sync mode drives it (manual entry blocked in Sync).
    delayTimeSlider.setEnabled (fxOn && ! delaySyncToggle.getToggleState());
}

void BerlinAudioProcessorEditor::pushGenerationParamsFromWidgets()
{
    berlin::GenerationParams params;
    params.mode            = static_cast<RhythmMode> (rhythmModeBox.getSelectedId() - 1);
    params.pulses          = (int) pulsesSlider.getValue();
    params.rotation        = (int) rotationSlider.getValue();
    params.stepProbability = (float) stepProbabilitySlider.getValue() / 100.0f;   // 0..100 -> 0.0..1.0 exactly
    params.lockSeed        = lockSeedToggle.getToggleState();

    params.scaleType      = static_cast<berlin::ScaleType> (scaleBox.getSelectedId() - 1);
    params.rootPitchClass = rootBox.getSelectedId() - 1;
    params.rangeLow       = (int) rangeLowSlider.getValue();
    params.rangeHigh      = (int) rangeHighSlider.getValue();

    owner.setGenerationParams (params);
}

void BerlinAudioProcessorEditor::applyGenerationParamsToWidgets (const berlin::GenerationParams& params)
{
    // Writes back ONLY the 4 persisted fields (design.md's data-flow diagram)
    // - the rhythm widgets (mode/pulses/rotation/stepProbability/lockSeed)
    // stay the staging source of truth and are never overwritten here.
    scaleBox.setSelectedId (static_cast<int> (params.scaleType) + 1, juce::dontSendNotification);
    rootBox.setSelectedId (params.rootPitchClass + 1, juce::dontSendNotification);
    rangeLowSlider.setValue (params.rangeLow, juce::dontSendNotification);
    rangeHighSlider.setValue (params.rangeHigh, juce::dontSendNotification);
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
