/*
  ==============================================================================

   UiBridge tests (ui-engine-api Phase 6, ui-bridge spec, design.md D9-D13).
   RED first: Source/bridge/UiBridge.h/.cpp do not exist in the Tests build
   yet, so this suite must fail to build until Phase 6's production file is
   registered and implemented.

   Headless: no audio device, no editor - same convention as
   BerlinAudioProcessorTests.cpp. Uses its OWN TempPresetDir copy (the
   original is file-local to BerlinAudioProcessorTests.cpp:30).

  ==============================================================================
*/

#include <cmath>
#include <limits>

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>
#include <juce_core/juce_core.h>

#include "bridge/UiBridge.h"
#include "plugin/BerlinAudioProcessor.h"

namespace
{
    struct TempPresetDir
    {
        TempPresetDir()
            : dir (juce::File::getSpecialLocation (juce::File::tempDirectory)
                       .getNonexistentChildFile ("UiBridgeTests", "", false))
        {
        }

        ~TempPresetDir()
        {
            dir.deleteRecursively();
        }

        juce::File dir;
    };

    // GIVEN/WHEN/THEN helper (ui-bridge spec's "Rejected Input Leaves State
    // Unchanged" requirement): every rejected dispatch must leave state
    // byte-identical, verified as JSON::toString(snapshot()) equality.
    juce::String snapshotText (berlin::UiBridge& bridge)
    {
        return juce::JSON::toString (bridge.snapshot());
    }
}

class UiBridgeTests final : public juce::UnitTest
{
public:
    UiBridgeTests() : juce::UnitTest ("UiBridge", "Berlin") {}

    void runTest() override
    {
        beginTest ("unknown command token leaves state unchanged (6.2)");
        {
            berlin::BerlinAudioProcessor processor;
            berlin::UiBridge bridge (processor);

            const auto before = snapshotText (bridge);

            const auto result = bridge.dispatch ("doesNotExist", juce::var (new juce::DynamicObject()));

            expect (! static_cast<bool> (result["ok"]));
            expect (result["error"].toString() == "unknown command: doesNotExist");
            expect (! result.hasProperty ("snapshot"));
            expect (snapshotText (bridge) == before);
        }

        beginTest ("generic invalid-argument matrix: missing/wrong-type/non-finite/unknown-field/bad-enum leave state unchanged (6.1, 6.3)");
        {
            berlin::BerlinAudioProcessor processor;
            berlin::UiBridge bridge (processor);

            auto checkRejected = [&] (const char* command, juce::DynamicObject::Ptr args, const char* expectedError)
            {
                const auto before = snapshotText (bridge);
                const auto result = bridge.dispatch (command, juce::var (args.get()));

                expect (! static_cast<bool> (result["ok"]), juce::String (command) + " should have failed");
                expect (result["error"].toString() == juce::String (expectedError),
                        juce::String (command) + " expected \"" + expectedError + "\" got \"" + result["error"].toString() + "\"");
                expect (! result.hasProperty ("snapshot"), juce::String (command) + " must not carry a snapshot on failure");
                expect (snapshotText (bridge) == before, juce::String (command) + " must leave state unchanged");
            };

            // missing arg
            checkRejected ("setBpm", new juce::DynamicObject(), "missing arg: bpm");

            // wrong type
            {
                auto args = new juce::DynamicObject();
                args->setProperty ("bpm", "fast");
                checkRejected ("setBpm", args, "invalid type: bpm");
            }

            // non-finite
            {
                auto args = new juce::DynamicObject();
                args->setProperty ("level", std::numeric_limits<double>::infinity());
                checkRejected ("setMasterLevel", args, "non-finite: level");
            }
            {
                auto args = new juce::DynamicObject();
                args->setProperty ("bpm", std::numeric_limits<double>::quiet_NaN());
                checkRejected ("setBpm", args, "non-finite: bpm");
            }

            // unknown field inside setPatch (protocol typo, not silently ignored)
            {
                auto args = new juce::DynamicObject();
                args->setProperty ("oscilatorWaveform", "square");
                checkRejected ("setPatch", args, "unknown field: oscilatorWaveform");
            }

            // unknown field inside setGenerationParams
            {
                auto args = new juce::DynamicObject();
                args->setProperty ("pulsess", 5);
                checkRejected ("setGenerationParams", args, "unknown field: pulsess");
            }

            // bad enum name
            {
                auto args = new juce::DynamicObject();
                args->setProperty ("waveform", "sawtooth");   // valid name is "saw"
                checkRejected ("setPatch", args, "invalid enum: waveform");
            }

            // wrong-type bool (string, not boolean)
            {
                auto args = new juce::DynamicObject();
                args->setProperty ("enabled", "yes");
                checkRejected ("setSynthEnabled", args, "invalid type: enabled");
            }

            // missing arg on a no-optional-fields command
            checkRejected ("setPlaying", new juce::DynamicObject(), "missing arg: playing");
            checkRejected ("setSeed", new juce::DynamicObject(), "missing arg: seed");
            checkRejected ("regenerate", new juce::DynamicObject(), "missing arg: randomize");
            checkRejected ("setSynthEnabled", new juce::DynamicObject(), "missing arg: enabled");
            checkRejected ("setEffectsEnabled", new juce::DynamicObject(), "missing arg: enabled");
            checkRejected ("setMasterLevel", new juce::DynamicObject(), "missing arg: level");
            checkRejected ("setAutoEvolveEnabled", new juce::DynamicObject(), "missing arg: enabled");
            checkRejected ("setAutoEvolveRate", new juce::DynamicObject(), "missing arg: rate");
            checkRejected ("savePreset", new juce::DynamicObject(), "missing arg: name");
            checkRejected ("loadPreset", new juce::DynamicObject(), "missing arg: name");
            checkRejected ("exportMidi", new juce::DynamicObject(), "missing arg: path");
        }

        beginTest ("valid round trip: setPlaying, setBpm, setSeed");
        {
            berlin::BerlinAudioProcessor processor;
            berlin::UiBridge bridge (processor);

            {
                auto args = new juce::DynamicObject();
                args->setProperty ("playing", false);
                const auto result = bridge.dispatch ("setPlaying", juce::var (args));
                expect (static_cast<bool> (result["ok"]));
                expect (result["error"].toString().isEmpty());
                expect (! static_cast<bool> (result["snapshot"]["playing"]));
            }

            {
                auto args = new juce::DynamicObject();
                args->setProperty ("bpm", 140.0);
                const auto result = bridge.dispatch ("setBpm", juce::var (args));
                expect (static_cast<bool> (result["ok"]));
                expectEquals ((double) result["snapshot"]["bpm"], 140.0);
            }

            {
                auto args = new juce::DynamicObject();
                args->setProperty ("seed", "9223372036854770000");   // 19-digit, round-trips as a string
                const auto result = bridge.dispatch ("setSeed", juce::var (args));
                expect (static_cast<bool> (result["ok"]));
                expect (result["snapshot"]["seed"].toString() == "9223372036854770000");
                expect (result["snapshot"]["seed"].isString());
            }
        }

        beginTest ("valid round trip: setPatch partial merge, enum encoding, synced delay recompute");
        {
            berlin::BerlinAudioProcessor processor;
            berlin::UiBridge bridge (processor);

            {
                auto args = new juce::DynamicObject();
                args->setProperty ("cutoffHz", 1200.0);
                args->setProperty ("waveform", "square");
                const auto result = bridge.dispatch ("setPatch", juce::var (args));
                expect (static_cast<bool> (result["ok"]));
                expectEquals ((double) result["snapshot"]["patch"]["cutoffHz"], 1200.0);
                expect (result["snapshot"]["patch"]["waveform"].toString() == "square");
                // omitted field preserved (resonance's default, kMinResonance 0.7071...8)
                expectWithinAbsoluteError ((double) result["snapshot"]["patch"]["resonance"], 0.7071068, 1.0e-5);
            }

            {
                auto bpmArgs = new juce::DynamicObject();
                bpmArgs->setProperty ("bpm", 120.0);
                bridge.dispatch ("setBpm", juce::var (bpmArgs));

                auto args = new juce::DynamicObject();
                args->setProperty ("delaySynced", true);
                args->setProperty ("delayDivision", "quarter");
                const auto result = bridge.dispatch ("setPatch", juce::var (args));
                expect (static_cast<bool> (result["ok"]));
                expectWithinAbsoluteError ((double) result["snapshot"]["patch"]["delayTimeSeconds"], 0.5, 1.0e-6);

                auto bpmArgs2 = new juce::DynamicObject();
                bpmArgs2->setProperty ("bpm", 150.0);
                const auto afterBpm = bridge.dispatch ("setBpm", juce::var (bpmArgs2));
                expectWithinAbsoluteError ((double) afterBpm["snapshot"]["patch"]["delayTimeSeconds"], 0.4, 1.0e-6);
            }
        }

        beginTest ("valid round trip: setGenerationParams partial merge, clamping, normalizePitchRange");
        {
            berlin::BerlinAudioProcessor processor;
            berlin::UiBridge bridge (processor);

            auto args = new juce::DynamicObject();
            args->setProperty ("pulses", 99);          // clamp to 16
            args->setProperty ("rotation", -5);        // clamp to 0
            args->setProperty ("stepProbability", 1.5);// clamp to 1.0
            args->setProperty ("rootPitchClass", 14);  // clamp to 11
            args->setProperty ("rangeLow", 10);
            args->setProperty ("rangeHigh", 15);       // span < kMinPitchRangeSpan -> normalizePitchRange widens it
            const auto result = bridge.dispatch ("setGenerationParams", juce::var (args));

            expect (static_cast<bool> (result["ok"]));
            expectEquals ((int) result["snapshot"]["generationParams"]["pulses"], 16);
            expectEquals ((int) result["snapshot"]["generationParams"]["rotation"], 0);
            expectEquals ((double) result["snapshot"]["generationParams"]["stepProbability"], 1.0);
            expectEquals ((int) result["snapshot"]["generationParams"]["rootPitchClass"], 11);

            const int low  = result["snapshot"]["generationParams"]["rangeLow"];
            const int high = result["snapshot"]["generationParams"]["rangeHigh"];
            expect (high - low >= 12, "normalizePitchRange must widen a sub-octave span to >= 12");
        }

        beginTest ("valid round trip: setSynthEnabled, setEffectsEnabled, setMasterLevel (clamp, not reject), setAutoEvolveEnabled, setAutoEvolveRate");
        {
            berlin::BerlinAudioProcessor processor;
            berlin::UiBridge bridge (processor);

            {
                auto args = new juce::DynamicObject();
                args->setProperty ("enabled", false);
                const auto result = bridge.dispatch ("setSynthEnabled", juce::var (args));
                expect (static_cast<bool> (result["ok"]));
                expect (! static_cast<bool> (result["snapshot"]["synthEnabled"]));
            }

            {
                auto args = new juce::DynamicObject();
                args->setProperty ("enabled", true);
                const auto result = bridge.dispatch ("setEffectsEnabled", juce::var (args));
                expect (static_cast<bool> (result["ok"]));
                expect (static_cast<bool> (result["snapshot"]["effectsEnabled"]));
            }

            {
                auto args = new juce::DynamicObject();
                args->setProperty ("level", 2.5);   // out-of-range FINITE -> clamp, not reject
                const auto result = bridge.dispatch ("setMasterLevel", juce::var (args));
                expect (static_cast<bool> (result["ok"]));
                expectEquals ((double) result["snapshot"]["masterLevel"], 1.0);
            }

            {
                auto args = new juce::DynamicObject();
                args->setProperty ("enabled", true);
                const auto result = bridge.dispatch ("setAutoEvolveEnabled", juce::var (args));
                expect (static_cast<bool> (result["ok"]));
                expect (static_cast<bool> (result["snapshot"]["autoEvolveEnabled"]));
            }

            {
                auto args = new juce::DynamicObject();
                args->setProperty ("rate", 99);   // clamp to [1,16]
                const auto result = bridge.dispatch ("setAutoEvolveRate", juce::var (args));
                expect (static_cast<bool> (result["ok"]));
                expectEquals ((int) result["snapshot"]["autoEvolveRate"], 16);
            }

            // Stop the auto-evolve Timer before the processor is destroyed.
            auto offArgs = new juce::DynamicObject();
            offArgs->setProperty ("enabled", false);
            bridge.dispatch ("setAutoEvolveEnabled", juce::var (offArgs));
        }

        beginTest ("busy regenerate/mutate -> ok:false \"busy\", unchanged; busy randomize draws no seed, snapshot byte-identical (6.5)");
        {
            berlin::BerlinAudioProcessor processor;
            berlin::UiBridge bridge (processor);

            {
                auto seedArgs = new juce::DynamicObject();
                seedArgs->setProperty ("seed", "42");
                bridge.dispatch ("setSeed", juce::var (seedArgs));
            }

            auto falseArgs = new juce::DynamicObject();
            falseArgs->setProperty ("randomize", false);
            const auto first = bridge.dispatch ("regenerate", juce::var (falseArgs));
            expect (static_cast<bool> (first["ok"]));   // first publish succeeds, now pending/unadopted

            const auto before = snapshotText (bridge);

            auto busyFalseArgs = new juce::DynamicObject();
            busyFalseArgs->setProperty ("randomize", false);
            const auto busyRegenerate = bridge.dispatch ("regenerate", juce::var (busyFalseArgs));
            expect (! static_cast<bool> (busyRegenerate["ok"]));
            expect (busyRegenerate["error"].toString() == "busy");
            expect (snapshotText (bridge) == before);

            auto busyTrueArgs = new juce::DynamicObject();
            busyTrueArgs->setProperty ("randomize", true);   // MUST NOT draw a new seed before the busy check
            const auto busyRandomize = bridge.dispatch ("regenerate", juce::var (busyTrueArgs));
            expect (! static_cast<bool> (busyRandomize["ok"]));
            expect (busyRandomize["error"].toString() == "busy");
            expect (snapshotText (bridge) == before, "busy randomize must leave the full snapshot (seed included) byte-identical");

            const auto busyMutate = bridge.dispatch ("mutate", juce::var (new juce::DynamicObject()));
            expect (! static_cast<bool> (busyMutate["ok"]));
            expect (busyMutate["error"].toString() == "busy");
            expect (snapshotText (bridge) == before);
        }

        beginTest ("lockSeed true keeps the seed on randomize; false changes it (6.6)");
        {
            {
                berlin::BerlinAudioProcessor processor;   // fresh processor: no pending publish
                berlin::UiBridge bridge (processor);

                auto seedArgs = new juce::DynamicObject();
                seedArgs->setProperty ("seed", "42");
                bridge.dispatch ("setSeed", juce::var (seedArgs));

                auto lockArgs = new juce::DynamicObject();
                lockArgs->setProperty ("lockSeed", true);
                lockArgs->setProperty ("mode", "random");
                lockArgs->setProperty ("pulses", 5);
                lockArgs->setProperty ("rotation", 0);
                lockArgs->setProperty ("stepProbability", 0.5);
                lockArgs->setProperty ("scaleType", "minor");
                lockArgs->setProperty ("rootPitchClass", 0);
                lockArgs->setProperty ("rangeLow", 36);
                lockArgs->setProperty ("rangeHigh", 72);
                bridge.dispatch ("setGenerationParams", juce::var (lockArgs));

                auto randomizeArgs = new juce::DynamicObject();
                randomizeArgs->setProperty ("randomize", true);
                const auto result = bridge.dispatch ("regenerate", juce::var (randomizeArgs));
                expect (static_cast<bool> (result["ok"]));
                expect (result["snapshot"]["seed"].toString() == "42");
            }

            {
                berlin::BerlinAudioProcessor processor;
                berlin::UiBridge bridge (processor);

                auto seedArgs = new juce::DynamicObject();
                seedArgs->setProperty ("seed", "42");
                bridge.dispatch ("setSeed", juce::var (seedArgs));

                auto unlockArgs = new juce::DynamicObject();
                unlockArgs->setProperty ("lockSeed", false);
                unlockArgs->setProperty ("mode", "random");
                unlockArgs->setProperty ("pulses", 5);
                unlockArgs->setProperty ("rotation", 0);
                unlockArgs->setProperty ("stepProbability", 0.5);
                unlockArgs->setProperty ("scaleType", "minor");
                unlockArgs->setProperty ("rootPitchClass", 0);
                unlockArgs->setProperty ("rangeLow", 36);
                unlockArgs->setProperty ("rangeHigh", 72);
                bridge.dispatch ("setGenerationParams", juce::var (unlockArgs));

                auto randomizeArgs = new juce::DynamicObject();
                randomizeArgs->setProperty ("randomize", true);
                const auto result = bridge.dispatch ("regenerate", juce::var (randomizeArgs));
                expect (static_cast<bool> (result["ok"]));
                expect (result["snapshot"]["seed"].toString() != "42");
            }
        }

        beginTest ("savePreset exists -> \"exists\", no write; loadPreset missing -> \"fileNotFound\" (6.7)");
        {
            TempPresetDir temp;
            berlin::BerlinAudioProcessor processor (temp.dir);
            berlin::UiBridge bridge (processor);

            auto saveArgs = new juce::DynamicObject();
            saveArgs->setProperty ("name", "Lead A");
            saveArgs->setProperty ("overwrite", false);
            const auto firstSave = bridge.dispatch ("savePreset", juce::var (saveArgs));
            expect (static_cast<bool> (firstSave["ok"]));

            const auto before = snapshotText (bridge);

            auto saveAgainArgs = new juce::DynamicObject();
            saveAgainArgs->setProperty ("name", "Lead A");
            saveAgainArgs->setProperty ("overwrite", false);
            const auto rejectedSave = bridge.dispatch ("savePreset", juce::var (saveAgainArgs));
            expect (! static_cast<bool> (rejectedSave["ok"]));
            expect (rejectedSave["error"].toString() == "exists");
            expect (snapshotText (bridge) == before);

            auto overwriteArgs = new juce::DynamicObject();
            overwriteArgs->setProperty ("name", "Lead A");
            overwriteArgs->setProperty ("overwrite", true);
            const auto overwriteSave = bridge.dispatch ("savePreset", juce::var (overwriteArgs));
            expect (static_cast<bool> (overwriteSave["ok"]));

            auto loadArgs = new juce::DynamicObject();
            loadArgs->setProperty ("name", "Ghost");
            const auto loadMissing = bridge.dispatch ("loadPreset", juce::var (loadArgs));
            expect (! static_cast<bool> (loadMissing["ok"]));
            expect (loadMissing["error"].toString() == "fileNotFound");
        }

        beginTest ("snapshot presetNames reflects files added or removed outside the bridge on the next read (ui-bridge-parity 2.5)");
        {
            TempPresetDir temp;
            berlin::BerlinAudioProcessor processor (temp.dir);
            berlin::UiBridge bridge (processor);

            const auto names = [&bridge]
            {
                juce::StringArray result;
                const auto snapshot = bridge.snapshot();   // keep the var alive while its array is read
                const auto* array = snapshot["presetNames"].getArray();
                if (array != nullptr)
                    for (const auto& name : *array)
                        result.add (name.toString());
                return result;
            };

            expect (names().isEmpty());   // empty folder: nothing yet

            {
                berlin::BerlinAudioProcessor external (temp.dir);   // a second writer, not the bridge's processor
                expect (external.save ("External") == berlin::PresetResult::ok);
            }
            expect (names().contains ("External"));
            expectEquals (names().size(), 1);

            expect (processor.listPresetNames().contains ("External"));
            expect (juce::File (temp.dir).getChildFile ("External.xml").deleteFile());
            expect (names().isEmpty());   // removed file is dropped
        }

        beginTest ("loadPreset while regeneration is busy -> ok:false \"busy\", no snapshot, state unchanged; success path unchanged (ui-bridge-parity 3.3)");
        {
            TempPresetDir temp;
            berlin::BerlinAudioProcessor processor (temp.dir);
            berlin::UiBridge bridge (processor);

            auto saveArgs = new juce::DynamicObject();
            saveArgs->setProperty ("name", "Lead A");
            saveArgs->setProperty ("overwrite", false);
            expect (static_cast<bool> (bridge.dispatch ("savePreset", juce::var (saveArgs))["ok"]));

            auto loadArgs = new juce::DynamicObject();
            loadArgs->setProperty ("name", "Lead A");

            // Not pending yet: load succeeds and (like any regenerate) leaves a publish pending.
            const auto loaded = bridge.dispatch ("loadPreset", juce::var (loadArgs));
            expect (static_cast<bool> (loaded["ok"]));
            expect (loaded.hasProperty ("snapshot"));

            const auto before = snapshotText (bridge);
            auto busyArgs = new juce::DynamicObject();
            busyArgs->setProperty ("name", "Lead A");
            const auto busy = bridge.dispatch ("loadPreset", juce::var (busyArgs));
            expect (! static_cast<bool> (busy["ok"]));
            expect (busy["error"].toString() == "busy");
            expect (! busy.hasProperty ("snapshot"));
            expect (snapshotText (bridge) == before);
        }

        beginTest ("exportMidi rejects a relative path before constructing a juce::File, no file written (6.8, D13)");
        {
            berlin::BerlinAudioProcessor processor;
            berlin::UiBridge bridge (processor);

            const auto before = snapshotText (bridge);

            auto args = new juce::DynamicObject();
            args->setProperty ("path", "output.mid");   // relative
            const auto result = bridge.dispatch ("exportMidi", juce::var (args));

            expect (! static_cast<bool> (result["ok"]));
            expect (result["error"].toString() == "path not absolute");
            expect (snapshotText (bridge) == before);

            expect (! juce::File::getCurrentWorkingDirectory().getChildFile ("output.mid").existsAsFile());
        }

        beginTest ("exportMidi with an absolute path writes a file");
        {
            const juce::File tempFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                             .getNonexistentChildFile ("UiBridgeExportTest", ".mid", false);
            struct ScopedFileDeleter
            {
                explicit ScopedFileDeleter (juce::File f) : file (std::move (f)) {}
                ~ScopedFileDeleter() { file.deleteFile(); }
                juce::File file;
            } deleter (tempFile);

            berlin::BerlinAudioProcessor processor;
            berlin::UiBridge bridge (processor);

            auto args = new juce::DynamicObject();
            args->setProperty ("path", tempFile.getFullPathName());
            const auto result = bridge.dispatch ("exportMidi", juce::var (args));

            expect (static_cast<bool> (result["ok"]));
            expect (tempFile.existsAsFile());
        }

        beginTest ("snapshot shape: all documented top-level keys, 21 patch fields, 16-entry steps, seed as string (6.9)");
        {
            berlin::BerlinAudioProcessor processor;
            berlin::UiBridge bridge (processor);

            const auto snap = bridge.snapshot();

            for (auto* key : { "patch", "generationParams", "seed", "bpm", "playing", "playheadStep",
                                "loopCount", "synthEnabled", "effectsEnabled", "masterLevel",
                                "autoEvolveEnabled", "autoEvolveRate", "mutationCount", "steps", "presetNames" })
                expect (snap.hasProperty (key), juce::String ("snapshot missing key: ") + key);

            expect (snap["seed"].isString());

            auto* patchObject = snap["patch"].getDynamicObject();
            expect (patchObject != nullptr);
            expectEquals (patchObject->getProperties().size(), 21);

            for (auto* key : { "waveform", "cutoffHz", "resonance", "pulseWidth", "attack", "decay", "sustain",
                                "release", "lfoRateHz", "lfoDepth", "lfoDestination", "delayTimeSeconds",
                                "delayFeedback", "delayMix", "reverbRoomSize", "reverbDamping", "reverbWetLevel",
                                "reverbDryLevel", "outputLevel", "delaySynced", "delayDivision" })
                expect (snap["patch"].hasProperty (key), juce::String ("patch missing key: ") + key);

            auto* generationObject = snap["generationParams"].getDynamicObject();
            expect (generationObject != nullptr);
            expectEquals (generationObject->getProperties().size(), 9);

            for (auto* key : { "mode", "pulses", "rotation", "stepProbability", "lockSeed",
                                "scaleType", "rootPitchClass", "rangeLow", "rangeHigh" })
                expect (snap["generationParams"].hasProperty (key), juce::String ("generationParams missing key: ") + key);

            expect (snap["steps"].isArray());
            expectEquals (snap["steps"].size(), 16);

            for (int i = 0; i < 16; ++i)
            {
                expect (snap["steps"][i].hasProperty ("note"));
                expect (snap["steps"][i].hasProperty ("active"));
            }
        }

        beginTest ("decodeGenerationParamsMerge: omitted range endpoint unchanged, supplied one span-clamped (REL-1)");
        {
            struct Case { const char* field; int value; int expectLow; int expectHigh; };
            for (auto& c : { Case { "rangeLow", 65, 60, 72 }, Case { "rangeHigh", 40, 36, 48 }, Case { "pulses", 5, 36, 72 } })
            {
                berlin::BerlinAudioProcessor processor;
                berlin::UiBridge bridge (processor);
                auto base = new juce::DynamicObject();
                base->setProperty ("rangeLow", 36);
                base->setProperty ("rangeHigh", 72);
                bridge.dispatch ("setGenerationParams", juce::var (base));
                auto args = new juce::DynamicObject();
                args->setProperty (c.field, c.value);
                auto result = bridge.dispatch ("setGenerationParams", juce::var (args));
                expectEquals ((int) result["snapshot"]["generationParams"]["rangeLow"], c.expectLow);
                expectEquals ((int) result["snapshot"]["generationParams"]["rangeHigh"], c.expectHigh);
            }
        }

        beginTest ("setPatch/setGenerationParams reject non-object args, no state change (RES-1)");
        {
            berlin::BerlinAudioProcessor processor;
            berlin::UiBridge bridge (processor);
            const auto before = snapshotText (bridge);
            auto r1 = bridge.dispatch ("setPatch", juce::var (5));
            expect (! static_cast<bool> (r1["ok"]) && r1["error"].toString() == "invalid type: args");
            auto r2 = bridge.dispatch ("setGenerationParams", juce::var());
            expect (! static_cast<bool> (r2["ok"]) && r2["error"].toString() == "invalid type: args");
            expect (snapshotText (bridge) == before);
        }

        beginTest ("readSeed accepts only -?[0-9]+ (REL-2/READ-2)");
        {
            berlin::BerlinAudioProcessor processor;
            berlin::UiBridge bridge (processor);
            for (auto* bad : { " 42", "+42", "42 ", "" })
            {
                auto args = new juce::DynamicObject();
                args->setProperty ("seed", bad);
                expect (! static_cast<bool> (bridge.dispatch ("setSeed", juce::var (args))["ok"]), bad);
            }
            for (auto* good : { "-7", "42" })
            {
                auto args = new juce::DynamicObject();
                args->setProperty ("seed", good);
                expect (static_cast<bool> (bridge.dispatch ("setSeed", juce::var (args))["ok"]), good);
            }
        }

        beginTest ("waveform round-trips using the preset XML name");
        {
            berlin::BerlinAudioProcessor processor;
            berlin::UiBridge bridge (processor);

            auto args = new juce::DynamicObject();
            args->setProperty ("waveform", "saw");
            const auto result = bridge.dispatch ("setPatch", juce::var (args));
            expect (static_cast<bool> (result["ok"]));
            expect (bridge.snapshot()["patch"]["waveform"].toString() == "saw");
        }
    }
};

static UiBridgeTests uiBridgeTests;
