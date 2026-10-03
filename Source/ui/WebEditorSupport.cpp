#include "ui/WebEditorSupport.h"

namespace berlin
{

namespace
{
    constexpr int kMinTcpPort = 1;
    constexpr int kMaxTcpPort = 65535;
    constexpr int kMaxPortDigits = 5;

    juce::var errorResult (const juce::String& token)
    {
        auto* result = new juce::DynamicObject();
        result->setProperty ("ok", false);
        result->setProperty ("error", token);
        return juce::var (result);
    }
}

juce::var dispatchNativeCall (UiBridge& bridge, const juce::Array<juce::var>& args)
{
    if (args.isEmpty())
        return errorResult ("missing arg: command");

    if (! args[0].isString())
        return errorResult ("invalid type: command");

    const auto command = args[0].toString();
    if (command.isEmpty())
        return errorResult ("missing arg: command");

    juce::var commandArgs (new juce::DynamicObject());
    if (args.size() > 1 && ! args[1].isVoid())
    {
        if (! args[1].isObject() || args[1].isArray())
            return errorResult ("invalid type: args");
        commandArgs = args[1];
    }

    return bridge.dispatch (command, commandArgs);
}

bool PlayheadChangeDetector::hasChanged (PlayheadState state) const noexcept
{
    return ! hasCommitted || state.step != last.step || state.playing != last.playing;
}

void PlayheadChangeDetector::commit (PlayheadState state) noexcept
{
    last = state;
    hasCommitted = true;
}

void PendingEventGate::markPending() noexcept
{
    pending = true;
}

bool PendingEventGate::shouldEmit (bool visible) const noexcept
{
    return pending && visible;
}

void PendingEventGate::commit() noexcept
{
    pending = false;
}

juce::File defaultExportFile (const juce::File& documentsDir)
{
    return documentsDir.getChildFile ("Berlin").getChildFile ("berlin-export.mid");
}

juce::var exportChooserResult (const juce::File& result)
{
    auto* object = new juce::DynamicObject();
    object->setProperty ("cancelled", result == juce::File());
    object->setProperty ("path", result == juce::File() ? juce::String() : result.getFullPathName());
    return juce::var (object);
}

juce::var busyChooserResult()
{
    auto* object = new juce::DynamicObject();
    object->setProperty ("cancelled", true);
    object->setProperty ("path", juce::String());
    object->setProperty ("error", "busy");
    return juce::var (object);
}

std::optional<juce::String> devServerOrigin (const juce::String& env)
{
    for (const auto* prefix : { "http://localhost:", "http://127.0.0.1:" })
    {
        const juce::String p (prefix);
        if (! env.startsWith (p))
            continue;

        const auto port = env.substring (p.length());
        if (port.isEmpty() || ! port.containsOnly ("0123456789") || port.length() > kMaxPortDigits)
            continue;

        const auto value = port.getIntValue();
        if (value >= kMinTcpPort && value <= kMaxTcpPort)
            return env;
    }

    return std::nullopt;
}

} // namespace berlin
