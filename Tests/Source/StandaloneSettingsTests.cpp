/*
  ==============================================================================

   StandaloneSettings tests (ui-settings: standalone-settings "Transactional
   Apply", "Persistence File", "Launch Restore And Fallback"; ui-bridge
   "Standalone-Only Settings Functions" payload shape).

  ==============================================================================
*/

#include <juce_core/juce_core.h>

#include "settings/StandaloneSettings.h"

namespace
{
    using namespace berlin;

    juce::var requestVar (const juce::String& deviceType = "Windows Audio", const juce::String& device = "Speakers",
                          juce::var rate = 48000.0, juce::var buffer = 256,
                          const juce::String& port = "p1", juce::var channel = 3)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("deviceType", deviceType);
        o->setProperty ("device", device);
        o->setProperty ("sampleRate", rate);
        o->setProperty ("bufferSize", buffer);
        o->setProperty ("midiPortId", port);
        o->setProperty ("midiChannel", channel);
        return juce::var (o);
    }

    juce::var withoutKey (juce::var v, const juce::String& key)
    {
        auto* o = new juce::DynamicObject();
        for (const auto& p : v.getDynamicObject()->getProperties())
            if (p.name.toString() != key)
                o->setProperty (p.name, p.value);
        return juce::var (o);
    }

    ApplyRequest validRequest()
    {
        ApplyRequest r;
        r.deviceType = "Windows Audio"; r.device = "Speakers"; r.sampleRate = 48000.0;
        r.bufferSize = 256; r.midiPortId = "p1"; r.midiChannel = 3;
        return r;
    }

    DeviceOptions validOptions()
    {
        DeviceOptions o;
        o.deviceExists = true;
        o.sampleRates = { 44100.0, 48000.0 };
        o.bufferSizes = { 128, 256, 512 };
        o.midiPorts.add ({ "p1", "Synth A" });
        o.midiPorts.add ({ "p2", "Synth B" });
        return o;
    }

    juce::File scratchDir()
    {
        return juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("BerlinSettingsTests");
    }
}

class StandaloneSettingsTests final : public juce::UnitTest
{
public:
    StandaloneSettingsTests() : juce::UnitTest ("StandaloneSettings", "Berlin") {}

    void runTest() override
    {
        testParse();
        testValidate();
        testXml();
        testResolve();
        testPortToPersist();
        testPayloads();
        testWriteFailure();
    }

private:
    void testParse()
    {
        beginTest ("parseApplyRequest accepts a valid request");
        {
            const auto p = parseApplyRequest (requestVar());
            expect (p.ok);
            expectEquals (p.request.deviceType, juce::String ("Windows Audio"));
            expectEquals (p.request.device, juce::String ("Speakers"));
            expectEquals (p.request.sampleRate, 48000.0);
            expectEquals (p.request.bufferSize, 256);
            expectEquals (p.request.midiPortId, juce::String ("p1"));
            expectEquals (p.request.midiChannel, 3);
            expectEquals (p.error, juce::String());
        }

        beginTest ("parseApplyRequest accepts the None port id (empty string)");
        {
            const auto p = parseApplyRequest (requestVar ("A", "B", 44100.0, 128, "", 1));
            expect (p.ok);
            expectEquals (p.request.midiPortId, juce::String());
        }

        beginTest ("parseApplyRequest rejects non-objects and missing fields as invalid-request");
        {
            for (const auto* key : { "deviceType", "device", "sampleRate", "bufferSize", "midiPortId", "midiChannel" })
            {
                const auto p = parseApplyRequest (withoutKey (requestVar(), key));
                expect (! p.ok, juce::String ("missing ") + key);
                expectEquals (p.error, juce::String ("invalid-request"));
            }
            expectEquals (parseApplyRequest (juce::var()).error, juce::String ("invalid-request"));
            expectEquals (parseApplyRequest (juce::var ("text")).error, juce::String ("invalid-request"));
        }

        beginTest ("parseApplyRequest rejects wrong field types");
        {
            expect (! parseApplyRequest (requestVar ("A", "B", juce::var ("48000"), 256, "p", 3)).ok);
            expect (! parseApplyRequest (requestVar ("A", "B", 48000.0, juce::var ("256"), "p", 3)).ok);
            expect (! parseApplyRequest (requestVar ("A", "B", 48000.0, 256, "p", juce::var ("3"))).ok);
            expect (! parseApplyRequest (requestVar ("A", "B", 48000.0, 256, "p", juce::var (3.5))).ok);
        }

        beginTest ("parseApplyRequest rejects channel 0 and 17 and non-positive rate or buffer");
        {
            expectEquals (parseApplyRequest (requestVar ("A", "B", 48000.0, 256, "p", 0)).error, juce::String ("invalid-request"));
            expectEquals (parseApplyRequest (requestVar ("A", "B", 48000.0, 256, "p", 17)).error, juce::String ("invalid-request"));
            expect (parseApplyRequest (requestVar ("A", "B", 48000.0, 256, "p", 1)).ok);
            expect (parseApplyRequest (requestVar ("A", "B", 48000.0, 256, "p", 16)).ok);
            expectEquals (parseApplyRequest (requestVar ("A", "B", 0.0, 256, "p", 1)).error, juce::String ("invalid-request"));
            expectEquals (parseApplyRequest (requestVar ("A", "B", -44100.0, 256, "p", 1)).error, juce::String ("invalid-request"));
            expectEquals (parseApplyRequest (requestVar ("A", "B", 48000.0, 0, "p", 1)).error, juce::String ("invalid-request"));
        }
    }

    void testValidate()
    {
        beginTest ("validateApplyRequest returns empty for a valid request");
        expectEquals (validateApplyRequest (validRequest(), validOptions()), juce::String());

        beginTest ("validateApplyRequest reports each token");
        {
            auto opts = validOptions();
            opts.deviceExists = false;
            expectEquals (validateApplyRequest (validRequest(), opts), juce::String ("device-unavailable"));

            auto r = validRequest(); r.sampleRate = 96000.0;
            expectEquals (validateApplyRequest (r, validOptions()), juce::String ("sample-rate-unsupported"));

            r = validRequest(); r.bufferSize = 333;
            expectEquals (validateApplyRequest (r, validOptions()), juce::String ("buffer-size-unsupported"));

            r = validRequest(); r.midiPortId = "gone";
            expectEquals (validateApplyRequest (r, validOptions()), juce::String ("midi-port-unavailable"));
        }

        beginTest ("validateApplyRequest checks device, rate, buffer, port in that order");
        {
            auto r = validRequest(); r.sampleRate = 1.0; r.bufferSize = 1; r.midiPortId = "gone";
            auto noDevice = validOptions(); noDevice.deviceExists = false;
            expectEquals (validateApplyRequest (r, noDevice), juce::String ("device-unavailable"));
            expectEquals (validateApplyRequest (r, validOptions()), juce::String ("sample-rate-unsupported"));
            r.sampleRate = 48000.0;
            expectEquals (validateApplyRequest (r, validOptions()), juce::String ("buffer-size-unsupported"));
            r.bufferSize = 256;
            expectEquals (validateApplyRequest (r, validOptions()), juce::String ("midi-port-unavailable"));
        }

        beginTest ("validateApplyRequest always accepts the None port id");
        {
            auto r = validRequest(); r.midiPortId = {};
            expectEquals (validateApplyRequest (r, validOptions()), juce::String());
            auto noPorts = validOptions(); noPorts.midiPorts.clear();
            expectEquals (validateApplyRequest (r, noPorts), juce::String());
        }
    }

    static SavedSettings sample()
    {
        SavedSettings s;
        s.audioState = std::make_unique<juce::XmlElement> ("DEVICESETUP");
        s.audioState->setAttribute ("deviceType", "Windows Audio");
        s.audioState->setAttribute ("audioOutputDeviceName", "Speakers");
        s.port = { false, "p1", "Synth A" };
        s.channel = 5;
        return s;
    }

    void expectDefaults (const SavedSettings& s, const juce::String& what)
    {
        expectEquals (s.channel, 1, what);
        expect (s.audioState == nullptr, what);
        expect (! s.port.none, what);
        expectEquals (s.port.id, juce::String(), what);
        expectEquals (s.port.name, juce::String(), what);
    }

    void testXml()
    {
        beginTest ("settings XML round trip keeps audio state, port id and name, channel");
        {
            const auto xml = settingsToXml (sample());
            expect (xml != nullptr);
            if (xml == nullptr)
                return;

            expectEquals (xml->getTagName(), juce::String ("BerlinSettings"));
            expectEquals (xml->getIntAttribute ("version"), 1);

            const auto parsed = juce::parseXML (xml->toString());
            const auto s = settingsFromXml (parsed.get());
            expectEquals (s.channel, 5);
            expect (! s.port.none);
            expectEquals (s.port.id, juce::String ("p1"));
            expectEquals (s.port.name, juce::String ("Synth A"));
            expect (s.audioState != nullptr);
            if (s.audioState != nullptr)
            {
                expectEquals (s.audioState->getTagName(), juce::String ("DEVICESETUP"));
                expectEquals (s.audioState->getStringAttribute ("audioOutputDeviceName"), juce::String ("Speakers"));
            }
        }

        beginTest ("an explicit None is serialized as none=1 with no id and survives the round trip");
        {
            SavedSettings s;
            s.port.none = true;
            s.channel = 2;
            const auto xml = settingsToXml (s);
            expect (xml != nullptr);
            if (xml == nullptr)
                return;

            auto* midi = xml->getChildByName ("MIDI");
            expect (midi != nullptr);
            if (midi != nullptr)
            {
                expectEquals (midi->getIntAttribute ("none"), 1);
                expect (! midi->hasAttribute ("id"));
            }
            const auto back = settingsFromXml (xml.get());
            expect (back.port.none);
            expectEquals (back.channel, 2);
            expect (back.audioState == nullptr);
        }

        beginTest ("missing, non-XML, wrong-root, unknown-version and invalid-channel inputs yield defaults");
        {
            expectDefaults (settingsFromXml (nullptr), "null");
            expectDefaults (settingsFromXml (juce::parseXML ("").get()), "empty");
            expectDefaults (settingsFromXml (juce::parseXML ("not xml at all <<<").get()), "garbage");
            expectDefaults (settingsFromXml (juce::parseXML ("<Other version=\"1\"><MIDI channel=\"5\"/></Other>").get()), "wrong root");
            expectDefaults (settingsFromXml (juce::parseXML ("<BerlinSettings version=\"2\"><MIDI channel=\"5\" id=\"x\"/></BerlinSettings>").get()), "version 2");
            expectDefaults (settingsFromXml (juce::parseXML ("<BerlinSettings><MIDI channel=\"5\"/></BerlinSettings>").get()), "no version");
        }

        beginTest ("a saved channel outside 1-16 falls back to 1 but keeps the rest");
        {
            const auto xml = juce::parseXML ("<BerlinSettings version=\"1\"><MIDI id=\"p1\" name=\"A\" channel=\"99\"/></BerlinSettings>");
            const auto s = settingsFromXml (xml.get());
            expectEquals (s.channel, 1);
            expectEquals (s.port.id, juce::String ("p1"));
        }

        beginTest ("loadSettingsFile returns defaults for a missing or corrupt file and round trips a written one");
        {
            const auto dir = scratchDir();
            dir.createDirectory();
            expectDefaults (loadSettingsFile (dir.getChildFile ("does-not-exist.xml")), "missing file");

            const auto bad = dir.getChildFile ("corrupt.xml");
            bad.replaceWithText ("{{ not xml");
            expectDefaults (loadSettingsFile (bad), "corrupt file");

            const auto good = dir.getChildFile ("sub").getChildFile ("settings.xml");
            expect (writeSettingsFile (good, sample()));
            const auto s = loadSettingsFile (good);
            expectEquals (s.channel, 5);
            expectEquals (s.port.id, juce::String ("p1"));
            dir.deleteRecursively();
        }
    }

    void testResolve()
    {
        juce::Array<MidiPort> ports;
        ports.add ({ "p1", "Synth A" });
        ports.add ({ "p2", "Synth B" });

        beginTest ("resolveMidiPort: a saved None stays None even with ports available");
        {
            MidiPortChoice c; c.none = true; c.id = "p1";
            expectEquals (resolveMidiPort (c, ports), juce::String());
        }

        beginTest ("resolveMidiPort: a saved id that is present is chosen");
        expectEquals (resolveMidiPort ({ false, "p2", "Synth B" }, ports), juce::String ("p2"));

        beginTest ("resolveMidiPort: a missing id falls back to a port with the saved name");
        expectEquals (resolveMidiPort ({ false, "old-id", "Synth B" }, ports), juce::String ("p2"));

        beginTest ("resolveMidiPort: neither id nor name found yields the first port");
        expectEquals (resolveMidiPort ({ false, "gone", "Gone" }, ports), juce::String ("p1"));

        beginTest ("resolveMidiPort: no saved choice yields the first port; zero ports yields None");
        {
            expectEquals (resolveMidiPort ({}, ports), juce::String ("p1"));
            expectEquals (resolveMidiPort ({ false, "p1", "Synth A" }, {}), juce::String());
        }
    }

    void testPortToPersist()
    {
        juce::Array<MidiPort> ports;
        ports.add ({ "p1", "Synth A" });

        beginTest ("portToPersist: an unavailable saved port stays the preference, not None");
        {
            const auto c = portToPersist ({}, { false, "gone", "Old Synth" }, ports);
            expect (! c.none);
            expectEquals (c.id, juce::String ("gone"));
            expectEquals (c.name, juce::String ("Old Synth"));
        }

        beginTest ("portToPersist: an explicit None stays None");
        expect (portToPersist ({}, chosenPortChoice ({}, ports), ports).none);

        beginTest ("portToPersist: an open port is persisted with its name");
        {
            const auto c = portToPersist ("p1", { true, {}, {} }, ports);
            expect (! c.none);
            expectEquals (c.id, juce::String ("p1"));
            expectEquals (c.name, juce::String ("Synth A"));
        }
    }

    void testPayloads()
    {
        beginTest ("audioSettingsToVar uses the contract field names, lists None first and has no volume");
        {
            AudioSettingsView v;
            v.deviceTypes = { "Windows Audio", "ASIO" };
            v.deviceType = "ASIO";
            v.devices = { "Dev1", "Dev2" };
            v.device = "Dev2";
            v.sampleRates = { 44100.0, 48000.0 };
            v.sampleRate = 48000.0;
            v.bufferSizes = { 128, 256 };
            v.bufferSize = 256;
            v.midiPorts.add ({ "p1", "Synth A" });
            v.midiPortId = "p1";
            v.midiChannel = 3;

            const auto var = audioSettingsToVar (v);
            auto* o = var.getDynamicObject();
            expect (o != nullptr);
            if (o == nullptr)
                return;

            for (const auto* key : { "deviceTypes", "deviceType", "devices", "device", "sampleRates", "sampleRate",
                                     "bufferSizes", "bufferSize", "midiPorts", "midiPortId", "midiChannel" })
                expect (o->hasProperty (key), juce::String ("has ") + key);

            expectEquals (o->getProperties().size(), 11);   // nothing else, in particular no volume
            expect (! o->hasProperty ("volume"));
            expectEquals (var["deviceType"].toString(), juce::String ("ASIO"));
            expectEquals (var["deviceTypes"].size(), 2);
            expectEquals (var["sampleRates"].size(), 2);
            expectEquals (static_cast<double> (var["sampleRate"]), 48000.0);
            expectEquals (static_cast<int> (var["bufferSize"]), 256);
            expectEquals (static_cast<int> (var["midiChannel"]), 3);
            expectEquals (var["midiPortId"].toString(), juce::String ("p1"));

            const auto ports = var["midiPorts"];
            expectEquals (ports.size(), 2);
            expectEquals (ports[0]["id"].toString(), juce::String());
            expectEquals (ports[0]["name"].toString(), juce::String ("None"));
            expectEquals (ports[1]["id"].toString(), juce::String ("p1"));
            expectEquals (ports[1]["name"].toString(), juce::String ("Synth A"));
        }

        beginTest ("okResult is {ok:true} with no snapshot; errorResult is {ok:false,error:token}");
        {
            const auto ok = okResult();
            expect (ok.getDynamicObject() != nullptr);
            expect (static_cast<bool> (ok["ok"]));
            if (ok.getDynamicObject() != nullptr)
            {
                expect (! ok.getDynamicObject()->hasProperty ("snapshot"));
                expect (! ok.getDynamicObject()->hasProperty ("error"));
            }

            const auto err = errorResult ("busy");
            expect (err.getDynamicObject() != nullptr);
            if (err.getDynamicObject() != nullptr)
                expect (err.getDynamicObject()->hasProperty ("ok"));
            expect (! static_cast<bool> (err["ok"]));
            expectEquals (err["error"].toString(), juce::String ("busy"));
        }
    }

    void testWriteFailure()
    {
        beginTest ("writeSettingsFile returns false instead of throwing when the directory is not writable");
        {
            const auto dir = scratchDir();
            dir.createDirectory();
            const auto blocker = dir.getChildFile ("blocker");
            blocker.replaceWithText ("a file where a directory is needed");
            expect (! writeSettingsFile (blocker.getChildFile ("settings.xml"), sample()));
            dir.deleteRecursively();
        }
    }
};

static StandaloneSettingsTests standaloneSettingsTests;
