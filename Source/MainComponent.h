#pragma once

#include <JuceHeader.h>
#include "midi/MidiOutputSink.h"
#include "plugin/BerlinAudioProcessor.h"
#include "plugin/BerlinAudioProcessorEditor.h"

//==============================================================================
/*
    Standalone shell (roadmap Phase 11 / vst3-au-plugin, plugin-host-integration
    spec's "Standalone Shell Preserves Today's Observable Behavior"
    requirement, design.md's standalone sub-buffer view). All engine/GUI logic
    now lives in berlin::BerlinAudioProcessor / berlin::BerlinAudioProcessorEditor
    - this class owns exactly one of each, plus the standalone-only
    MidiOutputSink (BerlinAudioProcessor never references it - see
    MidiOutputSink.h), and forwards the three AudioAppComponent callbacks.
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
    static constexpr int kMidiChannel     = 1;
    static constexpr int kMidiBufferBytes = 1024;

    berlin::BerlinAudioProcessor       processor;   // MUST precede `editor` - editor takes a reference to it
    berlin::BerlinAudioProcessorEditor editor;
    berlin::MidiOutputSink             midiSink;
    juce::MidiBuffer                   midiBlock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
