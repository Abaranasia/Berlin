/*
  ==============================================================================

   WebAssets tests (webview-poc Phase 4, embedded-ui-assets spec). Headless:
   juce_core only, fixture tables injected (no generated assets needed).

  ==============================================================================
*/

#include <juce_core/juce_core.h>

#include "ui/WebAssets.h"

namespace
{
    const unsigned char kIndex[] = { 'i', 'x' };
    const unsigned char kJs[]    = { 'j', 's', '!' };

    const berlin::EmbeddedAsset kEntries[] = {
        { "index.html", kIndex, sizeof (kIndex) },
        { "assets/a.js", kJs, sizeof (kJs) },
    };

    const berlin::AssetTable kTable { kEntries, 2 };
}

class WebAssetsTests final : public juce::UnitTest
{
public:
    WebAssetsTests() : juce::UnitTest ("WebAssets", "Berlin") {}

    void runTest() override
    {
        beginTest ("root and index.html resolve to index.html");
        {
            for (const char* path : { "/", "/index.html" })
            {
                const auto asset = berlin::findAsset (kTable, path);
                expect (asset.has_value(), path);
                if (asset)
                {
                    expect (asset->data.size() == 2);
                    expect (asset->mimeType == "text/html");
                }
            }
        }

        beginTest ("nested asset resolves and unknown path misses");
        {
            const auto asset = berlin::findAsset (kTable, "/assets/a.js");
            expect (asset.has_value() && asset->data.size() == 3);
            expect (asset.has_value() && asset->mimeType == "text/javascript");
            expect (! berlin::findAsset (kTable, "/missing.js").has_value());
        }

        beginTest ("query and fragment are stripped");
        {
            expect (berlin::findAsset (kTable, "/index.html?x=1").has_value());
            expect (berlin::findAsset (kTable, "/assets/a.js#frag").has_value());
        }

        beginTest ("traversal, backslash, colon and percent are rejected");
        {
            for (const char* path : { "../x", "/../secret", "/a/../b", "/assets/../../x", "\\x", "/c:x", "%2e", "/%2e%2e/x" })
                expect (! berlin::findAsset (kTable, path).has_value(), path);
        }

        beginTest ("empty table never matches and does not crash");
        {
            const berlin::AssetTable empty { nullptr, 0 };
            expect (! berlin::findAsset (empty, "/").has_value());
            expect (! berlin::findAsset (empty, "/index.html").has_value());
        }

        beginTest ("MIME by extension, case-insensitive, with default");
        {
            expect (berlin::mimeTypeForPath ("a.html") == "text/html");
            expect (berlin::mimeTypeForPath ("app.JS") == "text/javascript");
            expect (berlin::mimeTypeForPath ("a.css") == "text/css");
            expect (berlin::mimeTypeForPath ("a.svg") == "image/svg+xml");
            expect (berlin::mimeTypeForPath ("a.json") == "application/json");
            expect (berlin::mimeTypeForPath ("a.png") == "image/png");
            expect (berlin::mimeTypeForPath ("a.woff2") == "font/woff2");
            expect (berlin::mimeTypeForPath ("a.ico") == "image/x-icon");
            expect (berlin::mimeTypeForPath ("file.xyz") == "application/octet-stream");
            expect (berlin::mimeTypeForPath ("noextension") == "application/octet-stream");
        }

        beginTest ("web view user data folder is base/Berlin/WebView2");
        {
            const auto base = juce::File::getSpecialLocation (juce::File::tempDirectory);
            const auto folder = berlin::webViewUserDataFolder (base);
            expect (folder == base.getChildFile ("Berlin").getChildFile ("WebView2"));
            expect (folder.getFullPathName().replaceCharacter ('\\', '/').endsWith ("Berlin/WebView2"));
        }
    }
};

static WebAssetsTests webAssetsTests;
