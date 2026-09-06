#pragma once

#include <JuceHeader.h>
#include "playback/SequencePlayer.h"
#include "playback/StepEventBuffer.h"
#include "midi/MidiEventTranslator.h"
#include "midi/MidiOutputSink.h"
#include "export/MidiExportTimeline.h"
#include "export/MidiFileWriter.h"
#include "synth/SynthEngine.h"

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
    static berlin::Sequence buildSeededSequence();
    static juce::File       defaultExportFile();     // default destination seeded into the save dialog

    void launchExportChooser();
    void exportSequenceTo (const juce::File& destination);

    // Re-pushes every control's current value to `synth` (design.md Decision 6).
    // Called once from prepareToPlay, immediately after synth.prepare(spec) -
    // SynthEngine::prepare seeds the voice from kDefaultPatch, so without this a
    // device/sample-rate change would silently snap parameters back to default
    // while the sliders still showed the user's values.
    void pushAllParametersToSynth();

    const berlin::Sequence      sequence;            // MUST precede `player` (Decision 2)
    berlin::SequencePlayer      player;
    berlin::StepEventBuffer     blockEvents;
    berlin::MidiEventTranslator midiTranslator;
    berlin::MidiOutputSink      midiSink;
    juce::MidiBuffer            midiBlock;
    berlin::SynthEngine         synth;

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
