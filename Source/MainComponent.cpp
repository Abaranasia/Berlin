#include "MainComponent.h"

#include "core/Scale.h"
#include "generation/DeterministicRandom.h"
#include "generation/EuclideanRhythmGenerator.h"
#include "generation/MutationEngine.h"
#include "generation/PitchGenerator.h"
#include "generation/SkipMaskGenerator.h"

namespace
{
    constexpr double kBpm         = 120.0;
    constexpr int    kStepsPerBeat = 4;
    constexpr int    kNumSteps     = 16;
    constexpr int    kActiveSteps  = 11;   // research's cited 16 -> 11 displacement (design.md Decision 3)
    constexpr int    kDefaultPulses   = 5;   // roadmap Phase 10 / euclidean-rhythm, design.md Decision 14
    constexpr int    kDefaultRotation = 0;
    constexpr int    kSeed         = 12345;
    constexpr int    kMidiChannel     = 1;
    constexpr int    kMidiBufferBytes = 1024;
    constexpr int    kExportRepeats   = 4;   // 4 x 16 steps @ 16ths = 16 beats = exactly 4 bars of 4/4
    const char*      kExportFileName  = "berlin-export.mid";

    constexpr int kMargin = 12, kControlHeight = 28, kButtonWidth = 140, kLabelWidth = 96;

    juce::String describeWriteFailure (berlin::MidiFileWriteResult result)
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
}

berlin::Sequence MainComponent::buildSeededSequence (juce::int64 seed, RhythmMode mode, int pulses, int rotation)
{
    berlin::DeterministicRandom rng (seed);
    auto sequence = (mode == RhythmMode::euclidean)
        ? berlin::EuclideanRhythmGenerator (kNumSteps, pulses, rotation).generate()  // 0 draws
        : berlin::SkipMaskGenerator (kNumSteps, kActiveSteps).generate (rng);        // exactly 5 draws (design.md V1)

    berlin::PitchGenerator pitch (berlin::Scale::minor (48), 36, 72);
    for (int i = 0; i < sequence.size(); ++i)
    {
        if (sequence[i].active)
            sequence[i].note = pitch.generateNextNote (rng);
    }

    return sequence;
}

juce::File MainComponent::defaultExportFile()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("Berlin")
        .getChildFile (kExportFileName);
}

//==============================================================================
MainComponent::MainComponent()
    : currentSeed (kSeed),
      currentSequence (buildSeededSequence (currentSeed, RhythmMode::random, kDefaultPulses, kDefaultRotation)),
      sequenceSeed (kSeed),
      player (currentSequence, berlin::Transport (kBpm, kStepsPerBeat)),
      midiTranslator (kMidiChannel), midiSink (kMidiChannel)
{
    addAndMakeVisible (exportButton);
    addAndMakeVisible (statusLabel);
    exportButton.onClick = [this] { launchExportChooser(); };

    addAndMakeVisible (synthToggle);
    synthToggle.setToggleState (true, juce::dontSendNotification);   // matches the atomic's default (Requirement: default enabled)
    synthToggle.onClick = [this] { synth.setEnabled (synthToggle.getToggleState()); };

    addAndMakeVisible (fxToggle);
    // Default off, consistent with SynthEngine::effectsEnabled's atomic default -
    // deliberately no setToggleState(true, ...) here (internal-synth-output:
    // Terminal Delay And Reverb, Bypassed By Default).
    fxToggle.onClick = [this] { synth.setEffectsEnabled (fxToggle.getToggleState()); };

    // ---- Generation controls (roadmap Phase 10 / generation-randomize) ----
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
            currentSeed = text.getLargeIntValue();
        }
        else
        {
            // Reject: restore the previous seed's text, report why (Decision 4's
            // validation contract). Editing alone never regenerates either way.
            seedEditor.setText (juce::String (currentSeed), juce::dontSendNotification);
            statusLabel.setColour (juce::Label::textColourId, juce::Colours::red);
            statusLabel.setText ("Seed must be a whole number.", juce::dontSendNotification);
        }
    };

    addAndMakeVisible (seedLabel);
    seedLabel.setText ("Seed", juce::dontSendNotification);
    seedLabel.setJustificationType (juce::Justification::centredLeft);

    addAndMakeVisible (seedEditor);
    seedEditor.setText (juce::String (currentSeed), juce::dontSendNotification);
    seedEditor.onFocusLost = validateSeedField;
    seedEditor.onReturnKey = validateSeedField;

    addAndMakeVisible (generateButton);
    generateButton.onClick = [this] { regenerate (false); };

    addAndMakeVisible (randomizeButton);
    randomizeButton.onClick = [this] { regenerate (true); };

    addAndMakeVisible (mutateButton);
    mutateButton.onClick = [this] { mutate(); };

    addAndMakeVisible (lockSeedToggle);
    lockSeedToggle.setToggleState (false, juce::dontSendNotification);   // default off
    lockSeedToggle.onClick = [this] { randomizeButton.setEnabled (! lockSeedToggle.getToggleState()); };

    // ---- Preset controls (roadmap Phase 11 / preset-system) ----
    addAndMakeVisible (presetSectionLabel);
    presetSectionLabel.setText ("PRESETS", juce::dontSendNotification);

    addAndMakeVisible (presetNameEditor);
    presetNameEditor.onTextChange = [this]
    {
        // Decision 6: Save disabled while the name sanitizes to empty.
        savePresetButton.setEnabled (! presetNameEditor.getText().trim().isEmpty());
    };

    addAndMakeVisible (savePresetButton);
    savePresetButton.setEnabled (false);   // starts empty
    savePresetButton.onClick = [this] { savePreset(); };

    addAndMakeVisible (presetBox);
    presetBox.onChange = [this]
    {
        // Decision 6: browsing is non-destructive - selecting never auto-loads.
        loadPresetButton.setEnabled (presetBox.getSelectedId() != 0);
    };

    addAndMakeVisible (loadPresetButton);
    loadPresetButton.setEnabled (false);   // Decision 6: disabled while nothing is selected
    loadPresetButton.onClick = [this] { loadSelectedPreset(); };

    refreshPresetList();

    // ---- Auto-Evolve controls (roadmap Phase 9 "Evolution", Slice 2 /
    // auto-evolution spec) ----
    addAndMakeVisible (evolutionSectionLabel);
    evolutionSectionLabel.setText ("EVOLUTION", juce::dontSendNotification);

    addAndMakeVisible (autoEvolveToggle);
    autoEvolveToggle.setToggleState (false, juce::dontSendNotification);   // default off (byte-identical when off)
    autoEvolveToggle.onClick = [this]
    {
        if (autoEvolveToggle.getToggleState())
        {
            lastMutationLoopCount = player.getLoopCount();
            dueAtLoopCount = -1;
            startTimerHz (60);
        }
        else
        {
            stopTimer();
            dueAtLoopCount = -1;
        }
    };

    addAndMakeVisible (evolveRateLabel);
    evolveRateLabel.setText ("Every", juce::dontSendNotification);
    evolveRateLabel.setJustificationType (juce::Justification::centredLeft);

    addAndMakeVisible (evolveRateBox);
    evolveRateBox.addItem ("Every 1 loops",  1);
    evolveRateBox.addItem ("Every 2 loops",  2);
    evolveRateBox.addItem ("Every 4 loops",  4);
    evolveRateBox.addItem ("Every 8 loops",  8);
    evolveRateBox.addItem ("Every 16 loops", 16);
    evolveRateBox.setSelectedId (4, juce::dontSendNotification);   // default rate: every 4 loops

    // ---- Euclidean Rhythms controls (roadmap Phase 10 / euclidean-rhythm,
    // design.md Decision 10) - STAGED, not live: no onChange/onValueChange
    // here calls regenerate(). Changing mode/pulses/rotation does nothing
    // until Generate or Randomize is pressed (mirrors the seed-field
    // precedent, avoids flooding the status label during a slider drag).
    addAndMakeVisible (rhythmModeLabel);
    rhythmModeLabel.setText ("Rhythm", juce::dontSendNotification);
    rhythmModeLabel.setJustificationType (juce::Justification::centredLeft);

    addAndMakeVisible (rhythmModeBox);
    rhythmModeBox.addItem ("Random",    static_cast<int> (RhythmMode::random) + 1);
    rhythmModeBox.addItem ("Euclidean", static_cast<int> (RhythmMode::euclidean) + 1);
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

    // ---- Parameter controls (roadmap Phase 9 / parameter-controls) ----
    // Every control must be constructed and valued here, BEFORE setAudioChannels()
    // below - it can invoke prepareToPlay synchronously (design.md Decision 6's
    // hard ordering constraint).
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
    waveformBox.onChange = [this]
    {
        synth.setWaveform (static_cast<berlin::Waveform> (waveformBox.getSelectedId() - 1));
    };
    addAndMakeVisible (waveformLabel);
    waveformLabel.setText ("Waveform", juce::dontSendNotification);
    waveformLabel.setJustificationType (juce::Justification::centredLeft);

    configureSlider (pulseWidthSlider, pulseWidthLabel, "Pulse Width",
                      berlin::kMinPulseWidth, berlin::kMaxPulseWidth, berlin::kDefaultPatch.pulseWidth, 0.0);
    pulseWidthSlider.onValueChange = [this] { synth.setPulseWidth ((float) pulseWidthSlider.getValue()); };

    configureSlider (cutoffSlider, cutoffLabel, "Cutoff",
                      berlin::kMinCutoffHz, berlin::kMaxCutoffHz, berlin::kDefaultPatch.cutoffHz, 1000.0);
    cutoffSlider.onValueChange = [this] { synth.setCutoffHz ((float) cutoffSlider.getValue()); };

    configureSlider (resonanceSlider, resonanceLabel, "Resonance",
                      berlin::kMinResonance, berlin::kMaxResonance, berlin::kDefaultPatch.resonance, 2.0);
    resonanceSlider.onValueChange = [this] { synth.setResonance ((float) resonanceSlider.getValue()); };

    configureSlider (attackSlider, attackLabel, "Attack",
                      berlin::kMinAttackSeconds, berlin::kMaxAttackSeconds, berlin::kDefaultPatch.attack, 0.2);
    attackSlider.onValueChange = [this] { synth.setAttackSeconds ((float) attackSlider.getValue()); };

    configureSlider (decaySlider, decayLabel, "Decay",
                      berlin::kMinDecaySeconds, berlin::kMaxDecaySeconds, berlin::kDefaultPatch.decay, 0.3);
    decaySlider.onValueChange = [this] { synth.setDecaySeconds ((float) decaySlider.getValue()); };

    configureSlider (sustainSlider, sustainLabel, "Sustain",
                      berlin::kMinSustain, berlin::kMaxSustain, berlin::kDefaultPatch.sustain, 0.0);
    sustainSlider.onValueChange = [this] { synth.setSustain ((float) sustainSlider.getValue()); };

    configureSlider (releaseSlider, releaseLabel, "Release",
                      berlin::kMinReleaseSeconds, berlin::kMaxReleaseSeconds, berlin::kDefaultPatch.release, 0.5);
    releaseSlider.onValueChange = [this] { synth.setReleaseSeconds ((float) releaseSlider.getValue()); };

    addAndMakeVisible (lfoDestinationBox);
    lfoDestinationBox.addItem ("Pitch",       static_cast<int> (berlin::LfoDestination::pitch) + 1);
    lfoDestinationBox.addItem ("Cutoff",      static_cast<int> (berlin::LfoDestination::cutoff) + 1);
    lfoDestinationBox.addItem ("Amplitude",   static_cast<int> (berlin::LfoDestination::amplitude) + 1);
    lfoDestinationBox.addItem ("Pulse Width", static_cast<int> (berlin::LfoDestination::pulseWidth) + 1);
    lfoDestinationBox.setSelectedId (static_cast<int> (berlin::kDefaultPatch.lfoDestination) + 1, juce::dontSendNotification);
    lfoDestinationBox.onChange = [this]
    {
        synth.setLfoDestination (static_cast<berlin::LfoDestination> (lfoDestinationBox.getSelectedId() - 1));
    };
    addAndMakeVisible (lfoDestinationLabel);
    lfoDestinationLabel.setText ("LFO Dest", juce::dontSendNotification);
    lfoDestinationLabel.setJustificationType (juce::Justification::centredLeft);

    configureSlider (lfoRateSlider, lfoRateLabel, "LFO Rate",
                      berlin::kMinLfoRateHz, berlin::kMaxLfoRateHz, berlin::kDefaultPatch.lfoRateHz, 2.0);
    lfoRateSlider.onValueChange = [this] { synth.setLfoRateHz ((float) lfoRateSlider.getValue()); };

    configureSlider (lfoDepthSlider, lfoDepthLabel, "LFO Depth",
                      berlin::kMinLfoDepth, berlin::kMaxLfoDepth, berlin::kDefaultPatch.lfoDepth, 0.0);
    lfoDepthSlider.onValueChange = [this] { synth.setLfoDepth ((float) lfoDepthSlider.getValue()); };

    // Make sure you set the size of the component after
    // you add any child components.
    setSize (800, 640);   // bumped from 600 (roadmap Phase 10 / euclidean-rhythm, design.md Decision 11):
                          // the new EUCLIDEAN row needs 34px, only 10px of headroom remained

    player.start();
    midiSink.openFirstAvailableDevice();   // return ignored: false is the valid silent state

    // Some platforms require permissions to open input channels so request that here
    if (juce::RuntimePermissions::isRequired (juce::RuntimePermissions::recordAudio)
        && ! juce::RuntimePermissions::isGranted (juce::RuntimePermissions::recordAudio))
    {
        juce::RuntimePermissions::request (juce::RuntimePermissions::recordAudio,
                                           [&] (bool granted) { setAudioChannels (granted ? 2 : 0, 2); });
    }
    else
    {
        // Specify the number of input and output channels that we want to open
        setAudioChannels (2, 2);
    }
}

MainComponent::~MainComponent()
{
    stopTimer();   // MUST be first (design.md): the timer callback touches `player`/`this`

    exportChooser.reset();

    // This shuts down the audio device and clears the audio source.
    shutdownAudio();
    midiSink.closeDevice();
}

//==============================================================================
void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    player.prepare (sampleRate);
    midiSink.prepare (sampleRate);
    midiBlock.ensureSize (kMidiBufferBytes);

    // setAudioChannels() always requests 2 output channels (see the ctor) -
    // AudioAppComponent exposes no getTotalNumOutputChannels() query, and the
    // synth's scratch buffer is stereo regardless (design.md Decision 4).
    const juce::dsp::ProcessSpec spec { sampleRate,
                                        static_cast<juce::uint32> (samplesPerBlockExpected),
                                        static_cast<juce::uint32> (2) };
    synth.prepare (spec);
    pushAllParametersToSynth();   // design.md Decision 6: re-seed after every (re)prepare, not just kDefaultPatch
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    const juce::ScopedNoDenormals noDenormals;   // synth.render() below does real float DSP (first Phase 8 slice to do so)

    bufferToFill.clearActiveBufferRegion();
    player.process (bufferToFill.numSamples, blockEvents);
    midiTranslator.translate (blockEvents, midiBlock);
    midiSink.dispatch (midiBlock, bufferToFill.numSamples);
    synth.render (blockEvents, *bufferToFill.buffer, bufferToFill.startSample, bufferToFill.numSamples);
}

void MainComponent::releaseResources()
{
    // This will be called when the audio device stops, or when it is being
    // restarted due to a setting change. Flush any currently-sounding note so
    // it cannot hang; deliberately does NOT call player.stop() (Decision 3 -
    // preserves the device-restart-resumes behaviour playback-transport-clock
    // established: running state is set once at construction and prepare()
    // preserves it across a device restart).
    if (player.flushPendingNoteOff (blockEvents))
    {
        midiTranslator.translate (blockEvents, midiBlock);
        midiSink.sendImmediately (midiBlock);
    }

    synth.reset();
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    // You can add your drawing code here!
}

void MainComponent::resized()
{
    // This is called when the MainContentComponent is resized.
    // If you add any child components, this is where you should
    // update their positions.
    auto area = getLocalBounds().reduced (kMargin);
    exportButton.setBounds (area.removeFromTop (kControlHeight).removeFromLeft (kButtonWidth));
    area.removeFromTop (kMargin / 2);
    statusLabel .setBounds (area.removeFromTop (kControlHeight));
    area.removeFromTop (kMargin / 2);

    // synthToggle + fxToggle merged onto one shared row (design.md Decision 7 -
    // the only change to the pre-existing header layout, freeing 34px).
    auto toggleRow = area.removeFromTop (kControlHeight);
    synthToggle.setBounds (toggleRow.removeFromLeft (kButtonWidth));
    fxToggle   .setBounds (toggleRow.removeFromLeft (kButtonWidth));
    area.removeFromTop (kMargin / 2);

    // GENERATION section (roadmap Phase 10 / generation-randomize, design.md
    // Decision 5) - placed after the toggle row, before the two-column split:
    // a global transport-level action, alongside Export/status/toggles.
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

    // EUCLIDEAN (roadmap Phase 10 / euclidean-rhythm, design.md Decision 12) -
    // one row between the generation block and EVOLUTION: rhythm mode
    // configures what Generate produces, so it belongs with GENERATION and
    // before EVOLUTION, which mutates the generated result.
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

    // EVOLUTION (roadmap Phase 9 "Evolution", Slice 2 / auto-evolution spec,
    // design.md's resized() insertion) - one row between the generation block
    // and PRESETS: label 96 | toggle 140 | 6 | rate label 96 | rate box 140
    // = 478 <= 776.
    auto evolutionRow = area.removeFromTop (kControlHeight);
    evolutionSectionLabel.setBounds (evolutionRow.removeFromLeft (kLabelWidth));
    autoEvolveToggle     .setBounds (evolutionRow.removeFromLeft (kButtonWidth));
    evolutionRow.removeFromLeft (6);
    evolveRateLabel      .setBounds (evolutionRow.removeFromLeft (kLabelWidth));
    evolveRateBox        .setBounds (evolutionRow.removeFromLeft (kButtonWidth));
    area.removeFromTop (kMargin / 2);

    // PRESETS: one full-width row, deliberately compressing the section-label
    // + control-row house pattern (design.md Decision 6) - placed after
    // generationButtonRow, before the two-column split, same slot as
    // GENERATION. presetLabel 96 | presetNameEditor 180 | 6 | Save 140 | 6 |
    // presetBox 180 | 6 | Load 140 = 754 <= 776.
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

    // Left column - OSCILLATOR: waveform, pulse width. FILTER: cutoff, resonance.
    oscillatorSectionLabel.setBounds (left.removeFromTop (kControlHeight));
    placeLabelled (left, waveformLabel, waveformBox);
    placeLabelled (left, pulseWidthLabel, pulseWidthSlider);

    filterSectionLabel.setBounds (left.removeFromTop (kControlHeight));
    placeLabelled (left, cutoffLabel, cutoffSlider);
    placeLabelled (left, resonanceLabel, resonanceSlider);

    // Right column - ENVELOPE: attack, decay, sustain, release. LFO: destination, rate, depth.
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

void MainComponent::regenerate (bool drawNewSeed)
{
    if (drawNewSeed && ! lockSeedToggle.getToggleState())
    {
        currentSeed = juce::Random::getSystemRandom().nextInt64();
        seedEditor.setText (juce::String (currentSeed), juce::dontSendNotification);
    }

    const auto mode = static_cast<RhythmMode> (rhythmModeBox.getSelectedId() - 1);
    const int  pulses   = (int) pulsesSlider.getValue();
    const int  rotation = (int) rotationSlider.getValue();

    auto next = buildSeededSequence (currentSeed, mode, pulses, rotation);

    // `player.publishSequence` swaps THROUGH its argument (design.md Decision 1):
    // on success, the argument ends up holding the stale, now-superseded buffer,
    // not the freshly built one. Publish a COPY so `next` stays intact - it is
    // what `currentSequence` (Export's source) must become, not the swapped-out
    // leftover.
    auto forPlayer = next;

    if (! player.publishSequence (forPlayer))
    {
        statusLabel.setColour (juce::Label::textColourId, juce::Colours::red);
        statusLabel.setText ("Busy, try again", juce::dontSendNotification);
        return;
    }

    currentSequence = std::move (next);

    // Undo story (evolution-mutation-engine spec, "Generate restores the
    // seeded original"): Generate with the seed field unchanged rebuilds the
    // exact same sequence sequenceSeed already points at, so re-anchoring
    // here is a no-op in that case; when a NEW seed was drawn, this ties
    // sequenceSeed to it. Either way, any mutation chain built on the old
    // sequence is discarded and the next Mutate click starts at generation 1.
    sequenceSeed = currentSeed;
    mutationCount = 0;
}

void MainComponent::mutate()
{
    berlin::DeterministicRandom rng (berlin::MutationEngine::mutationSeed (sequenceSeed, mutationCount + 1));

    auto next = berlin::MutationEngine::applyRandomTransform (currentSequence, rng);

    // Same rationale as regenerate(): player.publishSequence swaps THROUGH
    // its argument, so publish a COPY and keep `next` intact as the value
    // `currentSequence` (Export's source) must become.
    auto forPlayer = next;

    if (! player.publishSequence (forPlayer))
    {
        statusLabel.setColour (juce::Label::textColourId, juce::Colours::red);
        statusLabel.setText ("Busy, try again", juce::dontSendNotification);
        return; // currentSequence AND mutationCount unchanged: retry reproduces the same candidate
    }

    currentSequence = std::move (next);
    ++mutationCount;
}

void MainComponent::timerCallback()
{
    const int rate  = evolveRateBox.getSelectedId();   // item ID == rate (design.md Decision 8)
    const int loops = player.getLoopCount();

    if (dueAtLoopCount < 0)
    {
        // Not due yet: mark due on the threshold crossing, remembering the
        // loop-count value AT WHICH it became due (design.md Decision 7) -
        // NOT whatever getLoopCount() reads later at delivery time.
        if (loops - lastMutationLoopCount >= rate)
            dueAtLoopCount = loops;

        return;
    }

    // Due: gate on isPublishPending() BEFORE calling mutate() at all, so a
    // busy publish never reaches mutate()'s own "Busy, try again" rejection
    // path for an automatic attempt the user did not initiate (design.md
    // Decision 6). Silently retry on a later tick instead.
    if (player.isPublishPending())
        return;

    const int before = mutationCount;
    mutate();

    if (mutationCount == before)
        return;   // rejected (shouldn't happen given the pre-check, but stay due just in case) - retry next tick

    lastMutationLoopCount = dueAtLoopCount;   // re-baseline to the REMEMBERED due value, not `loops`
    dueAtLoopCount = -1;
}

void MainComponent::pushAllParametersToSynth()
{
    synth.setWaveform       (static_cast<berlin::Waveform> (waveformBox.getSelectedId() - 1));
    synth.setCutoffHz       ((float) cutoffSlider.getValue());
    synth.setResonance      ((float) resonanceSlider.getValue());
    synth.setPulseWidth     ((float) pulseWidthSlider.getValue());
    synth.setAttackSeconds  ((float) attackSlider.getValue());
    synth.setDecaySeconds   ((float) decaySlider.getValue());
    synth.setSustain        ((float) sustainSlider.getValue());
    synth.setReleaseSeconds ((float) releaseSlider.getValue());
    synth.setLfoRateHz      ((float) lfoRateSlider.getValue());
    synth.setLfoDepth       ((float) lfoDepthSlider.getValue());
    synth.setLfoDestination (static_cast<berlin::LfoDestination> (lfoDestinationBox.getSelectedId() - 1));
}

//==============================================================================
// ---- Preset controls (roadmap Phase 11 / preset-system, design.md Decision 5)
berlin::SynthPatch MainComponent::currentPatchFromWidgets() const
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

void MainComponent::applyPatchToWidgets (const berlin::SynthPatch& patch)
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

void MainComponent::savePreset()
{
    const juce::String name = presetNameEditor.getText().trim();

    if (name.isEmpty())
    {
        statusLabel.setColour (juce::Label::textColourId, juce::Colours::red);
        statusLabel.setText ("Enter a preset name first.", juce::dontSendNotification);
        return;
    }

    berlin::Preset preset;
    preset.name  = name;
    preset.patch = currentPatchFromWidgets();
    preset.seed  = currentSeed;

    const juce::File existing = presetManager.fileForName (name);

    if (existing != juce::File() && existing.existsAsFile())
    {
        savePresetButton.setEnabled (false);   // guard against re-entrant clicks while the dialog is open

        juce::Component::SafePointer<MainComponent> safeThis (this);

        juce::NativeMessageBox::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon,
                                                 "Overwrite Preset?",
                                                 "A preset named \"" + preset.name + "\" already exists. Overwrite it?",
                                                 this,
                                                 juce::ModalCallbackFunction::create ([safeThis, preset] (int result)
        {
            if (safeThis == nullptr)
                return;   // MainComponent destroyed while the dialog was open

            if (result != 0)   // AlertWindow-style mapping: OK == 1, Cancel == 0
                safeThis->writePresetFile (preset);
            else
                safeThis->savePresetButton.setEnabled (! safeThis->presetNameEditor.getText().trim().isEmpty());
        }));

        return;
    }

    writePresetFile (preset);
}

void MainComponent::writePresetFile (const berlin::Preset& preset)
{
    const auto result = presetManager.save (preset);
    savePresetButton.setEnabled (! presetNameEditor.getText().trim().isEmpty());

    if (result != berlin::PresetResult::ok)
    {
        statusLabel.setColour (juce::Label::textColourId, juce::Colours::red);
        statusLabel.setText (describePresetFailure (result), juce::dontSendNotification);
        return;
    }

    statusLabel.removeColour (juce::Label::textColourId);
    statusLabel.setText ("Saved \"" + preset.name + "\".", juce::dontSendNotification);
    refreshPresetList (preset.name);
}

void MainComponent::loadSelectedPreset()
{
    berlin::Preset preset;
    const auto result = presetManager.load (presetBox.getText(), preset);

    if (result != berlin::PresetResult::ok)
    {
        statusLabel.setColour (juce::Label::textColourId, juce::Colours::red);
        statusLabel.setText (describePresetFailure (result), juce::dontSendNotification);
        return;   // synth and sequence left untouched
    }

    applyPatchToWidgets (preset.patch);
    pushAllParametersToSynth();   // EXISTING, unchanged (design.md Decision 5)

    currentSeed = preset.seed;
    seedEditor.setText (juce::String (currentSeed), juce::dontSendNotification);
    presetNameEditor.setText (preset.name, juce::dontSendNotification);   // re-saving targets the same preset

    // EXISTING, unchanged - drawNewSeed=false means regenerate()'s Lock-Seed
    // branch (guarded by drawNewSeed) is never evaluated, so the preset's
    // saved seed applies even if Lock Seed is on (generation-live-control).
    regenerate (false);

    statusLabel.removeColour (juce::Label::textColourId);
    statusLabel.setText ("Loaded \"" + preset.name + "\".", juce::dontSendNotification);
}

void MainComponent::refreshPresetList (const juce::String& nameToSelect)
{
    const auto names = presetManager.listPresetNames();

    presetBox.clear (juce::dontSendNotification);
    for (int i = 0; i < names.size(); ++i)
        presetBox.addItem (names[i], i + 1);

    const int idToSelect = nameToSelect.isNotEmpty() ? names.indexOf (nameToSelect) + 1 : 0;
    presetBox.setSelectedId (idToSelect, juce::dontSendNotification);
    loadPresetButton.setEnabled (presetBox.getSelectedId() != 0);
}

juce::String MainComponent::describePresetFailure (berlin::PresetResult result) const
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
void MainComponent::launchExportChooser()
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

void MainComponent::exportSequenceTo (const juce::File& destination)
{
    berlin::MidiExportTimeline exportTimeline;
    const auto exportStatus = berlin::buildMidiExportTimeline (currentSequence, kStepsPerBeat, kExportRepeats, exportTimeline);

    if (exportStatus != berlin::MidiExportStatus::ok)
    {
        const juce::String message = "Export failed: could not build timeline.";
        statusLabel.setColour (juce::Label::textColourId, juce::Colours::red);
        statusLabel.setText (message, juce::dontSendNotification);
        juce::Logger::writeToLog ("Berlin: MIDI export timeline build failed, status = "
                                  + juce::String ((int) exportStatus));
        juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                     "MIDI Export Failed", message, this);
        return;
    }

    const berlin::MidiFileWriter exportWriter (kMidiChannel, berlin::MidiEventTranslator::kNoteVelocity);
    const auto writeResult = exportWriter.writeToFile (exportTimeline, kBpm, destination);

    if (writeResult != berlin::MidiFileWriteResult::ok)
    {
        const juce::String message = describeWriteFailure (writeResult);
        statusLabel.setColour (juce::Label::textColourId, juce::Colours::red);
        statusLabel.setText (message, juce::dontSendNotification);
        juce::Logger::writeToLog ("Berlin: MIDI export write failed, result = " + juce::String ((int) writeResult));
        juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                     "MIDI Export Failed", message, this);
        return;
    }

    statusLabel.removeColour (juce::Label::textColourId);
    statusLabel.setText ("Exported to " + destination.getFileName(), juce::dontSendNotification);
}
