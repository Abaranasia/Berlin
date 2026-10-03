/*
  ==============================================================================

   UiBridge - out-of-line definitions (ui-bridge spec, design.md D9-D13).

   Field-descriptor tables (D9) drive BOTH setPatch/setGenerationParams
   decoding AND snapshot() encoding for the plain numeric/bool fields, so the
   two directions cannot drift apart. The 5 enum fields (waveform,
   lfoDestination, delayDivision, mode, scaleType) are handled directly,
   reusing PresetManager's shared name tables (D8) for 3 of them; RhythmMode
   is NOT persisted anywhere else, so its name<->enum mapping lives only
   here (D8's note).

   Every handler parses and validates ALL of its args into LOCALS - a copy of
   the patch/generationParams, or plain locals for scalar commands - before
   calling a single BerlinAudioProcessor mutator. A validation failure
   returns an error token with NO mutator ever called, so dispatch() can
   build its {ok:false, error} result without any snapshot of state that was
   never touched.

  ==============================================================================
*/

#include "UiBridge.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>

#include "generation/SequenceBuilder.h"   // normalizePitchRange
#include "plugin/BerlinAudioProcessor.h"

namespace berlin
{

namespace
{
    //==========================================================================
    // ---- D9: field-descriptor tables (pointer-to-member + bounds) ----------

    template <typename Owner>
    struct FloatField
    {
        const char* name;
        float Owner::* member;
        float lo;
        float hi;
    };

    template <typename Owner>
    struct IntField
    {
        const char* name;
        int Owner::* member;
        int lo;
        int hi;
    };

    template <typename Owner>
    struct BoolField
    {
        const char* name;
        bool Owner::* member;
    };

    constexpr FloatField<SynthPatch> kPatchFloatFields[] =
    {
        { "cutoffHz",         &SynthPatch::cutoffHz,         kMinCutoffHz,         kMaxCutoffHz },
        { "resonance",        &SynthPatch::resonance,        kMinResonance,        kMaxResonance },
        { "pulseWidth",       &SynthPatch::pulseWidth,       kMinPulseWidth,       kMaxPulseWidth },
        { "attack",           &SynthPatch::attack,           kMinAttackSeconds,    kMaxAttackSeconds },
        { "decay",            &SynthPatch::decay,            kMinDecaySeconds,     kMaxDecaySeconds },
        { "sustain",          &SynthPatch::sustain,          kMinSustain,          kMaxSustain },
        { "release",          &SynthPatch::release,          kMinReleaseSeconds,   kMaxReleaseSeconds },
        { "lfoRateHz",        &SynthPatch::lfoRateHz,        kMinLfoRateHz,        kMaxLfoRateHz },
        { "lfoDepth",         &SynthPatch::lfoDepth,         kMinLfoDepth,         kMaxLfoDepth },
        { "delayTimeSeconds", &SynthPatch::delayTimeSeconds, kMinDelayTimeSeconds, kMaxDelaySeconds },
        { "delayFeedback",    &SynthPatch::delayFeedback,    kMinDelayFeedback,    kMaxDelayFeedback },
        { "delayMix",         &SynthPatch::delayMix,         kMinDelayMix,         kMaxDelayMix },
        { "reverbRoomSize",   &SynthPatch::reverbRoomSize,   kMinReverbRoomSize,   kMaxReverbRoomSize },
        { "reverbDamping",    &SynthPatch::reverbDamping,    kMinReverbDamping,    kMaxReverbDamping },
        { "reverbWetLevel",   &SynthPatch::reverbWetLevel,   kMinReverbWetLevel,   kMaxReverbWetLevel },
        { "reverbDryLevel",   &SynthPatch::reverbDryLevel,   kMinReverbDryLevel,   kMaxReverbDryLevel },
        { "outputLevel",      &SynthPatch::outputLevel,      kMinOutputLevel,      kMaxOutputLevel },
    };

    constexpr BoolField<SynthPatch> kPatchBoolFields[] =
    {
        { "delaySynced", &SynthPatch::delaySynced },
    };

    constexpr IntField<GenerationParams> kGenerationIntFields[] =
    {
        { "pulses",         &GenerationParams::pulses,         0, kNumSteps },
        { "rotation",       &GenerationParams::rotation,       0, kNumSteps - 1 },
        { "rootPitchClass", &GenerationParams::rootPitchClass, 0, 11 },
        { "rangeLow",       &GenerationParams::rangeLow,       kMinPitch, kMaxPitch },
        { "rangeHigh",      &GenerationParams::rangeHigh,      kMinPitch, kMaxPitch },
    };

    constexpr FloatField<GenerationParams> kGenerationFloatFields[] =
    {
        { "stepProbability", &GenerationParams::stepProbability, 0.0f, 1.0f },
    };

    constexpr BoolField<GenerationParams> kGenerationBoolFields[] =
    {
        { "lockSeed", &GenerationParams::lockSeed },
    };

    constexpr int kMinAutoEvolveRate = 1, kMaxAutoEvolveRate = 16;

    //==========================================================================
    // ---- RhythmMode name<->enum (D8's note: NOT persisted, so it lives here
    //      only - no PresetManager table for it). ----------------------------

    juce::String rhythmModeName (RhythmMode mode)
    {
        switch (mode)
        {
            case RhythmMode::random:      return "random";
            case RhythmMode::euclidean:   return "euclidean";
            case RhythmMode::probability: return "probability";
        }

        return "random";   // unreachable for a valid enumerator
    }

    bool parseRhythmMode (const juce::String& name, RhythmMode& out)
    {
        if (name == "random")      { out = RhythmMode::random;      return true; }
        if (name == "euclidean")   { out = RhythmMode::euclidean;   return true; }
        if (name == "probability") { out = RhythmMode::probability; return true; }
        return false;
    }

    //==========================================================================
    // ---- Error-token mapping (D11) for the two non-UiBridge-owned enums. ---

    juce::String presetResultToken (PresetResult result)
    {
        switch (result)
        {
            case PresetResult::ok:                  return {};
            case PresetResult::nameInvalid:          return "nameInvalid";
            case PresetResult::directoryUnavailable: return "directoryUnavailable";
            case PresetResult::writeFailed:          return "writeFailed";
            case PresetResult::fileNotFound:         return "fileNotFound";
            case PresetResult::parseFailed:          return "parseFailed";
            case PresetResult::unsupportedVersion:   return "unsupportedVersion";
            case PresetResult::busy:                 return "busy";
        }

        return "parseFailed";   // unreachable for a valid enumerator
    }

    juce::String midiFileWriteResultToken (MidiFileWriteResult result)
    {
        switch (result)
        {
            case MidiFileWriteResult::ok:              return {};
            case MidiFileWriteResult::invalidTimeline: return "invalidTimeline";
            case MidiFileWriteResult::pathUnavailable: return "pathUnavailable";
            case MidiFileWriteResult::writeFailed:     return "writeFailed";
        }

        return "writeFailed";   // unreachable for a valid enumerator
    }

    //==========================================================================
    // ---- D10: argument-rule helpers. Every one returns an error token, or an
    //      empty String on success - never throws, never asserts. -----------

    bool isNumericVar (const juce::var& value)
    {
        return value.isInt() || value.isInt64() || value.isDouble();
    }

    juce::String readFiniteNumber (const juce::var& args, const char* field, double& out)
    {
        const juce::var& value = args[field];

        if (! isNumericVar (value))
            return juce::String ("invalid type: ") + field;

        out = static_cast<double> (value);

        if (! std::isfinite (out))
            return juce::String ("non-finite: ") + field;

        return {};
    }

    juce::String readRequiredNumber (const juce::var& args, const char* field, double& out)
    {
        if (! args.hasProperty (field))
            return juce::String ("missing arg: ") + field;

        return readFiniteNumber (args, field, out);
    }

    juce::String readRequiredBool (const juce::var& args, const char* field, bool& out)
    {
        if (! args.hasProperty (field))
            return juce::String ("missing arg: ") + field;

        const juce::var& value = args[field];

        if (! value.isBool())
            return juce::String ("invalid type: ") + field;

        out = static_cast<bool> (value);
        return {};
    }

    juce::String readRequiredString (const juce::var& args, const char* field, juce::String& out)
    {
        if (! args.hasProperty (field))
            return juce::String ("missing arg: ") + field;

        const juce::var& value = args[field];

        if (! value.isString())
            return juce::String ("invalid type: ") + field;

        out = value.toString();
        return {};
    }

    // Int fields use lround AFTER the clamp (D10) - the raw value must still
    // be a finite number first (reuses readRequiredNumber's own type/finite checks).
    juce::String readClampedInt (const juce::var& args, const char* field, int lo, int hi, int& out)
    {
        double raw = 0.0;

        if (auto err = readRequiredNumber (args, field, raw); err.isNotEmpty())
            return err;

        const double clamped = std::clamp (raw, static_cast<double> (lo), static_cast<double> (hi));
        out = static_cast<int> (std::lround (clamped));
        return {};
    }

    // Seed is a decimal string (-?[0-9]+), parsed via strtoll with an
    // end-pointer and ERANGE check (D10) - round-trips as a string in both
    // directions since int64 exceeds JS's safe-integer range.
    juce::String readSeed (const juce::var& args, juce::int64& out)
    {
        juce::String text;

        if (auto err = readRequiredString (args, "seed", text); err.isNotEmpty())
            return err;

        // REL-2/READ-2: strtoll alone accepts leading '+' and whitespace.
        const int digitsStart = (text.isNotEmpty() && text[0] == '-') ? 1 : 0;
        bool strict = digitsStart < text.length();
        for (int i = digitsStart; strict && i < text.length(); ++i)
            strict = text[i] >= '0' && text[i] <= '9';
        if (! strict)
            return "invalid type: seed";

        const auto* utf8 = text.toRawUTF8();
        char* end = nullptr;
        errno = 0;
        const long long parsed = std::strtoll (utf8, &end, 10);

        if (end == utf8 || *end != '\0' || errno == ERANGE)
            return "invalid type: seed";

        out = static_cast<juce::int64> (parsed);
        return {};
    }

    //==========================================================================
    // ---- setPatch / setGenerationParams: unknown-field detection + decode --

    bool isKnownPatchField (const juce::String& name)
    {
        static const std::initializer_list<const char*> known =
        {
            "waveform", "cutoffHz", "resonance", "pulseWidth", "attack", "decay", "sustain", "release",
            "lfoRateHz", "lfoDepth", "lfoDestination", "delayTimeSeconds", "delayFeedback", "delayMix",
            "reverbRoomSize", "reverbDamping", "reverbWetLevel", "reverbDryLevel", "outputLevel",
            "delaySynced", "delayDivision",
        };

        for (auto* k : known)
            if (name == k)
                return true;

        return false;
    }

    bool isKnownGenerationField (const juce::String& name)
    {
        static const std::initializer_list<const char*> known =
        {
            "mode", "pulses", "rotation", "stepProbability", "lockSeed",
            "scaleType", "rootPitchClass", "rangeLow", "rangeHigh",
        };

        for (auto* k : known)
            if (name == k)
                return true;

        return false;
    }

    juce::String decodePatchMerge (const juce::var& args, SynthPatch& patch)
    {
        if (args.getDynamicObject() == nullptr)
            return "invalid type: args";   // RES-1: reject, don't silently no-op

        if (auto* obj = args.getDynamicObject())
            for (auto& pair : obj->getProperties())
                if (! isKnownPatchField (pair.name.toString()))
                    return "unknown field: " + pair.name.toString();

        for (auto& f : kPatchFloatFields)
        {
            if (! args.hasProperty (f.name))
                continue;

            double value = 0.0;
            if (auto err = readFiniteNumber (args, f.name, value); err.isNotEmpty())
                return err;

            patch.*(f.member) = std::clamp (static_cast<float> (value), f.lo, f.hi);
        }

        for (auto& f : kPatchBoolFields)
        {
            if (! args.hasProperty (f.name))
                continue;

            bool value = false;
            if (auto err = readRequiredBool (args, f.name, value); err.isNotEmpty())
                return err;

            patch.*(f.member) = value;
        }

        if (args.hasProperty ("waveform"))
        {
            juce::String name;
            if (auto err = readRequiredString (args, "waveform", name); err.isNotEmpty())
                return err;

            Waveform parsed {};
            if (! PresetManager::parseWaveform (name, parsed))
                return "invalid enum: waveform";

            patch.waveform = parsed;
        }

        if (args.hasProperty ("lfoDestination"))
        {
            juce::String name;
            if (auto err = readRequiredString (args, "lfoDestination", name); err.isNotEmpty())
                return err;

            LfoDestination parsed {};
            if (! PresetManager::parseLfoDestination (name, parsed))
                return "invalid enum: lfoDestination";

            patch.lfoDestination = parsed;
        }

        if (args.hasProperty ("delayDivision"))
        {
            juce::String name;
            if (auto err = readRequiredString (args, "delayDivision", name); err.isNotEmpty())
                return err;

            SyncDivision parsed {};
            if (! PresetManager::parseDivision (name, parsed))
                return "invalid enum: delayDivision";

            patch.delayDivision = parsed;
        }

        return {};
    }

    juce::String decodeGenerationParamsMerge (const juce::var& args, GenerationParams& params)
    {
        if (args.getDynamicObject() == nullptr)
            return "invalid type: args";   // RES-1: reject, don't silently no-op

        if (auto* obj = args.getDynamicObject())
            for (auto& pair : obj->getProperties())
                if (! isKnownGenerationField (pair.name.toString()))
                    return "unknown field: " + pair.name.toString();

        for (auto& f : kGenerationIntFields)
        {
            if (! args.hasProperty (f.name))
                continue;

            int value = 0;
            if (auto err = readClampedInt (args, f.name, f.lo, f.hi, value); err.isNotEmpty())
                return err;

            params.*(f.member) = value;
        }

        for (auto& f : kGenerationFloatFields)
        {
            if (! args.hasProperty (f.name))
                continue;

            double value = 0.0;
            if (auto err = readFiniteNumber (args, f.name, value); err.isNotEmpty())
                return err;

            params.*(f.member) = std::clamp (static_cast<float> (value), f.lo, f.hi);
        }

        for (auto& f : kGenerationBoolFields)
        {
            if (! args.hasProperty (f.name))
                continue;

            bool value = false;
            if (auto err = readRequiredBool (args, f.name, value); err.isNotEmpty())
                return err;

            params.*(f.member) = value;
        }

        if (args.hasProperty ("mode"))
        {
            juce::String name;
            if (auto err = readRequiredString (args, "mode", name); err.isNotEmpty())
                return err;

            RhythmMode parsed {};
            if (! parseRhythmMode (name, parsed))
                return "invalid enum: mode";

            params.mode = parsed;
        }

        if (args.hasProperty ("scaleType"))
        {
            juce::String name;
            if (auto err = readRequiredString (args, "scaleType", name); err.isNotEmpty())
                return err;

            ScaleType parsed {};
            if (! PresetManager::parseScaleType (name, parsed))
                return "invalid enum: scaleType";

            params.scaleType = parsed;
        }

        // D12/REL-1: normalizePitchRange only when both are supplied; otherwise span-clamp the lone one.
        const bool rangeLowSupplied  = args.hasProperty ("rangeLow");
        const bool rangeHighSupplied = args.hasProperty ("rangeHigh");

        if (rangeLowSupplied && rangeHighSupplied)
            normalizePitchRange (params.rangeLow, params.rangeHigh);
        else if (rangeLowSupplied)
            params.rangeLow = std::clamp (params.rangeLow, kMinPitch, params.rangeHigh - kMinPitchRangeSpan);
        else if (rangeHighSupplied)
            params.rangeHigh = std::clamp (params.rangeHigh, params.rangeLow + kMinPitchRangeSpan, kMaxPitch);

        return {};
    }

    //==========================================================================
    // ---- snapshot() encoding (D9: the OTHER direction of the same tables) --

    juce::var encodePatch (const SynthPatch& patch)
    {
        auto* obj = new juce::DynamicObject();

        obj->setProperty ("waveform", PresetManager::waveformName (patch.waveform));

        for (auto& f : kPatchFloatFields)
            obj->setProperty (f.name, static_cast<double> (patch.*(f.member)));

        obj->setProperty ("lfoDestination", PresetManager::lfoDestinationName (patch.lfoDestination));

        for (auto& f : kPatchBoolFields)
            obj->setProperty (f.name, patch.*(f.member));

        obj->setProperty ("delayDivision", PresetManager::divisionName (patch.delayDivision));

        return juce::var (obj);
    }

    juce::var encodeGenerationParams (const GenerationParams& params)
    {
        auto* obj = new juce::DynamicObject();

        obj->setProperty ("mode", rhythmModeName (params.mode));

        for (auto& f : kGenerationIntFields)
            obj->setProperty (f.name, params.*(f.member));

        for (auto& f : kGenerationFloatFields)
            obj->setProperty (f.name, static_cast<double> (params.*(f.member)));

        for (auto& f : kGenerationBoolFields)
            obj->setProperty (f.name, params.*(f.member));

        obj->setProperty ("scaleType", PresetManager::scaleTypeName (params.scaleType));

        return juce::var (obj);
    }

    juce::var encodeSteps (const Sequence& sequence)
    {
        juce::Array<juce::var> steps;

        for (int i = 0; i < sequence.size(); ++i)
        {
            auto* stepObject = new juce::DynamicObject();
            stepObject->setProperty ("note", sequence[i].note);
            stepObject->setProperty ("active", sequence[i].active);
            steps.add (juce::var (stepObject));
        }

        return juce::var (steps);
    }

    //==========================================================================
    // ---- D9: the 15-entry dispatch table + its handlers. Each handler parses
    //      and validates everything into locals BEFORE calling any mutator. --

    juce::String handleSetPlaying (BerlinAudioProcessor& processor, const juce::var& args)
    {
        bool playing = false;
        if (auto err = readRequiredBool (args, "playing", playing); err.isNotEmpty())
            return err;

        processor.setPlaying (playing);
        return {};
    }

    juce::String handleSetBpm (BerlinAudioProcessor& processor, const juce::var& args)
    {
        double bpm = 0.0;
        if (auto err = readRequiredNumber (args, "bpm", bpm); err.isNotEmpty())
            return err;

        processor.setBpm (bpm);
        return {};
    }

    juce::String handleSetSeed (BerlinAudioProcessor& processor, const juce::var& args)
    {
        juce::int64 seed = 0;
        if (auto err = readSeed (args, seed); err.isNotEmpty())
            return err;

        processor.setSeed (seed);
        return {};
    }

    juce::String handleSetPatch (BerlinAudioProcessor& processor, const juce::var& args)
    {
        SynthPatch patch = processor.getPatch();
        if (auto err = decodePatchMerge (args, patch); err.isNotEmpty())
            return err;

        processor.setPatch (patch);   // also derives synced delay time (D7)
        return {};
    }

    juce::String handleSetGenerationParams (BerlinAudioProcessor& processor, const juce::var& args)
    {
        GenerationParams params = processor.getGenerationParams();
        if (auto err = decodeGenerationParamsMerge (args, params); err.isNotEmpty())
            return err;

        processor.setGenerationParams (params);   // commits immediately, no staging
        return {};
    }

    juce::String handleRegenerate (BerlinAudioProcessor& processor, const juce::var& args)
    {
        bool randomize = false;
        if (auto err = readRequiredBool (args, "randomize", randomize); err.isNotEmpty())
            return err;

        // D14: BerlinAudioProcessor::regenerate() itself checks busy BEFORE
        // drawing a seed, so a busy randomize draws no seed and changes nothing.
        return processor.regenerate (randomize) ? juce::String() : juce::String ("busy");
    }

    juce::String handleMutate (BerlinAudioProcessor& processor, const juce::var&)
    {
        return processor.mutate() ? juce::String() : juce::String ("busy");
    }

    juce::String handleSetSynthEnabled (BerlinAudioProcessor& processor, const juce::var& args)
    {
        bool enabled = false;
        if (auto err = readRequiredBool (args, "enabled", enabled); err.isNotEmpty())
            return err;

        processor.setSynthEnabled (enabled);
        return {};
    }

    juce::String handleSetEffectsEnabled (BerlinAudioProcessor& processor, const juce::var& args)
    {
        bool enabled = false;
        if (auto err = readRequiredBool (args, "enabled", enabled); err.isNotEmpty())
            return err;

        processor.setEffectsEnabled (enabled);
        return {};
    }

    juce::String handleSetMasterLevel (BerlinAudioProcessor& processor, const juce::var& args)
    {
        double level = 0.0;
        if (auto err = readRequiredNumber (args, "level", level); err.isNotEmpty())
            return err;

        processor.setMasterLevel (static_cast<float> (level));   // processor clamps
        return {};
    }

    juce::String handleSetAutoEvolveEnabled (BerlinAudioProcessor& processor, const juce::var& args)
    {
        bool enabled = false;
        if (auto err = readRequiredBool (args, "enabled", enabled); err.isNotEmpty())
            return err;

        processor.setAutoEvolveEnabled (enabled);
        return {};
    }

    juce::String handleSetAutoEvolveRate (BerlinAudioProcessor& processor, const juce::var& args)
    {
        int rate = kMinAutoEvolveRate;
        if (auto err = readClampedInt (args, "rate", kMinAutoEvolveRate, kMaxAutoEvolveRate, rate); err.isNotEmpty())
            return err;

        processor.setAutoEvolveRate (rate);
        return {};
    }

    juce::String handleSavePreset (BerlinAudioProcessor& processor, const juce::var& args)
    {
        juce::String name;
        if (auto err = readRequiredString (args, "name", name); err.isNotEmpty())
            return err;

        bool overwrite = false;
        if (auto err = readRequiredBool (args, "overwrite", overwrite); err.isNotEmpty())
            return err;

        if (! overwrite && processor.presetExists (name))
            return "exists";   // nothing written

        const auto result = processor.save (name);
        return result == PresetResult::ok ? juce::String() : presetResultToken (result);
    }

    juce::String handleLoadPreset (BerlinAudioProcessor& processor, const juce::var& args)
    {
        juce::String name;
        if (auto err = readRequiredString (args, "name", name); err.isNotEmpty())
            return err;

        const auto result = processor.loadPreset (name);
        return result == PresetResult::ok ? juce::String() : presetResultToken (result);
    }

    juce::String handleExportMidi (BerlinAudioProcessor& processor, const juce::var& args)
    {
        juce::String path;
        if (auto err = readRequiredString (args, "path", path); err.isNotEmpty())
            return err;

        // D13: juce::File's constructor asserts in debug on a relative path -
        // this check MUST come before any juce::File is constructed from `path`.
        if (! juce::File::isAbsolutePath (path))
            return "path not absolute";

        const auto result = processor.exportMidiTo (juce::File (path));
        return result == MidiFileWriteResult::ok ? juce::String() : midiFileWriteResultToken (result);
    }

    struct Command
    {
        const char* name;
        juce::String (*run) (BerlinAudioProcessor&, const juce::var&);
    };

    constexpr Command kCommands[] =
    {
        { "setPlaying",           handleSetPlaying },
        { "setBpm",               handleSetBpm },
        { "setSeed",              handleSetSeed },
        { "setPatch",             handleSetPatch },
        { "setGenerationParams",  handleSetGenerationParams },
        { "regenerate",           handleRegenerate },
        { "mutate",               handleMutate },
        { "setSynthEnabled",      handleSetSynthEnabled },
        { "setEffectsEnabled",    handleSetEffectsEnabled },
        { "setMasterLevel",       handleSetMasterLevel },
        { "setAutoEvolveEnabled", handleSetAutoEvolveEnabled },
        { "setAutoEvolveRate",    handleSetAutoEvolveRate },
        { "savePreset",           handleSavePreset },
        { "loadPreset",           handleLoadPreset },
        { "exportMidi",           handleExportMidi },
    };

    static_assert (sizeof (kCommands) / sizeof (kCommands[0]) == 15);
}

//==============================================================================
UiBridge::UiBridge (BerlinAudioProcessor& processorToControl) noexcept
    : processor (processorToControl)
{
}

juce::var UiBridge::dispatch (const juce::String& command, const juce::var& args)
{
    for (auto& entry : kCommands)
    {
        if (command != entry.name)
            continue;

        const juce::String error = entry.run (processor, args);

        auto* result = new juce::DynamicObject();

        if (error.isEmpty())
        {
            result->setProperty ("ok", true);
            result->setProperty ("error", juce::String());
            result->setProperty ("snapshot", snapshot());
        }
        else
        {
            result->setProperty ("ok", false);
            result->setProperty ("error", error);
        }

        return juce::var (result);
    }

    auto* result = new juce::DynamicObject();
    result->setProperty ("ok", false);
    result->setProperty ("error", "unknown command: " + command);
    return juce::var (result);
}

juce::var UiBridge::snapshot() const
{
    auto* obj = new juce::DynamicObject();

    obj->setProperty ("patch", encodePatch (processor.getPatch()));
    obj->setProperty ("generationParams", encodeGenerationParams (processor.getGenerationParams()));
    obj->setProperty ("seed", juce::String (processor.getSeed()));
    obj->setProperty ("bpm", processor.getBpm());
    obj->setProperty ("playing", processor.isPlaying());
    obj->setProperty ("playheadStep", processor.getPlayheadStep());
    obj->setProperty ("loopCount", processor.getLoopCount());
    obj->setProperty ("synthEnabled", processor.isSynthEnabled());
    obj->setProperty ("effectsEnabled", processor.areEffectsEnabled());
    obj->setProperty ("masterLevel", static_cast<double> (processor.getMasterLevel()));
    obj->setProperty ("autoEvolveEnabled", processor.isAutoEvolveEnabled());
    obj->setProperty ("autoEvolveRate", processor.getAutoEvolveRate());
    obj->setProperty ("mutationCount", processor.getMutationCount());
    obj->setProperty ("steps", encodeSteps (processor.getCurrentSequence()));

    juce::Array<juce::var> presetNames;
    for (auto& name : processor.listPresetNames())
        presetNames.add (name);
    obj->setProperty ("presetNames", presetNames);

    return juce::var (obj);
}

} // namespace berlin
