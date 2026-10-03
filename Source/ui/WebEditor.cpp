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
      owner (processorToEdit),
      bridge (processorToEdit)
{
    setSize (kEditorWidth, kEditorHeight);

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
    stopTimer();
    browser.reset();
}

void WebEditor::resized()
{
    fallbackLabel.setBounds (getLocalBounds());

    if (browser != nullptr)
        browser->setBounds (getLocalBounds());
}

void WebEditor::timerCallback()
{
    const PlayheadState state { owner.getPlayheadStep(), owner.isPlaying() };

    // Commit only when the event is really sent, so a hidden-to-visible
    // transition re-emits (web-editor spec).
    if (browser == nullptr || ! browser->isVisible() || ! detector.hasChanged (state))
        return;

    auto* payload = new juce::DynamicObject();
    payload->setProperty ("step", state.step);
    payload->setProperty ("playing", state.playing);
    browser->emitEventIfBrowserIsVisible ("playhead", juce::var (payload));
    detector.commit (state);
}

} // namespace berlin

#endif // BERLIN_WEB_UI
