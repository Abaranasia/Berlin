/*
  ==============================================================================

   PresetManager serialization core tests (preset-persistence spec, roadmap
   Phase 11 / preset-system). RED first: Source/preset/PresetManager.h does
   not exist yet, so this suite must fail to compile until Phase 1's
   production files are created.

   Covers the PURE core only (no file I/O, that's PresetManagerFileTests.cpp):
   round-trip of all 11 live fields + seed through
   toValueTree -> toXmlString -> ValueTree::fromXml -> fromValueTree, asserted
   with == (not epsilon); the 8 effects fields equal kDefaultPatch after
   fromValueTree (Decision 5's structural-exclusion guarantee); the precision-
   regression case resonance = 0.7071068f (guards against var::toString()'s
   6-significant-digit trap); seed fidelity at INT64_MIN/INT64_MAX/0/-1 (the
   case that rules JSON out); each of the 9 continuous parameters' range
   endpoints; schemaVersion newer than supported -> unsupportedVersion;
   missing/garbage schemaVersion -> parseFailed; wrong root tag / missing
   <Synth>/<Generation> / missing attribute / unrecognized enum name ->
   parseFailed (Decision 4's reject branch), out always left untouched; an
   out-of-range continuous value -> clamped, loads ok (Decision 4's clamp
   branch, NOT a rejection).

  ==============================================================================
*/

#include <limits>

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

#include "preset/Preset.h"
#include "preset/PresetManager.h"
#include "synth/SynthPatch.h"

namespace
{
    // Serializes `in` through the real text pipeline (ValueTree -> XML string ->
    // ValueTree) and deserializes into `out`. Returns true iff fromValueTree
    // reported PresetResult::ok.
    bool roundTripThroughXml (const berlin::Preset& in, berlin::Preset& out)
    {
        const auto tree = berlin::PresetManager::toValueTree (in);
        const juce::String xmlText = tree.toXmlString();
        const auto parsedTree = juce::ValueTree::fromXml (xmlText);

        return berlin::PresetManager::fromValueTree (parsedTree, out) == berlin::PresetResult::ok;
    }
}

class PresetSerializationTests final : public juce::UnitTest
{
public:
    PresetSerializationTests() : juce::UnitTest ("PresetSerialization", "Berlin") {}

    void runTest() override
    {
        beginTest ("round trip preserves all 11 fields, seed, and pins the 8 effects fields to kDefaultPatch");
        {
            berlin::Preset in;
            in.name                     = "Acid Bass";
            in.patch.waveform           = berlin::Waveform::pulse;
            in.patch.cutoffHz           = 850.0f;
            in.patch.resonance          = 4.5f;
            in.patch.pulseWidth         = 0.42f;
            in.patch.attack             = 0.02f;
            in.patch.decay              = 0.33f;
            in.patch.sustain            = 0.61f;
            in.patch.release            = 1.25f;
            in.patch.lfoRateHz          = 6.5f;
            in.patch.lfoDepth           = 0.75f;
            in.patch.lfoDestination     = berlin::LfoDestination::cutoff;
            in.seed                     = 123456789LL;

            berlin::Preset out;
            expect (roundTripThroughXml (in, out));

            expect (out.name == in.name);
            expect (out.patch.waveform == in.patch.waveform);
            expect (out.patch.cutoffHz == in.patch.cutoffHz);
            expect (out.patch.resonance == in.patch.resonance);
            expect (out.patch.pulseWidth == in.patch.pulseWidth);
            expect (out.patch.attack == in.patch.attack);
            expect (out.patch.decay == in.patch.decay);
            expect (out.patch.sustain == in.patch.sustain);
            expect (out.patch.release == in.patch.release);
            expect (out.patch.lfoRateHz == in.patch.lfoRateHz);
            expect (out.patch.lfoDepth == in.patch.lfoDepth);
            expect (out.patch.lfoDestination == in.patch.lfoDestination);
            expect (out.seed == in.seed);

            // The 8 non-live effects fields are never written/read - always kDefaultPatch.
            expect (out.patch.delayTimeSeconds == berlin::kDefaultPatch.delayTimeSeconds);
            expect (out.patch.delayFeedback    == berlin::kDefaultPatch.delayFeedback);
            expect (out.patch.delayMix         == berlin::kDefaultPatch.delayMix);
            expect (out.patch.reverbRoomSize   == berlin::kDefaultPatch.reverbRoomSize);
            expect (out.patch.reverbDamping    == berlin::kDefaultPatch.reverbDamping);
            expect (out.patch.reverbWetLevel   == berlin::kDefaultPatch.reverbWetLevel);
            expect (out.patch.reverbDryLevel   == berlin::kDefaultPatch.reverbDryLevel);
            expect (out.patch.outputLevel      == berlin::kDefaultPatch.outputLevel);
        }

        beginTest ("precision regression: resonance = 0.7071068f round-trips bit-exact");
        {
            berlin::Preset in;
            in.patch.resonance = 0.7071068f;

            berlin::Preset out;
            expect (roundTripThroughXml (in, out));
            expect (out.patch.resonance == in.patch.resonance);
        }

        beginTest ("seed round-trips exactly at INT64_MIN, INT64_MAX, 0, and -1");
        {
            const juce::int64 seeds[] = { std::numeric_limits<juce::int64>::min(),
                                          std::numeric_limits<juce::int64>::max(),
                                          (juce::int64) 0,
                                          (juce::int64) -1 };

            for (auto seed : seeds)
            {
                berlin::Preset in;
                in.seed = seed;

                berlin::Preset out;
                expect (roundTripThroughXml (in, out));
                expect (out.seed == seed);
            }
        }

        beginTest ("each of the 9 continuous parameters' range endpoints round-trip bit-exact");
        {
            auto checkEndpoint = [this] (float berlin::SynthPatch::* field, float value)
            {
                berlin::Preset in;
                in.patch.*field = value;

                berlin::Preset out;
                expect (roundTripThroughXml (in, out));
                expect (out.patch.*field == value);
            };

            checkEndpoint (&berlin::SynthPatch::cutoffHz, berlin::kMinCutoffHz);
            checkEndpoint (&berlin::SynthPatch::cutoffHz, berlin::kMaxCutoffHz);
            checkEndpoint (&berlin::SynthPatch::resonance, berlin::kMinResonance);
            checkEndpoint (&berlin::SynthPatch::resonance, berlin::kMaxResonance);
            checkEndpoint (&berlin::SynthPatch::pulseWidth, berlin::kMinPulseWidth);
            checkEndpoint (&berlin::SynthPatch::pulseWidth, berlin::kMaxPulseWidth);
            checkEndpoint (&berlin::SynthPatch::attack, berlin::kMinAttackSeconds);
            checkEndpoint (&berlin::SynthPatch::attack, berlin::kMaxAttackSeconds);
            checkEndpoint (&berlin::SynthPatch::decay, berlin::kMinDecaySeconds);
            checkEndpoint (&berlin::SynthPatch::decay, berlin::kMaxDecaySeconds);
            checkEndpoint (&berlin::SynthPatch::sustain, berlin::kMinSustain);
            checkEndpoint (&berlin::SynthPatch::sustain, berlin::kMaxSustain);
            checkEndpoint (&berlin::SynthPatch::release, berlin::kMinReleaseSeconds);
            checkEndpoint (&berlin::SynthPatch::release, berlin::kMaxReleaseSeconds);
            checkEndpoint (&berlin::SynthPatch::lfoRateHz, berlin::kMinLfoRateHz);
            checkEndpoint (&berlin::SynthPatch::lfoRateHz, berlin::kMaxLfoRateHz);
            checkEndpoint (&berlin::SynthPatch::lfoDepth, berlin::kMinLfoDepth);
            checkEndpoint (&berlin::SynthPatch::lfoDepth, berlin::kMaxLfoDepth);
        }

        beginTest ("schemaVersion newer than supported is rejected as unsupportedVersion, out left untouched");
        {
            auto tree = berlin::PresetManager::toValueTree (berlin::Preset{});
            tree.setProperty ("schemaVersion", juce::String (berlin::PresetManager::kSchemaVersion + 1), nullptr);

            berlin::Preset out;
            out.name = "Sentinel";
            expect (berlin::PresetManager::fromValueTree (tree, out) == berlin::PresetResult::unsupportedVersion);
            expect (out.name == "Sentinel");
        }

        beginTest ("missing or garbage schemaVersion is parseFailed, out left untouched");
        {
            auto treeMissing = berlin::PresetManager::toValueTree (berlin::Preset{});
            treeMissing.removeProperty ("schemaVersion", nullptr);

            berlin::Preset outMissing;
            outMissing.name = "Sentinel";
            expect (berlin::PresetManager::fromValueTree (treeMissing, outMissing) == berlin::PresetResult::parseFailed);
            expect (outMissing.name == "Sentinel");

            auto treeGarbage = berlin::PresetManager::toValueTree (berlin::Preset{});
            treeGarbage.setProperty ("schemaVersion", "not-a-number", nullptr);

            berlin::Preset outGarbage;
            outGarbage.name = "Sentinel";
            expect (berlin::PresetManager::fromValueTree (treeGarbage, outGarbage) == berlin::PresetResult::parseFailed);
            expect (outGarbage.name == "Sentinel");
        }

        beginTest ("an invalid/unparseable ValueTree (e.g. from malformed XML text) is parseFailed, out left untouched");
        {
            const auto invalidTree = juce::ValueTree::fromXml (juce::String ("not xml at all <<<"));

            berlin::Preset out;
            out.name = "Sentinel";
            expect (berlin::PresetManager::fromValueTree (invalidTree, out) == berlin::PresetResult::parseFailed);
            expect (out.name == "Sentinel");
        }

        beginTest ("wrong root element tag is rejected, out left untouched");
        {
            const auto tree = berlin::PresetManager::toValueTree (berlin::Preset{});

            juce::ValueTree wrongRoot ("NotAPreset");
            for (int i = 0; i < tree.getNumProperties(); ++i)
            {
                const auto propertyName = tree.getPropertyName (i);
                wrongRoot.setProperty (propertyName, tree.getProperty (propertyName), nullptr);
            }
            for (int i = 0; i < tree.getNumChildren(); ++i)
                wrongRoot.appendChild (tree.getChild (i).createCopy(), nullptr);

            berlin::Preset out;
            out.name = "Sentinel";
            expect (berlin::PresetManager::fromValueTree (wrongRoot, out) == berlin::PresetResult::parseFailed);
            expect (out.name == "Sentinel");
        }

        beginTest ("missing <Synth> section is rejected, out left untouched");
        {
            auto tree = berlin::PresetManager::toValueTree (berlin::Preset{});
            tree.removeChild (tree.getChildWithName ("Synth"), nullptr);

            berlin::Preset out;
            out.name = "Sentinel";
            expect (berlin::PresetManager::fromValueTree (tree, out) == berlin::PresetResult::parseFailed);
            expect (out.name == "Sentinel");
        }

        beginTest ("missing <Generation> section is rejected, out left untouched");
        {
            auto tree = berlin::PresetManager::toValueTree (berlin::Preset{});
            tree.removeChild (tree.getChildWithName ("Generation"), nullptr);

            berlin::Preset out;
            out.name = "Sentinel";
            expect (berlin::PresetManager::fromValueTree (tree, out) == berlin::PresetResult::parseFailed);
            expect (out.name == "Sentinel");
        }

        beginTest ("missing a required <Synth> attribute is rejected, out left untouched");
        {
            auto tree = berlin::PresetManager::toValueTree (berlin::Preset{});
            auto synthNode = tree.getChildWithName ("Synth");
            synthNode.removeProperty ("cutoffHz", nullptr);

            berlin::Preset out;
            out.name = "Sentinel";
            expect (berlin::PresetManager::fromValueTree (tree, out) == berlin::PresetResult::parseFailed);
            expect (out.name == "Sentinel");
        }

        beginTest ("unrecognized waveform enum name is rejected, out left untouched");
        {
            auto tree = berlin::PresetManager::toValueTree (berlin::Preset{});
            tree.getChildWithName ("Synth").setProperty ("waveform", "notARealWaveform", nullptr);

            berlin::Preset out;
            out.name = "Sentinel";
            expect (berlin::PresetManager::fromValueTree (tree, out) == berlin::PresetResult::parseFailed);
            expect (out.name == "Sentinel");
        }

        beginTest ("unrecognized lfoDestination enum name is rejected, out left untouched");
        {
            auto tree = berlin::PresetManager::toValueTree (berlin::Preset{});
            tree.getChildWithName ("Synth").setProperty ("lfoDestination", "notADestination", nullptr);

            berlin::Preset out;
            out.name = "Sentinel";
            expect (berlin::PresetManager::fromValueTree (tree, out) == berlin::PresetResult::parseFailed);
            expect (out.name == "Sentinel");
        }

        beginTest ("an out-of-range continuous value is clamped to its documented range, and the preset still loads ok");
        {
            berlin::Preset in;
            in.patch.cutoffHz = 99999.0f;   // beyond kMaxCutoffHz - simulates a hand-edited file

            const auto tree = berlin::PresetManager::toValueTree (in);

            berlin::Preset out;
            const auto result = berlin::PresetManager::fromValueTree (tree, out);
            expect (result == berlin::PresetResult::ok);
            expect (out.patch.cutoffHz == berlin::kMaxCutoffHz);
        }
    }
};

static PresetSerializationTests presetSerializationTests;
