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
    constexpr int    kExportRepeats   = 4;   // 4 x 16 steps @ 16ths = 16 beats = exactly 4 bars of 4/4
    const char*      kExportFileName  = "berlin-export.mid";

    constexpr int kMargin = 12, kControlHeight = 28, kButtonWidth = 140;

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

juce::File MainComponent::defaultExportFile()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("Berlin")
        .getChildFile (kExportFileName);
}

//==============================================================================
MainComponent::MainComponent()
    : sequence (buildSeededSequence()),
      player (sequence, berlin::Transport (kBpm, kStepsPerBeat)),
      midiTranslator (kMidiChannel), midiSink (kMidiChannel)
{
    addAndMakeVisible (exportButton);
    addAndMakeVisible (statusLabel);
    exportButton.onClick = [this] { launchExportChooser(); };

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
    exportChooser.reset();

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
    auto area = getLocalBounds().reduced (kMargin);
    exportButton.setBounds (area.removeFromTop (kControlHeight).removeFromLeft (kButtonWidth));
    area.removeFromTop (kMargin / 2);
    statusLabel .setBounds (area.removeFromTop (kControlHeight));
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
    const auto exportStatus = berlin::buildMidiExportTimeline (sequence, kStepsPerBeat, kExportRepeats, exportTimeline);

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
