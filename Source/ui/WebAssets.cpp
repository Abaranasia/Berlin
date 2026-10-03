#include "ui/WebAssets.h"

#include <cstring>

namespace berlin
{

std::optional<juce::String> normaliseAssetPath (const juce::String& requestPath)
{
    auto path = requestPath.upToFirstOccurrenceOf ("#", false, false).upToFirstOccurrenceOf ("?", false, false);

    if (path.containsAnyOf ("\\:%"))
        return std::nullopt;

    path = path.trimCharactersAtStart ("/");

    if (path.isEmpty())
        return juce::String ("index.html");

    juce::StringArray segments;
    segments.addTokens (path, "/", "");
    if (segments.contains (".."))
        return std::nullopt;

    return path;
}

std::optional<AssetResource> findAsset (AssetTable table, const juce::String& requestPath)
{
    if (table.entries == nullptr || table.count == 0)
        return std::nullopt;

    const auto path = normaliseAssetPath (requestPath);
    if (! path)
        return std::nullopt;

    for (size_t i = 0; i < table.count; ++i)
    {
        const auto& entry = table.entries[i];
        if (*path != juce::String::fromUTF8 (entry.path))
            continue;

        AssetResource resource;
        resource.data.resize (entry.size);
        if (entry.size > 0)
            std::memcpy (resource.data.data(), entry.data, entry.size);
        resource.mimeType = mimeTypeForPath (*path);
        return resource;
    }

    return std::nullopt;
}

juce::String mimeTypeForPath (const juce::String& path)
{
    const auto extension = path.fromLastOccurrenceOf (".", false, false).toLowerCase();

    if (! path.contains (".")) return "application/octet-stream";
    if (extension == "html")   return "text/html";
    if (extension == "js")     return "text/javascript";
    if (extension == "css")    return "text/css";
    if (extension == "svg")    return "image/svg+xml";
    if (extension == "json")   return "application/json";
    if (extension == "png")    return "image/png";
    if (extension == "woff2")  return "font/woff2";
    if (extension == "ico")    return "image/x-icon";
    return "application/octet-stream";
}

juce::File webViewUserDataFolder (const juce::File& appDataBase)
{
    return appDataBase.getChildFile ("Berlin").getChildFile ("WebView2");
}

} // namespace berlin
