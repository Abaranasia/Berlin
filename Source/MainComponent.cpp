#include "MainComponent.h"

#include "settings/StandaloneSettings.h"

namespace
{
    // What the Settings dialog needs to know about one output device (ui-settings D9).
    struct DeviceProbe
    {
        berlin::DeviceOptions options;      // deviceExists false when the type/device is unknown or cannot be created
        double                defaultRate   = 0.0;
        int                   defaultBuffer = 0;
        bool                  isOpenDevice  = false;
        double                openRate      = 0.0;
        int                   openBuffer    = 0;
    };

    juce::Array<berlin::MidiPort> availableMidiPorts()
    {
        juce::Array<berlin::MidiPort> ports;

        for (const auto& d : juce::MidiOutput::getAvailableDevices())
            ports.add ({ d.identifier, d.name });

        return ports;
    }

    juce::AudioIODeviceType* findDeviceType (juce::AudioDeviceManager& manager, const juce::String& typeName)
    {
        for (auto* type : manager.getAvailableDeviceTypes())
            if (type->getTypeName() == typeName)
                return type;

        return nullptr;
    }

    double pickDefaultRate (const juce::Array<double>& rates)
    {
        if (rates.contains (48000.0))
            return 48000.0;

        return rates.isEmpty() ? 0.0 : rates.getFirst();
    }

    // Re-scans the device type and lists the rates and buffer sizes of one device.
    // The device that is already open is read directly; any other is created
    // temporarily and never opened. When a driver cannot create a second device
    // (ASIO), the result has deviceExists == false and empty lists.
    DeviceProbe probeDevice (juce::AudioDeviceManager& manager, const juce::String& typeName, const juce::String& deviceName)
    {
        DeviceProbe probe;
        auto* type = findDeviceType (manager, typeName);

        if (type == nullptr)
            return probe;

        type->scanForDevices();

        if (! type->getDeviceNames (false).contains (deviceName))
            return probe;

        const auto setup = manager.getAudioDeviceSetup();
        auto* open = manager.getCurrentAudioDevice();

        if (open != nullptr && manager.getCurrentAudioDeviceType() == typeName && setup.outputDeviceName == deviceName)
        {
            probe.isOpenDevice = true;
            probe.openRate     = open->getCurrentSampleRate();
            probe.openBuffer   = open->getCurrentBufferSizeSamples();
            probe.options.deviceExists = true;
            probe.options.sampleRates  = open->getAvailableSampleRates();
            probe.options.bufferSizes  = open->getAvailableBufferSizes();
            probe.defaultRate   = probe.openRate;
            probe.defaultBuffer = probe.openBuffer;
            return probe;
        }

        // A type with separate inputs and outputs (WASAPI) is probed output-only; a combined
        // one (ASIO) takes the same name for both.
        std::unique_ptr<juce::AudioIODevice> temporary (type->createDevice (deviceName,
                                                                           type->hasSeparateInputsAndOutputs() ? juce::String() : deviceName));

        if (temporary == nullptr)
            return probe;

        probe.options.deviceExists = true;
        probe.options.sampleRates  = temporary->getAvailableSampleRates();
        probe.options.bufferSizes  = temporary->getAvailableBufferSizes();
        probe.defaultRate   = pickDefaultRate (probe.options.sampleRates);
        probe.defaultBuffer = temporary->getDefaultBufferSize();
        return probe;
    }
}

//==============================================================================
MainComponent::MainComponent()
    : midiSink (berlin::kMidiChannel),
      editor (berlin::createBerlinEditor (processor, this))
{
    addAndMakeVisible (*editor);
    setSize (editor->getWidth(), editor->getHeight());

    // Launch restore (ui-settings D8): the saved MIDI channel and port, then the saved audio
    // state. A missing, corrupt or unknown-version file yields defaults (channel 1, first
    // port, default audio device); a saved audio device that cannot be opened falls back to
    // the default one (AudioDeviceManager::initialise with selectDefaultDeviceOnFailure).
    auto saved = berlin::loadSettingsFile (settingsFilePath());

    midiSink.setChannel (saved.channel);
    midiPreference    = saved.port;   // stays remembered even when that port is unavailable now
    currentMidiPortId = berlin::resolveMidiPort (saved.port, availableMidiPorts());

    if (! midiSink.openDevice (currentMidiPortId))   // false is the valid silent state (None / failed open)
        currentMidiPortId = {};

    // Some platforms require permissions to open input channels so request that here
    if (juce::RuntimePermissions::isRequired (juce::RuntimePermissions::recordAudio)
        && ! juce::RuntimePermissions::isGranted (juce::RuntimePermissions::recordAudio))
    {
        std::shared_ptr<juce::XmlElement> audioState (saved.audioState.release());
        juce::RuntimePermissions::request (juce::RuntimePermissions::recordAudio,
                                           [this, audioState] (bool granted) { setAudioChannels (granted ? 2 : 0, 2, audioState.get()); });
    }
    else
    {
        // Specify the number of input and output channels that we want to open
        setAudioChannels (2, 2, saved.audioState.get());
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
juce::File MainComponent::settingsFilePath() const
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("Berlin")
               .getChildFile ("settings.xml");
}

bool MainComponent::saveSettings()
{
    berlin::SavedSettings settings;
    settings.audioState = deviceManager.createStateXml();
    settings.channel    = midiSink.getChannel();

    settings.port = berlin::portToPersist (currentMidiPortId, midiPreference, availableMidiPorts());

    return berlin::writeSettingsFile (settingsFilePath(), settings);
}

juce::var MainComponent::getAudioSettings (const juce::var& query)
{
    const auto setup = deviceManager.getAudioDeviceSetup();

    juce::String deviceType = deviceManager.getCurrentAudioDeviceType();
    juce::String device     = setup.outputDeviceName;

    // Optional {deviceType, device}: the device selected in the dialog but not applied yet.
    if (query.isObject() && query["deviceType"].isString() && query["device"].isString())
    {
        deviceType = query["deviceType"].toString();
        device     = query["device"].toString();
    }

    berlin::AudioSettingsView view;

    for (auto* type : deviceManager.getAvailableDeviceTypes())
        view.deviceTypes.add (type->getTypeName());

    view.deviceType = deviceType;
    view.device     = device;

    // Re-enumerated on every call (re-listed on each open; no hot-plug refresh while open).
    if (auto* type = findDeviceType (deviceManager, deviceType))
    {
        type->scanForDevices();
        view.devices = type->getDeviceNames (false);
    }

    const auto probe = probeDevice (deviceManager, deviceType, device);
    view.sampleRates = probe.options.sampleRates;
    view.bufferSizes = probe.options.bufferSizes;
    view.sampleRate  = probe.isOpenDevice ? probe.openRate   : probe.defaultRate;
    view.bufferSize  = probe.isOpenDevice ? probe.openBuffer : probe.defaultBuffer;

    view.midiPorts   = availableMidiPorts();
    view.midiPortId  = midiSink.isDeviceOpen() ? currentMidiPortId : juce::String();
    view.midiChannel = midiSink.getChannel();

    return berlin::audioSettingsToVar (view);
}

juce::var MainComponent::applyAudioSettings (const juce::var& requestVar)
{
    if (applying)
        return berlin::errorResult (berlin::settingsError::busy);

    const juce::ScopedValueSetter<bool> guard (applying, true);

    const auto parsed = berlin::parseApplyRequest (requestVar);

    if (! parsed.ok)
        return berlin::errorResult (parsed.error);

    const auto& request = parsed.request;

    // Validate the whole request before touching anything (ui-settings D4).
    auto probe = probeDevice (deviceManager, request.deviceType, request.device);
    probe.options.midiPorts = availableMidiPorts();

    const auto validationError = berlin::validateApplyRequest (request, probe.options);

    if (validationError.isNotEmpty())
        return berlin::errorResult (validationError);

    // Remember the live state for a best-effort rollback.
    const auto previousSetup   = deviceManager.getAudioDeviceSetup();
    const auto previousType    = deviceManager.getCurrentAudioDeviceType();
    const auto previousPort    = currentMidiPortId;
    const auto previousChannel = midiSink.getChannel();

    // Stop audio first: releaseResources() flushes the pending note-off to the OLD port
    // and channel, so the MIDI swap below runs with the audio callback provably stopped.
    deviceManager.closeAudioDevice();

    // ---- MIDI ----
    const bool portChanged = request.midiPortId != currentMidiPortId;
    bool midiOk = true;

    if (portChanged)
    {
        // openDevice() closes the old port in the ordered sequence (CC123 last, on the old
        // channel) before opening the new one; the channel changes only afterwards.
        midiOk = midiSink.openDevice (request.midiPortId) || request.midiPortId.isEmpty();
        currentMidiPortId = midiSink.isDeviceOpen() ? request.midiPortId : juce::String();
    }

    if (midiOk)
        midiSink.setChannel (request.midiChannel);

    // ---- audio ----
    juce::String audioError;

    if (midiOk)
    {
        auto setup = previousSetup;
        const bool typeChanged = request.deviceType != previousType;

        setup.outputDeviceName = request.device;
        setup.sampleRate       = request.sampleRate;
        setup.bufferSize       = request.bufferSize;

        // A type with combined inputs and outputs (ASIO) takes the same name for both; a
        // separate-I/O type (WASAPI) keeps its input unless the type itself changes.
        auto* type = findDeviceType (deviceManager, request.deviceType);

        if (type != nullptr && ! type->hasSeparateInputsAndOutputs())
            setup.inputDeviceName = request.device;
        else if (typeChanged)
            setup.inputDeviceName = {};

        const bool changed = typeChanged
                          || setup.outputDeviceName != previousSetup.outputDeviceName
                          || setup.inputDeviceName  != previousSetup.inputDeviceName
                          || setup.sampleRate       != previousSetup.sampleRate
                          || setup.bufferSize       != previousSetup.bufferSize;

        if (changed)
        {
            if (typeChanged)
                deviceManager.setCurrentAudioDeviceType (request.deviceType, true);

            audioError = deviceManager.setAudioDeviceSetup (setup, true);
        }
        else
        {
            deviceManager.restartLastAudioDevice();
        }
    }

    if (! midiOk || audioError.isNotEmpty())
    {
        // Best-effort rollback to the previous port, channel and audio setup; the file is not written.
        if (portChanged)
        {
            midiSink.openDevice (previousPort);
            currentMidiPortId = midiSink.isDeviceOpen() ? previousPort : juce::String();
        }

        midiSink.setChannel (previousChannel);

        if (previousType != deviceManager.getCurrentAudioDeviceType())
            deviceManager.setCurrentAudioDeviceType (previousType, true);

        if (deviceManager.setAudioDeviceSetup (previousSetup, true).isNotEmpty())
            deviceManager.restartLastAudioDevice();

        return berlin::errorResult (midiOk ? berlin::settingsError::deviceUnavailable
                                           : berlin::settingsError::midiPortUnavailable);
    }

    midiPreference = berlin::chosenPortChoice (request.midiPortId, availableMidiPorts());
    saveSettings();   // a failed write is silent: the apply itself succeeded (D7)
    return berlin::okResult();
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

const juce::ComponentBoundsConstrainer& MainComponent::getEditorConstrainer() const noexcept
{
    auto* constrainer = editor->getConstrainer();
    jassert (constrainer != nullptr);   // the WebEditor always installs its size limits
    return *constrainer;
}

void MainComponent::resized()
{
    editor->setBounds (getLocalBounds());
}
