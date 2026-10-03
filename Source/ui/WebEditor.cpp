#include "ui/WebEditor.h"

#if BERLIN_WEB_UI

#include "ui/WebAssets.h"
#include "ui/generated/EmbeddedAssets.h"

// A flag-on build must embed the real UI, never the empty stub (design D2).
static_assert (BERLIN_EMBEDDED_ASSETS_FULL == 1,
               "BERLIN_WEB_UI is on but the embedded assets are the stub: run the pre-build with pnpm available");

namespace berlin
{

namespace
{
    constexpr int kEditorWidth  = 800;
    constexpr int kEditorHeight = 680;
    constexpr int kPlayheadHz   = 30;
    constexpr int kConfirmOkResult = 1;   // OK/Cancel box: button[0] (OK) returns 1

    std::optional<juce::String> debugDevOrigin()
    {
       #if JUCE_DEBUG
        return devServerOrigin (juce::SystemStats::getEnvironmentVariable ("BERLIN_WEB_UI_DEV_URL", {}));
       #else
        return std::nullopt;
       #endif
    }
}

WebEditor::WebEditor (BerlinAudioProcessor& processorToEdit)
    : juce::AudioProcessorEditor (processorToEdit),
      bridge (processorToEdit)
{
    setSize (kEditorWidth, kEditorHeight);
    engine().addChangeListener (this);

    const auto dataFolder = webViewUserDataFolder (juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory));
    const auto devOrigin  = debugDevOrigin();

    using Options = juce::WebBrowserComponent::Options;

    auto options = Options()
        .withBackend (Options::Backend::webview2)
        .withWinWebView2Options (Options::WinWebView2().withUserDataFolder (dataFolder))
        .withNativeFunction ("dispatch",
                             [this] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion done)
                             {
                                 done (dispatchNativeCall (bridge, args));
                             })
        .withNativeFunction ("getSnapshot",
                             [this] (const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion done)
                             {
                                 done (bridge.snapshot());
                             })
        .withNativeFunction ("chooseExportFile",
                             [this] (const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion done)
                             {
                                 chooseExportFile (std::move (done));
                             })
        .withNativeFunction ("confirm",
                             [this] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion done)
                             {
                                 confirm (args, std::move (done));
                             })
        .withResourceProvider ([] (const juce::String& url) -> std::optional<juce::WebBrowserComponent::Resource>
                               {
                                   auto asset = findAsset (getEmbeddedAssets(), url);
                                   if (! asset)
                                       return std::nullopt;
                                   return juce::WebBrowserComponent::Resource { std::move (asset->data), std::move (asset->mimeType) };
                               },
                               devOrigin);

    if (! dataFolder.createDirectory().wasOk() || ! juce::WebBrowserComponent::areOptionsSupported (options))
    {
        fallbackLabel.setText ("The Microsoft WebView2 runtime is not available, so the web interface cannot be shown.",
                               juce::dontSendNotification);
        fallbackLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (fallbackLabel);
        resized();
        return;
    }

    browser = std::make_unique<juce::WebBrowserComponent> (options);
    addAndMakeVisible (*browser);
    // setSize() above ran resized() before the browser existed, and the host
    // keeps the same size, so lay it out now or it stays at 0x0.
    resized();
    browser->goToURL (devOrigin ? *devOrigin : juce::WebBrowserComponent::getResourceProviderRoot());
    startTimerHz (kPlayheadHz);
}

WebEditor::~WebEditor()
{
    // Order matters (design D1/D2): stop new events first, then flag teardown
    // so a dialog callback fired synchronously by a reset below is ignored
    // (a SafePointer is only cleared in ~Component, after this body).
    engine().removeChangeListener (this);
    shuttingDown = true;
    stopTimer();
    exportChooser.reset();
    confirmBox = {};
    browser.reset();
}

void WebEditor::resized()
{
    fallbackLabel.setBounds (getLocalBounds());

    if (browser != nullptr)
        browser->setBounds (getLocalBounds());
}

void WebEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    gate.markPending();   // emitted from timerCallback, one path for all events
}

void WebEditor::timerCallback()
{
    const bool visible = browser != nullptr && browser->isVisible();
    const PlayheadState state { engine().getPlayheadStep(), engine().isPlaying() };

    // Commit only when the event is really sent, so a hidden-to-visible
    // transition re-emits (web-editor spec).
    if (visible && detector.hasChanged (state))
    {
        auto* payload = new juce::DynamicObject();
        payload->setProperty ("step", state.step);
        payload->setProperty ("playing", state.playing);
        browser->emitEventIfBrowserIsVisible ("playhead", juce::var (payload));
        detector.commit (state);
    }

    if (gate.shouldEmit (visible))
    {
        browser->emitEventIfBrowserIsVisible ("snapshot", bridge.snapshot());
        gate.commit();
    }
}

void WebEditor::chooseExportFile (juce::WebBrowserComponent::NativeFunctionCompletion done)
{
    if (dialogOpen)
    {
        done (busyChooserResult());
        return;
    }

    dialogOpen = true;
    exportChooser = std::make_unique<juce::FileChooser> ("Export MIDI...",
                                                          defaultExportFile (juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)),
                                                          "*.mid");

    const int chooserFlags = juce::FileBrowserComponent::saveMode
                            | juce::FileBrowserComponent::canSelectFiles
                            | juce::FileBrowserComponent::warnAboutOverwriting;

    exportChooser->launchAsync (chooserFlags,
                                [safe = juce::Component::SafePointer<WebEditor> (this), done] (const juce::FileChooser& chooser)
                                {
                                    if (safe == nullptr || safe->shuttingDown)
                                        return;

                                    safe->dialogOpen = false;
                                    done (exportChooserResult (chooser.getResult()));
                                });
}

void WebEditor::confirm (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion done)
{
    if (dialogOpen)
    {
        done (false);
        return;
    }

    dialogOpen = true;
    confirmBox = juce::NativeMessageBox::showScopedAsync (
        juce::MessageBoxOptions::makeOptionsOkCancel (juce::MessageBoxIconType::QuestionIcon,
                                                      args[0].toString(),
                                                      args[1].toString(),
                                                      {}, {}, this),
        [safe = juce::Component::SafePointer<WebEditor> (this), done] (int result)
        {
            if (safe == nullptr || safe->shuttingDown)
                return;

            safe->dialogOpen = false;
            done (result == kConfirmOkResult);
        });
}

} // namespace berlin

#endif // BERLIN_WEB_UI
