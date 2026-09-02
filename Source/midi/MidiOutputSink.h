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
    bool openFirstAvailableDevice();   // false = none available / open failed (valid degraded state)
    void closeDevice() noexcept;       // Decision 4 sequence; idempotent
    bool isDeviceOpen() const noexcept;
    juce::String getOpenDeviceName() const;   // manual gate / Phase 7 readout; NEVER from the callback

    // ---- prepareToPlay (audio thread, per JUCE) ----
    void prepare (double sampleRateToUse) noexcept;

    // ---- AUDIO THREAD ----
    void dispatch (const juce::MidiBuffer& buffer, int numSamples) noexcept;

    // ---- releaseResources / shutdown; NOT the audio callback ----
    void sendImmediately (const juce::MidiBuffer& buffer) noexcept;

private:
    std::unique_ptr<juce::MidiOutput> device;
    const int channel;
    double    sampleRate     { 0.0 };
    double    startMs        { 0.0 };
    long long elapsedSamples { 0 };
};

} // namespace berlin
