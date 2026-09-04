#pragma once

#include <JuceHeader.h>
#include "playback/SequencePlayer.h"
#include "playback/StepEventBuffer.h"
#include "midi/MidiEventTranslator.h"
#include "midi/MidiOutputSink.h"
#include "export/MidiExportTimeline.h"
#include "export/MidiFileWriter.h"

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

    const berlin::Sequence      sequence;            // MUST precede `player` (Decision 2)
    berlin::SequencePlayer      player;
    berlin::StepEventBuffer     blockEvents;
    berlin::MidiEventTranslator midiTranslator;
    berlin::MidiOutputSink      midiSink;
    juce::MidiBuffer            midiBlock;

    juce::TextButton exportButton { "Export MIDI..." };
    juce::Label      statusLabel;
    std::unique_ptr<juce::FileChooser> exportChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
