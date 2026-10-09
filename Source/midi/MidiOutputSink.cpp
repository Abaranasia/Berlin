/*
  ==============================================================================

   MidiOutputSink - out-of-line definitions (midi-output-routing spec,
   midi-output-dispatch + midi-device-selection).

  ==============================================================================
*/

#include "MidiOutputSink.h"

#include "midi/MidiChannelRemap.h"
#include "midi/MidiMessageBytes.h"

namespace berlin
{

MidiOutputSink::MidiOutputSink (int outputChannel) noexcept
    : channel (clampMidiChannel (outputChannel))
{
}

MidiOutputSink::~MidiOutputSink()
{
    closeDevice();
}

bool MidiOutputSink::openDevice (const juce::String& deviceId)
{
    closeDevice();   // the old device is fully closed (CC123 last) before any new one opens

    if (deviceId.isEmpty())
        return false;   // None

    device = juce::MidiOutput::openDevice (deviceId);

    if (device == nullptr)
        return false;

    device->startBackgroundThread();
    return true;
}

void MidiOutputSink::setChannel (int newChannel) noexcept
{
    const int clamped = clampMidiChannel (newChannel);

    if (device != nullptr && clamped != channel.load (std::memory_order_relaxed))
    {
        // Same discard-then-panic order as closeDevice(): a queued note-on must
        // not overtake the All Notes Off sent on the channel it was played on.
        device->clearAllPendingMessages();
        const MidiMessageBytes panic = makeAllNotesOff (channel.load (std::memory_order_relaxed));
        device->sendMessageNow (juce::MidiMessage (panic.status, panic.data1, panic.data2));
    }

    channel.store (clamped, std::memory_order_relaxed);
}

int MidiOutputSink::getChannel() const noexcept
{
    return channel.load (std::memory_order_relaxed);
}

void MidiOutputSink::closeDevice() noexcept
{
    if (device == nullptr)
        return;

    // Exact order (design.md Decision 4): discard the queue FIRST, so the
    // CC123 panic guard sent next is provably the last message on the wire.
    // Reversing these two lets a still-queued future note-on overtake the
    // guard and hang the exact note it exists to prevent.
    device->clearAllPendingMessages();
    const MidiMessageBytes panic = makeAllNotesOff (channel.load (std::memory_order_relaxed));
    device->sendMessageNow (juce::MidiMessage (panic.status, panic.data1, panic.data2));
    device->stopBackgroundThread();
    device.reset();
}

bool MidiOutputSink::isDeviceOpen() const noexcept
{
    return device != nullptr;
}

juce::String MidiOutputSink::getOpenDeviceName() const
{
    return device != nullptr ? device->getName() : juce::String();
}

void MidiOutputSink::prepare (double sampleRateToUse) noexcept
{
    sampleRate = sampleRateToUse;
    elapsedSamples = 0;
    startMs = juce::Time::getMillisecondCounterHiRes();
}

void MidiOutputSink::dispatch (juce::MidiBuffer& buffer, int numSamples) noexcept
{
    if (device != nullptr && sampleRate > 0.0 && ! buffer.isEmpty())
    {
        remapMidiChannel (buffer, channel.load (std::memory_order_relaxed));   // in place, no allocation

        device->sendBlockOfMessages (buffer,
                                     startMs + static_cast<double> (elapsedSamples) * 1000.0 / sampleRate,
                                     sampleRate);
    }

    elapsedSamples += numSamples;   // ALWAYS, even with no device
}

void MidiOutputSink::sendImmediately (juce::MidiBuffer& buffer) noexcept
{
    if (device == nullptr)
        return;

    remapMidiChannel (buffer, channel.load (std::memory_order_relaxed));

    for (const auto item : buffer)
        device->sendMessageNow (item.getMessage());
}

} // namespace berlin
