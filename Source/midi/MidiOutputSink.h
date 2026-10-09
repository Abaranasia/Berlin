/*
  ==============================================================================

   MidiOutputSink - owns the juce::MidiOutput device, its background thread,
   the elapsed-sample timestamp origin, and the CC 123 panic guard on close
   (midi-output-routing spec, midi-output-dispatch + midi-device-selection).

   JUCE-aware (juce_audio_devices): no automated test can reach this file
   (Tests/BerlinTests.jucer stays juce_core-only by design) - manual-gate-only,
   same accepted tradeoff as Phase 4's realtime-audio-wiring coverage gap in
   playback-transport-clock.

   Device lifecycle (open/close) is confined to the MESSAGE THREAD, always
   called with the audio callback provably stopped (open before
   setAudioChannels, close after shutdownAudio() - design.md Decision 6).
   dispatch()'s device == nullptr early-return is the ZERO-DEVICES / FAILED-
   OPEN degraded state, not a race guard - no lock/atomic protects `device`.
   The Settings dialog (ui-settings D4) reopens/switches the port only through
   openDevice(), which closes the old device first via the ordered sequence
   below, with the audio callback stopped by the caller.

   Output channel (ui-settings D5/D6): an atomic int, default 1, written on the
   message thread (setChannel) and read relaxed on the audio thread. Events are
   authored on channel 1 by the processor; dispatch()/sendImmediately() rewrite
   the channel-voice status bytes in place (MidiChannelRemap.h) right before the
   bytes go to the device. The CC123 close guard uses the CURRENT channel.

   closeDevice() runs, in this EXACT order (design.md Decision 4):
     (1) device->clearAllPendingMessages()
     (2) device->sendMessageNow (CC123 All Notes Off)
     (3) device->stopBackgroundThread()
     (4) device.reset()
   sendMessageNow() bypasses the background queue. If CC123 were sent BEFORE
   discarding pending messages, a still-queued future note-on could be sent
   AFTER it and overtake the panic guard on the wire - hanging exactly the
   note this guard exists to prevent. Discarding the queue first makes CC123
   provably last. This ordering is load-bearing; do not reverse steps 1/2.
   Verified against the pinned JUCE checkout at apply time:
   juce::MidiOutput::clearAllPendingMessages() DOES exist (juce_MidiDevices.h)
   - no fallback substitution was needed.

   prepare() anchors startMs ONCE per device start (design.md Decision 5,
   a deviation from the proposal's literal "message thread" wording - JUCE
   calls prepareToPlay on the audio thread, so this is one bounded QPC read
   per device start, not per block, and it is the only way the anchor and
   elapsedSamples share an origin).

  ==============================================================================
*/

#pragma once

#include <atomic>
#include <memory>

#include <juce_audio_devices/juce_audio_devices.h>

namespace berlin
{

class MidiOutputSink
{
public:
    explicit MidiOutputSink (int outputChannel) noexcept;
    ~MidiOutputSink();   // calls closeDevice()

    // ---- MESSAGE THREAD ONLY, audio callback provably stopped ----
    // Closes the open device first (ordered close sequence), then opens `deviceId`.
    // "" means None (nothing is opened). false = None or open failed (valid degraded state).
    bool openDevice (const juce::String& deviceId);
    // Output channel 1-16. If a device is open, discards queued messages and sends
    // CC123 on the OLD channel before switching, so no note hangs across the change.
    void setChannel (int newChannel) noexcept;
    int  getChannel() const noexcept;
    void closeDevice() noexcept;       // Decision 4 sequence; idempotent
    bool isDeviceOpen() const noexcept;
    juce::String getOpenDeviceName() const;   // manual gate / Phase 7 readout; NEVER from the callback

    // ---- prepareToPlay (audio thread, per JUCE) ----
    void prepare (double sampleRateToUse) noexcept;

    // ---- AUDIO THREAD ----
    // Non-const: remaps the channel of the buffer's events in place (no allocation).
    void dispatch (juce::MidiBuffer& buffer, int numSamples) noexcept;

    // ---- releaseResources / shutdown; NOT the audio callback ----
    void sendImmediately (juce::MidiBuffer& buffer) noexcept;

private:
    std::unique_ptr<juce::MidiOutput> device;
    std::atomic<int> channel;
    double    sampleRate     { 0.0 };
    double    startMs        { 0.0 };
    long long elapsedSamples { 0 };
};

} // namespace berlin
