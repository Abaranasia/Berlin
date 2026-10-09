/*
  ==============================================================================

   StandaloneSettings - the pure (JUCE-core + data-structures only, no
   juce_audio_devices) half of the standalone Settings dialog (ui-settings
   design D3, D7, D8, D9): apply-request parsing, validation, the persisted
   settings XML, MIDI port resolution at launch, and the JSON payload shapes
   exchanged with the web UI.

   MainComponent keeps only the JUCE glue (device enumeration, the stopped-audio
   bracket, file I/O); everything decidable without a device lives here so
   BerlinTests can cover it headless.

   MIDI port identity: the id "" means None (listed first as {id:"",name:"None"}).

  ==============================================================================
*/

#pragma once

#include <memory>

#include <juce_core/juce_core.h>

namespace berlin
{

// ---- error tokens (the wire contract with the web UI) ----
namespace settingsError
{
    inline const char* const invalidRequest         = "invalid-request";
    inline const char* const deviceUnavailable      = "device-unavailable";
    inline const char* const sampleRateUnsupported  = "sample-rate-unsupported";
    inline const char* const bufferSizeUnsupported  = "buffer-size-unsupported";
    inline const char* const midiPortUnavailable    = "midi-port-unavailable";
    inline const char* const busy                   = "busy";
}

struct MidiPort
{
    juce::String id;
    juce::String name;
};

// ---- apply request ----
struct ApplyRequest
{
    juce::String deviceType;
    juce::String device;
    double       sampleRate  = 0.0;
    int          bufferSize  = 0;
    juce::String midiPortId;       // "" = None
    int          midiChannel = 1;
};

struct ParsedRequest
{
    bool         ok = false;
    ApplyRequest request;
    juce::String error;            // settingsError::invalidRequest when !ok
};

ParsedRequest parseApplyRequest (const juce::var& request);

// What the host found for the requested device type/device. Plain arrays so the
// check stays pure; `busy` is owned by the host guard, not by validation.
struct DeviceOptions
{
    bool                   deviceExists = false;
    juce::Array<double>    sampleRates;
    juce::Array<int>       bufferSizes;
    juce::Array<MidiPort>  midiPorts;      // real ports only (None is always valid)
};

// "" when valid, else the first violated token in the order
// device, sample rate, buffer size, MIDI port.
juce::String validateApplyRequest (const ApplyRequest& request, const DeviceOptions& options);

// ---- persisted settings (one versioned XML tree) ----
struct MidiPortChoice
{
    bool         none = false;     // explicit None
    juce::String id;
    juce::String name;
};

struct SavedSettings
{
    std::unique_ptr<juce::XmlElement> audioState;   // AudioDeviceManager::createStateXml(), may be null
    MidiPortChoice                    port;
    int                               channel = 1;
};

inline constexpr int kSettingsVersion = 1;

std::unique_ptr<juce::XmlElement> settingsToXml (const SavedSettings& settings);
SavedSettings                     settingsFromXml (const juce::XmlElement* root);   // defaults on anything wrong
SavedSettings                     loadSettingsFile (const juce::File& file);        // defaults on anything wrong
bool                              writeSettingsFile (const juce::File& file, const SavedSettings& settings);   // false on failure, never throws

// The port id to open at launch: "" means None.
juce::String resolveMidiPort (const MidiPortChoice& saved, const juce::Array<MidiPort>& ports);

// ---- JSON payloads ----
struct AudioSettingsView
{
    juce::StringArray     deviceTypes;
    juce::String          deviceType;
    juce::StringArray     devices;
    juce::String          device;
    juce::Array<double>   sampleRates;
    double                sampleRate = 0.0;
    juce::Array<int>      bufferSizes;
    int                   bufferSize = 0;
    juce::Array<MidiPort> midiPorts;       // real ports only; None is prepended by audioSettingsToVar
    juce::String          midiPortId;
    int                   midiChannel = 1;
};

juce::var audioSettingsToVar (const AudioSettingsView& view);
juce::var okResult();
juce::var errorResult (const juce::String& token);

} // namespace berlin
