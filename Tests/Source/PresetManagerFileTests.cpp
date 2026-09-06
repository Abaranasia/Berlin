/*
  ==============================================================================

   PresetManager file I/O tests (preset-persistence spec, roadmap Phase 11 /
   preset-system, design.md Decision 2). RED first: PresetManager's
   fileForName/listPresetNames/save/load are currently intentional-fail STUBS
   (Phase 1's placeholder implementation), so this suite must fail until
   Phase 2's production implementation replaces them.

   Everything here runs against a File::getSpecialLocation(tempDirectory)-
   scoped temporary directory, deleted at the end of each test - never the
   real userApplicationDataDirectory location.

   Covers: fileForName never escapes the preset directory for hostile/
   degenerate names (Threat Matrix's path-construction row); save ->
   listPresetNames contains it -> load returns an equal Preset; saving under
   the same name twice overwrites the file in place (no duplicate/stale
   copy); an unparseable file placed directly in the directory is silently
   omitted from listPresetNames, not fatal; enumerating a missing/absent
   directory returns an empty list without error, and the directory is
   auto-created on first save.

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

#include "preset/Preset.h"
#include "preset/PresetManager.h"

namespace
{
    // RAII wrapper around a uniquely-named temp directory, deleted on scope exit.
    struct TempPresetDir
    {
        TempPresetDir()
            : dir (juce::File::getSpecialLocation (juce::File::tempDirectory)
                       .getNonexistentChildFile ("BerlinPresetManagerTests", "", false))
        {
        }

        ~TempPresetDir()
        {
            dir.deleteRecursively();
        }

        juce::File dir;
    };
}

class PresetManagerFileTests final : public juce::UnitTest
{
public:
    PresetManagerFileTests() : juce::UnitTest ("PresetManagerFile", "Berlin") {}

    void runTest() override
    {
        beginTest ("fileForName never escapes the preset directory, for any hostile or degenerate name");
        {
            TempPresetDir temp;
            temp.dir.createDirectory();
            berlin::PresetManager manager (temp.dir);

            const juce::StringArray hostileNames { "../../evil", "a/b", "..", "", "   ", "###" };

            for (const auto& hostileName : hostileNames)
            {
                const auto result = manager.fileForName (hostileName);
                expect (result == juce::File() || result.getParentDirectory() == temp.dir);
            }
        }

        beginTest ("save -> listPresetNames contains it -> load returns an equal Preset");
        {
            TempPresetDir temp;
            berlin::PresetManager manager (temp.dir);

            berlin::Preset in;
            in.name             = "My Preset";
            in.patch.cutoffHz   = 1234.5f;
            in.patch.resonance  = 3.25f;
            in.seed             = 42;

            expect (manager.save (in) == berlin::PresetResult::ok);

            const auto names = manager.listPresetNames();
            expect (names.contains (in.name));

            berlin::Preset out;
            expect (manager.load (in.name, out) == berlin::PresetResult::ok);
            expect (out.name == in.name);
            expect (out.patch.cutoffHz == in.patch.cutoffHz);
            expect (out.patch.resonance == in.patch.resonance);
            expect (out.seed == in.seed);
        }

        beginTest ("saving under the same name twice overwrites the file in place - no duplicate or stale copy");
        {
            TempPresetDir temp;
            berlin::PresetManager manager (temp.dir);

            berlin::Preset first;
            first.name = "Dup";
            first.seed = 1;

            berlin::Preset second;
            second.name = "Dup";
            second.seed = 2;

            expect (manager.save (first) == berlin::PresetResult::ok);
            expect (manager.save (second) == berlin::PresetResult::ok);

            const auto files = temp.dir.findChildFiles (juce::File::findFiles, false, "*.xml");
            expectEquals (files.size(), 1);

            berlin::Preset out;
            expect (manager.load ("Dup", out) == berlin::PresetResult::ok);
            expect (out.seed == second.seed);
        }

        beginTest ("an unparseable file in the directory is silently omitted from listPresetNames, not fatal");
        {
            TempPresetDir temp;
            berlin::PresetManager manager (temp.dir);

            berlin::Preset valid;
            valid.name = "Good";
            expect (manager.save (valid) == berlin::PresetResult::ok);

            temp.dir.getChildFile ("garbage.xml").replaceWithText ("not xml at all <<<");

            const auto names = manager.listPresetNames();
            expect (names.contains ("Good"));
            expectEquals (names.size(), 1);
        }

        beginTest ("enumerating a missing directory returns an empty list without error, and save auto-creates it");
        {
            TempPresetDir temp;
            const auto missingDir = temp.dir.getChildFile ("does-not-exist-yet");
            berlin::PresetManager manager (missingDir);

            expect (manager.listPresetNames().isEmpty());

            berlin::Preset preset;
            preset.name = "First";
            expect (manager.save (preset) == berlin::PresetResult::ok);

            expect (missingDir.isDirectory());
            expect (manager.listPresetNames().contains ("First"));
        }

        beginTest ("load of a name with no saved preset returns fileNotFound, out left untouched");
        {
            TempPresetDir temp;
            berlin::PresetManager manager (temp.dir);

            berlin::Preset out;
            out.name = "Sentinel";
            expect (manager.load ("Nonexistent", out) == berlin::PresetResult::fileNotFound);
            expect (out.name == "Sentinel");
        }
    }
};

static PresetManagerFileTests presetManagerFileTests;
