#include "MainComponent.h"

//==============================================================================
MainComponent::MainComponent()
    : editor (processor),
      midiSink (berlin::kMidiChannel)
{
    addAndMakeVisible (editor);
    setSize (800, 680);

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
    // This shuts down the audio device and clears the audio source.
    shutdownAudio();
    midiSink.closeDevice();
}

//==============================================================================
void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    processor.prepareToPlay (sampleRate, samplesPerBlockExpected);   // NOTE: arg order flipped vs AudioAppComponent
    midiSink.prepare (sampleRate);
    midiBlock.ensureSize (kMidiBufferBytes);
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    // Wraps bufferToFill's existing write pointers - 2 channels stays inside
    // AudioBuffer's inline channel array, so this does NOT allocate
    // (design.md's standalone sub-buffer view).
    juce::AudioBuffer<float> block (bufferToFill.buffer->getArrayOfWritePointers(),
                                    bufferToFill.buffer->getNumChannels(),
                                    bufferToFill.startSample,
                                    bufferToFill.numSamples);

    processor.processBlock (block, midiBlock);   // clears block + midiBlock internally
    midiSink.dispatch (midiBlock, bufferToFill.numSamples);   // standalone only
}

void MainComponent::releaseResources()
{
    // Flush any currently-sounding note so it cannot hang, THEN let the
    // processor do its own (now-idempotent) internal flush + synth.reset().
    // Deliberately does NOT call player.stop() - preserves the
    // device-restart-resumes behaviour playback-transport-clock established.
    if (processor.flushPendingNoteOff (midiBlock))
        midiSink.sendImmediately (midiBlock);

    processor.releaseResources();
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void MainComponent::resized()
{
    editor.setBounds (getLocalBounds());
}
