#pragma once

#include <JuceHeader.h>
#include "midi/MidiChannel.h"
#include "midi/MidiOutputSink.h"
#include "plugin/BerlinAudioProcessor.h"
#include "settings/StandaloneSettings.h"
#include "ui/EditorFactory.h"
#include "ui/StandaloneHost.h"

//==============================================================================
/*
    Standalone shell (roadmap Phase 11 / vst3-au-plugin, plugin-host-integration
    spec's "Standalone Shell Preserves Today's Observable Behavior"
    requirement, design.md's standalone sub-buffer view). All engine/GUI logic
    now lives in berlin::BerlinAudioProcessor / the WebEditor (created through
    berlin::createBerlinEditor) - this class owns exactly one of each, plus the standalone-only
    MidiOutputSink (BerlinAudioProcessor never references it - see
    MidiOutputSink.h), and forwards the three AudioAppComponent callbacks.

    It is also the StandaloneHost (ui-settings D1): the Settings dialog reads
    and applies the audio device, sample rate, buffer size, MIDI port and MIDI
    channel through it, and the chosen setup is persisted to settings.xml and
    restored at launch. The decidable logic (parse, validate, XML, port
    resolution, JSON shapes) is in settings/StandaloneSettings; this class is
    only the JUCE glue (device enumeration, the stopped-audio bracket, file I/O).
*/
class MainComponent  : public juce::AudioAppComponent,
                       public berlin::StandaloneHost
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

    //==============================================================================
    // The editor's size limits (the WebEditor is always resizable). Owned by the editor.
    const juce::ComponentBoundsConstrainer& getEditorConstrainer() const noexcept;

    //==============================================================================
    // berlin::StandaloneHost (message thread only)
    juce::var getAudioSettings (const juce::var& query) override;
    juce::var applyAudioSettings (const juce::var& request) override;

private:
    //==============================================================================
    static constexpr int kMidiBufferBytes = 1024;

    juce::File settingsFilePath() const;
    bool       saveSettings();   // false = write failed (never fatal)

    // Member order matters (ui-settings D2): the editor (whose browser lambdas call back
    // into this host) is destroyed before everything the host uses.
    berlin::BerlinAudioProcessor       processor;   // MUST precede `editor` - editor takes a reference to it
    berlin::MidiOutputSink             midiSink;
    berlin::MidiPortChoice             midiPreference;      // remembered choice, persisted when no port is open
    juce::String                       currentMidiPortId;   // "" = None / no device open
    bool                               applying = false;     // reentrancy guard for applyAudioSettings
    std::unique_ptr<juce::AudioProcessorEditor> editor;     // WebEditor (berlin::createBerlinEditor)
    juce::MidiBuffer                   midiBlock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
