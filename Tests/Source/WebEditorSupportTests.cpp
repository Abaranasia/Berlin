/*
  ==============================================================================

   WebEditorSupport tests (webview-poc Phase 5): native-args adapter, playhead
   change detector, dev-server origin check. Headless, no WebView2.

  ==============================================================================
*/

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>
#include <juce_core/juce_core.h>

#include "bridge/UiBridge.h"
#include "plugin/BerlinAudioProcessor.h"
#include "ui/WebEditorSupport.h"

namespace
{
    juce::var makeArgs (std::initializer_list<juce::var> items)
    {
        return juce::var (juce::Array<juce::var> (items));
    }

    juce::Array<juce::var> callArgs (std::initializer_list<juce::var> items)
    {
        juce::Array<juce::var> array;
        for (const auto& item : items)
            array.add (item);
        return array;
    }

    juce::var bpmArgs (double bpm)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("bpm", bpm);
        return juce::var (o);
    }
}

class WebEditorSupportTests final : public juce::UnitTest
{
public:
    WebEditorSupportTests() : juce::UnitTest ("WebEditorSupport", "Berlin") {}

    void runTest() override
    {
        using berlin::dispatchNativeCall;

        beginTest ("adapter dispatches a valid command and returns the bridge result");
        {
            berlin::BerlinAudioProcessor processor;
            berlin::UiBridge bridge (processor);

            const auto result = dispatchNativeCall (bridge, callArgs ({ "setBpm", bpmArgs (97.0) }));
            expect (static_cast<bool> (result["ok"]));
            expect (result.hasProperty ("snapshot"));
        }

        beginTest ("adapter rejects missing and empty command");
        {
            berlin::BerlinAudioProcessor processor;
            berlin::UiBridge bridge (processor);

            expect (dispatchNativeCall (bridge, juce::Array<juce::var>())["error"].toString() == "missing arg: command");
            expect (dispatchNativeCall (bridge, callArgs ({ "" }))["error"].toString() == "missing arg: command");
            expect (! static_cast<bool> (dispatchNativeCall (bridge, juce::Array<juce::var>())["ok"]));
        }

        beginTest ("adapter rejects a non-string command");
        {
            berlin::BerlinAudioProcessor processor;
            berlin::UiBridge bridge (processor);

            expect (dispatchNativeCall (bridge, callArgs ({ 42 }))["error"].toString() == "invalid type: command");
        }

        beginTest ("absent or void args are treated as an empty object");
        {
            berlin::BerlinAudioProcessor processor;
            berlin::UiBridge bridge (processor);

            // "mutate" ignores its args and succeeds, so ok:true proves the adapter
            // forwarded a valid (empty) args object instead of rejecting or crashing.
            const auto absent = dispatchNativeCall (bridge, callArgs ({ "mutate" }));

            // mutate reports "busy" while one is in flight, so use a fresh processor.
            berlin::BerlinAudioProcessor otherProcessor;
            berlin::UiBridge otherBridge (otherProcessor);
            const auto voided = dispatchNativeCall (otherBridge, callArgs ({ "mutate", juce::var() }));
            expect (static_cast<bool> (absent["ok"]), absent["error"].toString());
            expect (static_cast<bool> (voided["ok"]), voided["error"].toString());

            // A command that needs a field reports the BRIDGE's own missing-arg error,
            // not the adapter's "invalid type: args".
            const auto missingBpm = dispatchNativeCall (bridge, callArgs ({ "setBpm" }));
            expect (missingBpm["error"].toString() == "missing arg: bpm", missingBpm["error"].toString());
        }

        beginTest ("adapter rejects non-object args");
        {
            berlin::BerlinAudioProcessor processor;
            berlin::UiBridge bridge (processor);

            const auto asArray  = dispatchNativeCall (bridge, callArgs ({ "setBpm", makeArgs ({ 1, 2 }) }));
            const auto asNumber = dispatchNativeCall (bridge, callArgs ({ "setBpm", 7 }));
            expect (asArray["error"].toString() == "invalid type: args");
            expect (asNumber["error"].toString() == "invalid type: args");
        }

        beginTest ("playhead detector: changed until committed, then stable");
        {
            berlin::PlayheadChangeDetector detector;
            const berlin::PlayheadState a { 3, true };

            expect (detector.hasChanged (a));          // before the first commit
            detector.commit (a);
            expect (! detector.hasChanged (a));        // same state
            expect (detector.hasChanged ({ 4, true })); // step differs
            expect (detector.hasChanged ({ 3, false })); // playing differs
            expect (detector.hasChanged ({ 4, true })); // uncommitted stays changed
            detector.commit ({ 4, true });
            expect (! detector.hasChanged ({ 4, true }));
        }

        beginTest ("pending event gate: initially quiet, emits only when pending and visible, quiet after commit");
        {
            berlin::PendingEventGate gate;
            expect (! gate.shouldEmit (true));            // nothing pending yet

            gate.markPending();
            expect (! gate.shouldEmit (false));           // hidden keeps it pending
            expect (gate.shouldEmit (true));              // visible -> emit
            expect (gate.shouldEmit (true));              // no commit -> still true

            gate.commit();
            expect (! gate.shouldEmit (true));            // committed

            gate.markPending();
            gate.markPending();
            gate.markPending();
            expect (gate.shouldEmit (true));
            gate.commit();                                // one commit absorbs the burst
            expect (! gate.shouldEmit (true));
        }

        beginTest ("default export file is <documents>/Berlin/berlin-export.mid");
        {
            const auto docs = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("docs-root");
            const auto file = berlin::defaultExportFile (docs);

            expect (file == docs.getChildFile ("Berlin").getChildFile ("berlin-export.mid"));
            expect (file.getFileName() == "berlin-export.mid");
            expect (file.getParentDirectory().getFileName() == "Berlin");
        }

        beginTest ("export chooser result: cancelled for no file, path for a chosen file");
        {
            const auto cancelled = berlin::exportChooserResult (juce::File());
            expect (static_cast<bool> (cancelled["cancelled"]));
            expect (cancelled["path"].toString().isEmpty());
            expect (! cancelled.hasProperty ("error"));

            const auto chosenFile = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("out.mid");
            const auto chosen = berlin::exportChooserResult (chosenFile);
            expect (! static_cast<bool> (chosen["cancelled"]));
            expect (chosen["path"].toString() == chosenFile.getFullPathName());
            expect (chosen["path"].toString().isNotEmpty());
        }

        beginTest ("busy chooser result is cancelled with error busy");
        {
            const auto busy = berlin::busyChooserResult();
            expect (static_cast<bool> (busy["cancelled"]));
            expect (busy["path"].toString().isEmpty());
            expect (busy["error"].toString() == "busy");
        }

        beginTest ("dev server origin accepts only loopback http with a port");
        {
            using berlin::devServerOrigin;
            expect (devServerOrigin ("http://localhost:5173").has_value());
            expect (devServerOrigin ("http://127.0.0.1:5173").has_value());
            expect (! devServerOrigin ("").has_value());
            expect (! devServerOrigin ("https://localhost:5173").has_value());
            expect (! devServerOrigin ("http://example.com:5173").has_value());
            expect (! devServerOrigin ("http://localhost").has_value());
            expect (! devServerOrigin ("http://localhost:5173.evil.com").has_value());
        }

        beginTest ("dev server origin port must be 1-65535");
        {
            using berlin::devServerOrigin;
            expect (devServerOrigin ("http://localhost:1").has_value());
            expect (devServerOrigin ("http://localhost:65535").has_value());
            expect (! devServerOrigin ("http://localhost:0").has_value());
            expect (! devServerOrigin ("http://localhost:00000").has_value());
            expect (! devServerOrigin ("http://localhost:65536").has_value());
            expect (! devServerOrigin ("http://127.0.0.1:99999").has_value());
        }
    }
};

static WebEditorSupportTests webEditorSupportTests;
