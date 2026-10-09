#include "settings/StandaloneSettings.h"

#include <cmath>

namespace berlin
{

namespace
{
    constexpr int kMinChannel = 1;
    constexpr int kMaxChannel = 16;

    bool isNumber (const juce::var& v) noexcept { return v.isInt() || v.isInt64() || v.isDouble(); }

    bool isWholeNumber (const juce::var& v) noexcept
    {
        if (v.isInt() || v.isInt64())
            return true;

        if (! v.isDouble())
            return false;

        const double d = v;
        return d == std::floor (d);
    }

    ParsedRequest invalid()
    {
        ParsedRequest p;
        p.ok = false;
        p.error = settingsError::invalidRequest;
        return p;
    }

    bool hasPort (const juce::Array<MidiPort>& ports, const juce::String& id)
    {
        for (const auto& p : ports)
            if (p.id == id)
                return true;

        return false;
    }

    juce::var makeObject()
    {
        return juce::var (new juce::DynamicObject());
    }
}

//==============================================================================
ParsedRequest parseApplyRequest (const juce::var& request)
{
    auto* object = request.getDynamicObject();

    if (object == nullptr)
        return invalid();

    const auto deviceType = object->getProperty ("deviceType");
    const auto device     = object->getProperty ("device");
    const auto rate       = object->getProperty ("sampleRate");
    const auto buffer     = object->getProperty ("bufferSize");
    const auto port       = object->getProperty ("midiPortId");
    const auto channel    = object->getProperty ("midiChannel");

    if (! deviceType.isString() || ! device.isString() || ! port.isString())
        return invalid();

    if (! isNumber (rate) || ! isWholeNumber (buffer) || ! isWholeNumber (channel))
        return invalid();

    const double rateValue    = rate;
    const int    bufferValue  = buffer;
    const int    channelValue = channel;

    if (! (rateValue > 0.0) || bufferValue <= 0 || channelValue < kMinChannel || channelValue > kMaxChannel)
        return invalid();

    ParsedRequest parsed;
    parsed.ok = true;
    parsed.request.deviceType  = deviceType.toString();
    parsed.request.device      = device.toString();
    parsed.request.sampleRate  = rateValue;
    parsed.request.bufferSize  = bufferValue;
    parsed.request.midiPortId  = port.toString();
    parsed.request.midiChannel = channelValue;
    return parsed;
}

juce::String validateApplyRequest (const ApplyRequest& request, const DeviceOptions& options)
{
    if (! options.deviceExists)
        return settingsError::deviceUnavailable;

    if (! options.sampleRates.contains (request.sampleRate))
        return settingsError::sampleRateUnsupported;

    if (! options.bufferSizes.contains (request.bufferSize))
        return settingsError::bufferSizeUnsupported;

    if (request.midiPortId.isNotEmpty() && ! hasPort (options.midiPorts, request.midiPortId))
        return settingsError::midiPortUnavailable;

    return {};
}

//==============================================================================
std::unique_ptr<juce::XmlElement> settingsToXml (const SavedSettings& settings)
{
    auto root = std::make_unique<juce::XmlElement> ("BerlinSettings");
    root->setAttribute ("version", kSettingsVersion);

    if (settings.audioState != nullptr)
        root->createNewChildElement ("AUDIO")->addChildElement (new juce::XmlElement (*settings.audioState));

    auto* midi = root->createNewChildElement ("MIDI");

    if (settings.port.none)
    {
        midi->setAttribute ("none", 1);
    }
    else
    {
        if (settings.port.id.isNotEmpty())
            midi->setAttribute ("id", settings.port.id);

        if (settings.port.name.isNotEmpty())
            midi->setAttribute ("name", settings.port.name);
    }

    midi->setAttribute ("channel", settings.channel);
    return root;
}

SavedSettings settingsFromXml (const juce::XmlElement* root)
{
    SavedSettings settings;

    if (root == nullptr || ! root->hasTagName ("BerlinSettings") || root->getIntAttribute ("version", 0) != kSettingsVersion)
        return settings;

    if (auto* audio = root->getChildByName ("AUDIO"))
        if (auto* state = audio->getFirstChildElement())
            settings.audioState = std::make_unique<juce::XmlElement> (*state);

    if (auto* midi = root->getChildByName ("MIDI"))
    {
        settings.port.none = midi->getIntAttribute ("none", 0) == 1;

        if (! settings.port.none)
        {
            settings.port.id   = midi->getStringAttribute ("id");
            settings.port.name = midi->getStringAttribute ("name");
        }

        const int channel = midi->getIntAttribute ("channel", 1);
        settings.channel = (channel >= kMinChannel && channel <= kMaxChannel) ? channel : 1;
    }

    return settings;
}

SavedSettings loadSettingsFile (const juce::File& file)
{
    if (! file.existsAsFile())
        return {};

    const auto xml = juce::parseXML (file);
    return settingsFromXml (xml.get());
}

bool writeSettingsFile (const juce::File& file, const SavedSettings& settings)
{
    if (! file.getParentDirectory().createDirectory().wasOk())
        return false;

    const auto xml = settingsToXml (settings);
    return xml != nullptr && xml->writeTo (file);
}

//==============================================================================
juce::String resolveMidiPort (const MidiPortChoice& saved, const juce::Array<MidiPort>& ports)
{
    if (saved.none || ports.isEmpty())
        return {};

    if (saved.id.isNotEmpty() && hasPort (ports, saved.id))
        return saved.id;

    if (saved.name.isNotEmpty())
        for (const auto& p : ports)
            if (p.name == saved.name)
                return p.id;

    return ports.getFirst().id;
}

MidiPortChoice chosenPortChoice (const juce::String& portId, const juce::Array<MidiPort>& ports)
{
    MidiPortChoice choice;
    choice.none = portId.isEmpty();
    choice.id   = portId;

    for (const auto& p : ports)
        if (p.id == portId)
            choice.name = p.name;

    return choice;
}

MidiPortChoice portToPersist (const juce::String& openPortId, const MidiPortChoice& preference,
                              const juce::Array<MidiPort>& ports)
{
    return openPortId.isNotEmpty() ? chosenPortChoice (openPortId, ports) : preference;
}

//==============================================================================
juce::var audioSettingsToVar (const AudioSettingsView& view)
{
    auto toArray = [] (const juce::StringArray& strings)
    {
        juce::Array<juce::var> out;
        for (const auto& s : strings)
            out.add (s);
        return juce::var (out);
    };

    juce::Array<juce::var> rates;
    for (const auto r : view.sampleRates)
        rates.add (r);

    juce::Array<juce::var> buffers;
    for (const auto b : view.bufferSizes)
        buffers.add (b);

    auto port = [] (const juce::String& id, const juce::String& name)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("id", id);
        o->setProperty ("name", name);
        return juce::var (o);
    };

    juce::Array<juce::var> ports;
    ports.add (port ({}, "None"));
    for (const auto& p : view.midiPorts)
        ports.add (port (p.id, p.name));

    auto* o = new juce::DynamicObject();
    o->setProperty ("deviceTypes", toArray (view.deviceTypes));
    o->setProperty ("deviceType", view.deviceType);
    o->setProperty ("devices", toArray (view.devices));
    o->setProperty ("device", view.device);
    o->setProperty ("sampleRates", juce::var (rates));
    o->setProperty ("sampleRate", view.sampleRate);
    o->setProperty ("bufferSizes", juce::var (buffers));
    o->setProperty ("bufferSize", view.bufferSize);
    o->setProperty ("midiPorts", juce::var (ports));
    o->setProperty ("midiPortId", view.midiPortId);
    o->setProperty ("midiChannel", view.midiChannel);
    return juce::var (o);
}

juce::var okResult()
{
    auto result = makeObject();
    result.getDynamicObject()->setProperty ("ok", true);
    return result;
}

juce::var errorResult (const juce::String& token)
{
    auto result = makeObject();
    result.getDynamicObject()->setProperty ("ok", false);
    result.getDynamicObject()->setProperty ("error", token);
    return result;
}

} // namespace berlin
