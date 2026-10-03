/*
  ==============================================================================

   PresetManager - owns all preset I/O and the ValueTree<->XML serialization
   core (preset-persistence spec, roadmap Phase 11 / preset-system, design.md
   Decisions 1-5).

   Serialization rule (binding, design.md Decision 1): every property is
   written as an explicitly formatted juce::String - floats via
   juce::String (v, 9), the seed via juce::String (juce::int64), enums via a
   name table. No native double/int64 juce::var is ever placed in the tree,
   which is what keeps a round trip through XML text bit-exact (a raw
   double/int64 var's own toString() silently loses precision / overflows -
   see design.md's Verified Findings table).

   Schema (Decision 3): root element "BerlinPreset" carries a schemaVersion
   attribute and a "name" attribute; child sections "Synth" (the 11 live
   parameters, plus - since schema v3, tempo-delay-ui - the 8 effects fields
   and delaySynced/delayDivision), "Generation" (the seed, plus - since
   schema v2, scale-aware-generation - scaleType/rootPitchClass/rangeLow/
   rangeHigh), and "Transport" (schema v3: bpm - a Preset-level field, not a
   SynthPatch one, hence its own sibling node rather than joining "Synth"). A
   version newer than kSchemaVersion is rejected (unsupportedVersion, unknown
   future semantics unguessable); a version older than kSchemaVersion is
   accepted with any field the older schema didn't have filled from a
   documented default - kDefaultPatch for Synth fields (the 8 effects fields'
   first real exercise of this migration path, tempo-delay-ui design.md), and
   minor/C/36-72 for a v1 file's absent scaleType/rootPitchClass/rangeLow/
   rangeHigh, and kDefaultBpm/Free/quarter for a pre-v3 file's absent
   bpm/delaySynced/delayDivision.

   Malformed input (Decision 4): structural defects (not the right root
   element, missing/unparseable schemaVersion, a missing required section or
   attribute, an unrecognized enum name) reject the WHOLE preset - `out` is
   left untouched. A continuous value outside its documented range is
   CLAMPED via the existing clampParameter, then the preset still loads.

   Storage (Decision 2): one file per preset in
   userApplicationDataDirectory/Berlin/Presets/*.xml. fileForName sanitises
   via createLegalFileName AND asserts the result is a DIRECT child of the
   preset directory (containment guard) - createLegalFileName alone does not
   prevent File::getChildFile from resolving ".." components.

  ==============================================================================
*/

#pragma once

#include <cstddef>
#include <map>
#include <mutex>
#include <optional>

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

#include "preset/Preset.h"

namespace berlin
{

class PresetManager
{
public:
    static constexpr int kSchemaVersion = 3;

    explicit PresetManager (juce::File presetDirectoryToUse = defaultPresetDirectory());

    static juce::File defaultPresetDirectory();   // <appData>/Berlin/Presets

    // Sanitised via createLegalFileName. Returns juce::File() if the name is
    // unusable OR if the result would not be a DIRECT child of the preset
    // directory (containment guard - Threat Matrix's path-construction row).
    juce::File fileForName (const juce::String& name) const;

    // `name` property of every parseable *.xml directly inside the preset
    // directory, sorted. Unparseable files are silently excluded, never fatal.
    // Per-file validity is cached by (size, mtime): an unchanged file is not
    // re-parsed, a changed/added file is, and a removed file's entry is
    // pruned (ui-bridge-parity D4). Safe to call from any thread.
    juce::StringArray listPresetNames() const;

    // Number of preset files parsed by listPresetNames() so far (test seam).
    std::size_t parseCount() const;

    // Unconditional - the caller has already confirmed any overwrite.
    // Creates the preset directory if needed.
    PresetResult save (const Preset& preset) const;

    // `out` is left untouched unless the result is PresetResult::ok.
    PresetResult load (const juce::String& name, Preset& out) const;

    // Pure, no I/O - the directly unit-testable core.
    static juce::ValueTree toValueTree (const Preset& preset);
    static PresetResult    fromValueTree (const juce::ValueTree& tree, Preset& out);

    // ui-engine-api design.md D8: shared enum<->name forwarders, so UiBridge
    // (and anything else) can encode/decode the SAME preset-XML name tables
    // this class already owns, without a second copy drifting out of sync.
    // Each delegates one line into the existing anonymous-namespace helper in
    // PresetManager.cpp - no table move, no behavior change.
    static juce::String waveformName (Waveform waveform);
    static bool         parseWaveform (const juce::String& name, Waveform& out);

    static juce::String lfoDestinationName (LfoDestination destination);
    static bool         parseLfoDestination (const juce::String& name, LfoDestination& out);

    static juce::String scaleTypeName (ScaleType type);
    static bool         parseScaleType (const juce::String& name, ScaleType& out);

    static juce::String divisionName (SyncDivision division);
    static bool         parseDivision (const juce::String& name, SyncDivision& out);

private:
    struct CacheEntry
    {
        juce::int64                 size = 0;
        juce::Time                  modified;
        std::optional<juce::String> name;   // nullopt: the file is not a valid preset
    };

    juce::File presetDirectory;

    mutable std::mutex                       cacheMutex;
    mutable std::map<juce::String, CacheEntry> cache;   // keyed by full path
    mutable std::size_t                      parses = 0;
};

} // namespace berlin
