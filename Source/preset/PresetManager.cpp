/*
  ==============================================================================

   PresetManager - out-of-line definitions (preset-persistence spec).

  ==============================================================================
*/

#include "PresetManager.h"

#include "synth/SynthPatch.h"

namespace
{
    constexpr const char* kRootType       = "BerlinPreset";
    constexpr const char* kSynthType      = "Synth";
    constexpr const char* kGenerationType = "Generation";

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
    root.appendChild (synthNode, nullptr);

    juce::ValueTree generationNode (kGenerationType);
    generationNode.setProperty ("seed", juce::String (preset.seed), nullptr);
    root.appendChild (generationNode, nullptr);

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

    Waveform waveform {};
    if (! waveformFromName (synthNode.getProperty ("waveform").toString(), waveform))
        return PresetResult::parseFailed;

    LfoDestination lfoDestination {};
    if (! lfoDestinationFromName (synthNode.getProperty ("lfoDestination").toString(), lfoDestination))
        return PresetResult::parseFailed;

    Preset result;
    result.name                 = tree.getProperty ("name").toString();
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

    out = result;
    return PresetResult::ok;
}

// ---- File I/O (Phase 2 / preset-persistence Decision 2) -------------------
// Stubs for now - Phase 2's RED test (PresetManagerFileTests.cpp) must fail
// against these before Phase 2's GREEN task replaces them with the real
// implementation.

juce::File PresetManager::fileForName (const juce::String&) const
{
    return {};
}

juce::StringArray PresetManager::listPresetNames() const
{
    return {};
}

PresetResult PresetManager::save (const Preset&) const
{
    return PresetResult::directoryUnavailable;
}

PresetResult PresetManager::load (const juce::String&, Preset&) const
{
    return PresetResult::fileNotFound;
}

} // namespace berlin
