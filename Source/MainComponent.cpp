#include "MainComponent.h"

#include "core/Scale.h"
#include "generation/DeterministicRandom.h"
#include "generation/PitchGenerator.h"
#include "generation/RhythmGenerator.h"

namespace
{
    constexpr double kBpm         = 120.0;
    constexpr int    kStepsPerBeat = 4;
    constexpr int    kNumSteps     = 16;
    constexpr int    kSeed         = 12345;
    constexpr int    kMidiChannel     = 1;
    constexpr int    kMidiBufferBytes = 1024;
}

berlin::Sequence MainComponent::buildSeededSequence()
{
    berlin::DeterministicRandom rng (kSeed);
    auto sequence = berlin::RhythmGenerator (kNumSteps, 0.5f).generate (rng);

    berlin::PitchGenerator pitch (berlin::Scale::minor (48), 36, 72);
    for (int i = 0; i < sequence.size(); ++i)
    {
        if (sequence[i].active)
            sequence[i].note = pitch.generateNextNote (rng);
    }

    return sequence;
}

//==============================================================================
MainComponent::MainComponent()
    : player (buildSeededSequence(), berlin::Transport (kBpm, kStepsPerBeat)),
      midiTranslator (kMidiChannel), midiSink (kMidiChannel)
{
    // Make sure you set the size of the component after
    // you add any child components.
    setSize (800, 600);

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
    // This shuts down the audio device and clears the audio source.
    shutdownAudio();
    midiSink.closeDevice();
}

//==============================================================================
void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    juce::ignoreUnused (samplesPerBlockExpected);
    player.prepare (sampleRate);
    midiSink.prepare (sampleRate);
    midiBlock.ensureSize (kMidiBufferBytes);
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    bufferToFill.clearActiveBufferRegion();
    player.process (bufferToFill.numSamples, blockEvents);
    midiTranslator.translate (blockEvents, midiBlock);
    midiSink.dispatch (midiBlock, bufferToFill.numSamples);
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
}
