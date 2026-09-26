/*
  ==============================================================================

   PresetManager - out-of-line definitions (preset-persistence spec).

  ==============================================================================
*/

#include "PresetManager.h"

#include "generation/SequenceBuilder.h"
#include "synth/SynthPatch.h"

namespace
{
    constexpr const char* kRootType       = "BerlinPreset";
    constexpr const char* kSynthType      = "Synth";
    constexpr const char* kGenerationType = "Generation";
    constexpr const char* kTransportType  = "Transport";   // schema v3 (tempo-delay-ui, design.md D7)

    const juce::StringArray& waveformNames()
    {
        static const juce::StringArray names { "saw", "square", "pulse", "triangle" };
        return names;
    }

    const juce::StringArray& lfoDestinationNames()
    {
        static const juce::StringArray names { "pitch", "cutoff", "amplitude", "pulseWidth" };
        return names;
    }

    // scale-aware-generation schema v2: mirrors waveformNames()'s "name string,
    // not a raw ordinal" convention (PresetManager.h's ScaleType-ordinals-are-
    // UI-only note). Order MUST match berlin::ScaleType's declaration order.
    const juce::StringArray& scaleNames()
    {
        static const juce::StringArray names { "minor", "major", "dorian", "phrygian", "mixolydian", "harmonicMinor" };
        return names;
    }

    juce::String scaleTypeToName (berlin::ScaleType type)
    {
        return scaleNames()[static_cast<int> (type)];
    }

    bool scaleTypeFromName (const juce::String& name, berlin::ScaleType& out)
    {
        const int index = scaleNames().indexOf (name);
        if (index < 0)
            return false;
        out = static_cast<berlin::ScaleType> (index);
        return true;
    }

    // tempo-delay-ui schema v3 (design.md D7): mirrors scaleNames()'s "name
    // string, not a raw ordinal" convention. Order MUST match
    // berlin::SyncDivision's declaration order (TempoSync.h's own doc
    // comment: "NEVER reorder without a schema bump").
    const juce::StringArray& divisionNames()
    {
        static const juce::StringArray names { "half", "quarter", "dottedEighth", "eighth", "eighthTriplet", "sixteenth" };
        return names;
    }

    juce::String divisionToName (berlin::SyncDivision division)
    {
        return divisionNames()[static_cast<int> (division)];
    }

    bool divisionFromName (const juce::String& name, berlin::SyncDivision& out)
    {
        const int index = divisionNames().indexOf (name);
        if (index < 0)
            return false;
        out = static_cast<berlin::SyncDivision> (index);
        return true;
    }

    // Booleans are written as an explicit "true"/"false" text token (Decision
    // 1's "explicitly formatted juce::String" rule) - never a native
    // juce::var bool, and never silently accepted from any other text.
    juce::String boolToName (bool value) { return value ? "true" : "false"; }

    bool boolFromName (const juce::String& name, bool& out)
    {
        if (name == "true")  { out = true;  return true; }
        if (name == "false") { out = false; return true; }
        return false;
    }

    juce::String waveformToName (berlin::Waveform waveform)
    {
        return waveformNames()[static_cast<int> (waveform)];
    }

    bool waveformFromName (const juce::String& name, berlin::Waveform& out)
    {
        const int index = waveformNames().indexOf (name);
        if (index < 0)
            return false;
        out = static_cast<berlin::Waveform> (index);
        return true;
    }

    juce::String lfoDestinationToName (berlin::LfoDestination destination)
    {
        return lfoDestinationNames()[static_cast<int> (destination)];
    }

    bool lfoDestinationFromName (const juce::String& name, berlin::LfoDestination& out)
    {
        const int index = lfoDestinationNames().indexOf (name);
        if (index < 0)
            return false;
        out = static_cast<berlin::LfoDestination> (index);
        return true;
    }

    // Missing/garbage schemaVersion must be parseFailed, never a silent 0 -
    // String::getIntValue() would parse "abc" as 0, which is NOT the same as
    // "genuinely absent" - this predicate closes that gap.
    bool isUnsignedInteger (const juce::String& text)
    {
        return text.isNotEmpty() && text.containsOnly ("0123456789");
    }
}

namespace berlin
{

PresetManager::PresetManager (juce::File presetDirectoryToUse)
    : presetDirectory (std::move (presetDirectoryToUse))
{
}

juce::File PresetManager::defaultPresetDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("Berlin")
        .getChildFile ("Presets");
}

juce::ValueTree PresetManager::toValueTree (const Preset& preset)
{
    juce::ValueTree root (kRootType);
    root.setProperty ("schemaVersion", juce::String (kSchemaVersion), nullptr);
    root.setProperty ("name", preset.name, nullptr);

    juce::ValueTree synthNode (kSynthType);
    synthNode.setProperty ("waveform",       waveformToName (preset.patch.waveform), nullptr);
    synthNode.setProperty ("cutoffHz",       juce::String (preset.patch.cutoffHz, 9), nullptr);
    synthNode.setProperty ("resonance",      juce::String (preset.patch.resonance, 9), nullptr);
    synthNode.setProperty ("pulseWidth",     juce::String (preset.patch.pulseWidth, 9), nullptr);
    synthNode.setProperty ("attack",         juce::String (preset.patch.attack, 9), nullptr);
    synthNode.setProperty ("decay",          juce::String (preset.patch.decay, 9), nullptr);
    synthNode.setProperty ("sustain",        juce::String (preset.patch.sustain, 9), nullptr);
    synthNode.setProperty ("release",        juce::String (preset.patch.release, 9), nullptr);
    synthNode.setProperty ("lfoRateHz",      juce::String (preset.patch.lfoRateHz, 9), nullptr);
    synthNode.setProperty ("lfoDepth",       juce::String (preset.patch.lfoDepth, 9), nullptr);
    synthNode.setProperty ("lfoDestination", lfoDestinationToName (preset.patch.lfoDestination), nullptr);

    // tempo-delay-ui schema v3 (design.md D7): the 8 effects fields join the
    // 11 live parameters above - none of them are pinned to kDefaultPatch
    // anymore (preset-persistence's "Preset Scope Is Exactly The 11 Live
    // Parameters Plus Seed" requirement, widened).
    synthNode.setProperty ("delayTimeSeconds", juce::String (preset.patch.delayTimeSeconds, 9), nullptr);
    synthNode.setProperty ("delayFeedback",    juce::String (preset.patch.delayFeedback, 9), nullptr);
    synthNode.setProperty ("delayMix",         juce::String (preset.patch.delayMix, 9), nullptr);
    synthNode.setProperty ("reverbRoomSize",   juce::String (preset.patch.reverbRoomSize, 9), nullptr);
    synthNode.setProperty ("reverbDamping",    juce::String (preset.patch.reverbDamping, 9), nullptr);
    synthNode.setProperty ("reverbWetLevel",   juce::String (preset.patch.reverbWetLevel, 9), nullptr);
    synthNode.setProperty ("reverbDryLevel",   juce::String (preset.patch.reverbDryLevel, 9), nullptr);
    synthNode.setProperty ("outputLevel",      juce::String (preset.patch.outputLevel, 9), nullptr);
    synthNode.setProperty ("delaySynced",      boolToName (preset.patch.delaySynced), nullptr);
    synthNode.setProperty ("delayDivision",    divisionToName (preset.patch.delayDivision), nullptr);
    root.appendChild (synthNode, nullptr);

    juce::ValueTree generationNode (kGenerationType);
    generationNode.setProperty ("seed", juce::String (preset.seed), nullptr);
    generationNode.setProperty ("scaleType",      scaleTypeToName (preset.scaleType), nullptr);
    generationNode.setProperty ("rootPitchClass", juce::String (preset.rootPitchClass), nullptr);
    generationNode.setProperty ("rangeLow",       juce::String (preset.rangeLow), nullptr);
    generationNode.setProperty ("rangeHigh",      juce::String (preset.rangeHigh), nullptr);
    root.appendChild (generationNode, nullptr);

    // tempo-delay-ui schema v3 (design.md D7): BPM is a Preset-level field
    // (Transport/BerlinAudioProcessor's, not SynthPatch's), so it gets its
    // own sibling node rather than joining <Synth> or <Generation>.
    juce::ValueTree transportNode (kTransportType);
    transportNode.setProperty ("bpm", juce::String (preset.bpm, 9), nullptr);
    root.appendChild (transportNode, nullptr);

    return root;
}

PresetResult PresetManager::fromValueTree (const juce::ValueTree& tree, Preset& out)
{
    if (! tree.hasType (kRootType))
        return PresetResult::parseFailed;

    if (! tree.hasProperty ("schemaVersion"))
        return PresetResult::parseFailed;

    const juce::String versionText = tree.getProperty ("schemaVersion").toString();
    if (! isUnsignedInteger (versionText))
        return PresetResult::parseFailed;

    const int version = versionText.getIntValue();
    if (version > kSchemaVersion)
        return PresetResult::unsupportedVersion;
    // version < kSchemaVersion: accept + migrate forward, absent fields from
    // kDefaultPatch (Decision 3) - unreachable at kSchemaVersion == 1, policy only.

    const auto synthNode      = tree.getChildWithName (kSynthType);
    const auto generationNode = tree.getChildWithName (kGenerationType);
    const auto transportNode  = tree.getChildWithName (kTransportType);   // schema v3 only

    if (! synthNode.isValid() || ! generationNode.isValid())
        return PresetResult::parseFailed;

    static const char* const requiredSynthAttributes[] =
    {
        "waveform", "cutoffHz", "resonance", "pulseWidth", "attack", "decay",
        "sustain", "release", "lfoRateHz", "lfoDepth", "lfoDestination"
    };

    for (auto* attribute : requiredSynthAttributes)
        if (! synthNode.hasProperty (attribute))
            return PresetResult::parseFailed;

    if (! generationNode.hasProperty ("seed"))
        return PresetResult::parseFailed;

    // tempo-delay-ui schema v3 (design.md D7): BPM + sync division/mode +
    // all 8 effects fields are REQUIRED for version >= 3 (missing -> parseFailed,
    // mirrors scale-aware-generation's v1->v2 all-or-nothing group check
    // above); a v2-or-earlier file is missing them BY DEFINITION and defaults
    // to kDefaultBpm/Free/quarter/kDefaultPatch's own effects values
    // (preset-persistence's "Old-Format Presets Default Missing BPM, Sync
    // Division, Sync Mode, and Effects Fields" requirement).
    double       bpm            = kDefaultBpm;
    bool         delaySynced    = false;
    SyncDivision delayDivision  = SyncDivision::quarter;   // unused while Free
    float        delayTimeSeconds = kDefaultPatch.delayTimeSeconds;
    float        delayFeedback    = kDefaultPatch.delayFeedback;
    float        delayMix         = kDefaultPatch.delayMix;
    float        reverbRoomSize   = kDefaultPatch.reverbRoomSize;
    float        reverbDamping    = kDefaultPatch.reverbDamping;
    float        reverbWetLevel   = kDefaultPatch.reverbWetLevel;
    float        reverbDryLevel   = kDefaultPatch.reverbDryLevel;
    float        outputLevel      = kDefaultPatch.outputLevel;

    if (version >= 3)
    {
        static const char* const requiredV3SynthAttributes[] =
        {
            "delayTimeSeconds", "delayFeedback", "delayMix", "reverbRoomSize", "reverbDamping",
            "reverbWetLevel", "reverbDryLevel", "outputLevel", "delaySynced", "delayDivision"
        };

        for (auto* attribute : requiredV3SynthAttributes)
            if (! synthNode.hasProperty (attribute))
                return PresetResult::parseFailed;

        if (! transportNode.isValid() || ! transportNode.hasProperty ("bpm"))
            return PresetResult::parseFailed;

        if (! divisionFromName (synthNode.getProperty ("delayDivision").toString(), delayDivision))
            return PresetResult::parseFailed;

        if (! boolFromName (synthNode.getProperty ("delaySynced").toString(), delaySynced))
            return PresetResult::parseFailed;

        // bpm stays double-precision throughout (matches Transport/
        // BerlinAudioProcessor's own double bpm) - clampParameter is float-only,
        // so this mirrors its NaN-then-clamp contract by hand rather than
        // narrowing through float and losing precision.
        const double rawBpm = transportNode.getProperty ("bpm").toString().getDoubleValue();
        bpm = (rawBpm == rawBpm) ? std::clamp (rawBpm, kMinBpm, kMaxBpm) : kMinBpm;

        delayTimeSeconds = clampParameter (synthNode.getProperty ("delayTimeSeconds").toString().getFloatValue(),
                                            kMinDelayTimeSeconds, kMaxDelaySeconds);
        delayFeedback    = clampParameter (synthNode.getProperty ("delayFeedback").toString().getFloatValue(),
                                            kMinDelayFeedback, kMaxDelayFeedback);
        delayMix         = clampParameter (synthNode.getProperty ("delayMix").toString().getFloatValue(),
                                            kMinDelayMix, kMaxDelayMix);
        reverbRoomSize   = clampParameter (synthNode.getProperty ("reverbRoomSize").toString().getFloatValue(),
                                            kMinReverbRoomSize, kMaxReverbRoomSize);
        reverbDamping    = clampParameter (synthNode.getProperty ("reverbDamping").toString().getFloatValue(),
                                            kMinReverbDamping, kMaxReverbDamping);
        reverbWetLevel   = clampParameter (synthNode.getProperty ("reverbWetLevel").toString().getFloatValue(),
                                            kMinReverbWetLevel, kMaxReverbWetLevel);
        reverbDryLevel   = clampParameter (synthNode.getProperty ("reverbDryLevel").toString().getFloatValue(),
                                            kMinReverbDryLevel, kMaxReverbDryLevel);
        outputLevel      = clampParameter (synthNode.getProperty ("outputLevel").toString().getFloatValue(),
                                            kMinOutputLevel, kMaxOutputLevel);
    }

    Waveform waveform {};
    if (! waveformFromName (synthNode.getProperty ("waveform").toString(), waveform))
        return PresetResult::parseFailed;

    LfoDestination lfoDestination {};
    if (! lfoDestinationFromName (synthNode.getProperty ("lfoDestination").toString(), lfoDestination))
        return PresetResult::parseFailed;

    // scale-aware-generation schema v2: scaleType/rootPitchClass/rangeLow/
    // rangeHigh are REQUIRED for version >= 2 (missing -> parseFailed); a v1
    // file is missing them BY DEFINITION and defaults to minor/C/36-72
    // (PresetManager.h's Schema decision / design.md's migration policy).
    ScaleType scaleType     = ScaleType::minor;
    int       rootPitchClass = 0;
    int       rangeLow       = 36;
    int       rangeHigh      = 72;

    if (version >= 2)
    {
        static const char* const requiredGenerationAttributes[] =
        {
            "scaleType", "rootPitchClass", "rangeLow", "rangeHigh"
        };

        for (auto* attribute : requiredGenerationAttributes)
            if (! generationNode.hasProperty (attribute))
                return PresetResult::parseFailed;

        if (! scaleTypeFromName (generationNode.getProperty ("scaleType").toString(), scaleType))
            return PresetResult::parseFailed;

        rootPitchClass = generationNode.getProperty ("rootPitchClass").toString().getIntValue();
        rangeLow       = generationNode.getProperty ("rangeLow").toString().getIntValue();
        rangeHigh      = generationNode.getProperty ("rangeHigh").toString().getIntValue();

        // Bounded-review correction (finding R4-001, CRITICAL): these 3 fields
        // came from untrusted preset XML with no bounds check, unlike scaleType
        // (enum-validated above) and every SynthPatch field (clampParameter
        // below). An out-of-[0,11] rootPitchClass desyncs the editor's Root
        // combo box (ComboBox::setSelectedId deselects on an unmatched ID),
        // silently corrupting the root on the next Generate/Randomize/Lock-Seed
        // click. Normalize here so the STORED value is always sane, matching
        // normalizePitchRange's own "covers preset-loaded params" contract.
        rootPitchClass = ((rootPitchClass % 12) + 12) % 12;
        normalizePitchRange (rangeLow, rangeHigh);
    }

    Preset result;
    result.name                 = tree.getProperty ("name").toString();
    result.scaleType            = scaleType;
    result.rootPitchClass       = rootPitchClass;
    result.rangeLow             = rangeLow;
    result.rangeHigh            = rangeHigh;
    result.patch.waveform       = waveform;
    result.patch.lfoDestination = lfoDestination;
    result.patch.cutoffHz   = clampParameter (synthNode.getProperty ("cutoffHz").toString().getFloatValue(),   kMinCutoffHz,       kMaxCutoffHz);
    result.patch.resonance  = clampParameter (synthNode.getProperty ("resonance").toString().getFloatValue(),  kMinResonance,      kMaxResonance);
    result.patch.pulseWidth = clampParameter (synthNode.getProperty ("pulseWidth").toString().getFloatValue(), kMinPulseWidth,     kMaxPulseWidth);
    result.patch.attack     = clampParameter (synthNode.getProperty ("attack").toString().getFloatValue(),     kMinAttackSeconds,  kMaxAttackSeconds);
    result.patch.decay      = clampParameter (synthNode.getProperty ("decay").toString().getFloatValue(),      kMinDecaySeconds,   kMaxDecaySeconds);
    result.patch.sustain    = clampParameter (synthNode.getProperty ("sustain").toString().getFloatValue(),    kMinSustain,        kMaxSustain);
    result.patch.release    = clampParameter (synthNode.getProperty ("release").toString().getFloatValue(),    kMinReleaseSeconds, kMaxReleaseSeconds);
    result.patch.lfoRateHz  = clampParameter (synthNode.getProperty ("lfoRateHz").toString().getFloatValue(),  kMinLfoRateHz,      kMaxLfoRateHz);
    result.patch.lfoDepth   = clampParameter (synthNode.getProperty ("lfoDepth").toString().getFloatValue(),   kMinLfoDepth,       kMaxLfoDepth);
    result.seed = generationNode.getProperty ("seed").toString().getLargeIntValue();

    // tempo-delay-ui schema v3 (design.md D7).
    result.bpm                    = bpm;
    result.patch.delaySynced      = delaySynced;
    result.patch.delayDivision    = delayDivision;
    result.patch.delayTimeSeconds = delayTimeSeconds;
    result.patch.delayFeedback    = delayFeedback;
    result.patch.delayMix         = delayMix;
    result.patch.reverbRoomSize   = reverbRoomSize;
    result.patch.reverbDamping    = reverbDamping;
    result.patch.reverbWetLevel   = reverbWetLevel;
    result.patch.reverbDryLevel   = reverbDryLevel;
    result.patch.outputLevel      = outputLevel;

    out = result;
    return PresetResult::ok;
}

// ---- File I/O (Phase 2 / preset-persistence Decision 2) -------------------

juce::File PresetManager::fileForName (const juce::String& name) const
{
    const juce::String sanitized = juce::File::createLegalFileName (name.trim());

    if (sanitized.isEmpty())
        return {};

    const juce::File candidate = presetDirectory.getChildFile (sanitized + ".xml");

    // Threat Matrix's path-construction row: createLegalFileName already
    // strips path separators, but this is the defense-in-depth assertion -
    // never trust sanitization alone to guarantee containment.
    if (candidate.getParentDirectory() != presetDirectory)
        return {};

    return candidate;
}

juce::StringArray PresetManager::listPresetNames() const
{
    juce::StringArray names;

    for (const auto& file : presetDirectory.findChildFiles (juce::File::findFiles, false, "*.xml"))
    {
        Preset preset;
        if (load (file.getFileNameWithoutExtension(), preset) == PresetResult::ok)
            names.add (preset.name);
    }

    names.sort (true);
    return names;
}

PresetResult PresetManager::save (const Preset& preset) const
{
    if (! presetDirectory.exists() && presetDirectory.createDirectory().failed())
        return PresetResult::directoryUnavailable;

    const juce::File destination = fileForName (preset.name);
    if (destination == juce::File())
        return PresetResult::nameInvalid;

    const juce::String xmlText = toValueTree (preset).toXmlString();

    juce::TemporaryFile temp (destination);
    bool writeOk = false;

    {
        juce::FileOutputStream stream (temp.getFile());

        if (stream.openedOk())
        {
            writeOk = stream.writeText (xmlText, false, false, nullptr);
            stream.flush();
            writeOk = writeOk && ! stream.getStatus().failed();
        }
    }   // stream closed here - required before overwriteTargetFileWithTemporary()

    if (! writeOk)
        return PresetResult::writeFailed;

    return temp.overwriteTargetFileWithTemporary() ? PresetResult::ok : PresetResult::writeFailed;
}

PresetResult PresetManager::load (const juce::String& name, Preset& out) const
{
    const juce::File source = fileForName (name);
    if (source == juce::File() || ! source.existsAsFile())
        return PresetResult::fileNotFound;

    const juce::ValueTree tree = juce::ValueTree::fromXml (source.loadFileAsString());
    return fromValueTree (tree, out);
}

} // namespace berlin
