---
project: Berlin
date: 2026-09-03
problem_type: audio_device_issue
component: audio_device_manager
symptoms:
  - "MIDI monitor attached to a loopMIDI virtual port shows zero activity while Berlin.exe runs, with no crash, assert, or error output"
  - "juce::MidiOutput::openDevice() succeeds (non-null) and startBackgroundThread() runs, yet the intended virtual port never receives any message"
  - "Disabling 'Microsoft GS Wavetable Synth' via Device Manager > Sound, video and game controllers > Disable does not remove it from juce::MidiOutput::getAvailableDevices()"
root_cause: logic_error
resolution_type: environment_setup
severity: moderate
related_components:
  - juce_audio_devices
tags:
  - midi-output
  - device-enumeration
  - windows
  - manual-verification
  - gs-wavetable-synth
---

## Problem

`Source/midi/MidiOutputSink::openFirstAvailableDevice()` opens
`juce::MidiOutput::getAvailableDevices()[0]` unconditionally (a locked product
decision from the `midi-output-routing` change — no device-selection UI this
phase, auto-open the first available device). During the change's mandatory
manual verification gate (loopMIDI + a MIDI monitor), the monitor showed
**no traffic at all**, with the app itself reporting no error.

## Symptoms

- MIDI monitor listening on the loopMIDI virtual port: completely silent.
- `openFirstAvailableDevice()` returns `true` (device opened successfully).
- No crash, no assert, no console/log error anywhere.
- Disabling `Microsoft GS Wavetable Synth` in Device Manager (Sound, video
  and game controllers, with "Show hidden devices" on) did **not** change
  which device got opened — it still won.

## What Didn't Work

- **Assuming the dispatch code was broken.** `MidiEventTranslator::translate`
  and `MidiOutputSink::dispatch` were re-read line by line against
  `design.md` — both matched the spec exactly (pre-reserved `MidiBuffer`,
  `sendBlockOfMessages` with elapsed-sample timestamping, no allocation/lock
  beyond the one documented exception). Nothing there was wrong.
- **Disabling the device in Device Manager.** `Microsoft GS Wavetable Synth`
  is not a true PnP device — it's Windows' built-in software MIDI synth,
  registered through the multimedia (winmm) subsystem rather than a
  hardware/driver stack. Disabling its Device Manager entry does not remove
  it from `midiOutGetNumDevs()`/`juce::MidiOutput::getAvailableDevices()`
  enumeration. It kept enumerating first regardless.

## Solution

The actual bug wasn't in the shipped code — `openFirstAvailableDevice()` was
opening the first device exactly as designed. The problem was **which**
device Windows puts first: `Microsoft GS Wavetable Synth` reliably beats any
virtual MIDI port (loopMIDI, LoopBe1, etc.) to index 0 on a stock Windows
install, because it's always present and enumerates early.

Two things closed the loop:

1. **Add visibility.** The return value of `openFirstAvailableDevice()` was
   deliberately ignored in production ("`false` is the valid silent state"),
   so there was no way to see which device actually got opened. A temporary,
   `#if JUCE_DEBUG`-gated diagnostic in `MainComponent`'s constructor fixed
   that:

   ```cpp
   // TEMPORARY diagnostic — not part of shipped code
   #if JUCE_DEBUG
   {
       juce::String log = "[MIDI diagnostic] devices available:\n";
       for (auto& d : juce::MidiOutput::getAvailableDevices())
           log << "  \"" << d.name << "\" (" << d.identifier << ")\n";
       log << "[MIDI diagnostic] opened device: \"" << midiSink.getOpenDeviceName() << "\"\n";
       DBG (log);
   }
   #endif
   ```

   Visible in Visual Studio's Debug Output window when run under the
   debugger (or via Sysinternals DebugView otherwise — `DBG` is
   `OutputDebugStringW` under the hood). This immediately confirmed
   `"Microsoft GS Wavetable Synth"` was the opened device, with `"loopMIDI
   Port"` present but second in the list.

2. **Force the test target for verification purposes only.** A second
   temporary, debug-only method let the manual gate actually run:

   ```cpp
   // TEMPORARY, debug-only — not part of the locked design
   bool MidiOutputSink::openDevicePreferring (const juce::String& nameContains)
   {
       const auto devices = juce::MidiOutput::getAvailableDevices();
       if (devices.isEmpty())
           return false;

       int chosen = 0;
       for (int i = 0; i < devices.size(); ++i)
           if (devices[i].name.containsIgnoreCase (nameContains)) { chosen = i; break; }

       device = juce::MidiOutput::openDevice (devices[chosen].identifier);
       if (device == nullptr)
           return false;

       device->startBackgroundThread();
       return true;
   }
   ```

   wired in behind `#if JUCE_DEBUG` / `#else` in `MainComponent`'s
   constructor so **Release builds kept the exact locked
   "auto-open first available device" behavior unchanged**:

   ```cpp
   #if JUCE_DEBUG
       const bool midiOpened = midiSink.openDevicePreferring ("loopMIDI");
   #else
       const bool midiOpened = midiSink.openFirstAvailableDevice();
   #endif
   ```

With loopMIDI forced open, the monitor lit up immediately, timing checked
out (~125 ms intervals, 120 BPM/16th notes), quit-mid-note showed the
tracked note-off followed by CC123 with nothing stuck, and a zero-device
relaunch ran cleanly. **Both temporary additions were then fully reverted**
— confirmed via `git diff --stat` showing `MainComponent.cpp` and
`MidiOutputSink.h/.cpp` byte-identical to the pre-diagnostic apply output,
both solutions rebuilt clean, full test suite re-run green (15 suites, 80
tests). No shipped/production code changed as a result of this diagnosis.

## Why This Works

`juce::MidiOutput::getAvailableDevices()` returns devices in the order the
underlying OS API (`midiOutGetDevCaps` on Windows) enumerates them, which is
driver/registration order, not anything JUCE or this app controls. Windows
ships `Microsoft GS Wavetable Synth` as a permanently-installed software
synth tied to the OS's built-in MIDI mapper rather than a PnP driver stack,
so it doesn't respond to Device Manager's Disable the way a real device
would, and it reliably claims one of the earliest enumeration slots. Any
"just open the first device" design will hit this on a stock Windows dev
box the moment a virtual MIDI port is also installed.

Confirming the dispatch chain was correct *before* chasing this device-order
theory mattered: the user was asked to simply listen to their speakers, and
the sequence was indeed playing — correct notes, correct timing — through
GS Wavetable Synth. That ruled out a translate/dispatch bug in one step and
pointed straight at device selection.

## Prevention

- When a design locks in "auto-open first available MIDI device" with no
  selection UI (as this project's Phase 5/`midi-output-routing` change did,
  deliberately, deferring a real picker to Phase 7), document this
  enumeration quirk as a known manual-testing gotcha in that change's
  tasks.md — don't let a future tester rediscover it from scratch.
- If a genuinely silent MIDI output is ever reported again, check
  `getOpenDeviceName()` (or equivalent temporary diagnostic) **before**
  suspecting the translate/dispatch code — device selection is the cheaper
  hypothesis to rule in/out first.
- Do not rely on Windows Device Manager's Disable to remove
  `Microsoft GS Wavetable Synth` from MIDI enumeration; it does not work.
  If a permanent workaround is ever needed, prefer selecting by name
  substring (as the temporary `openDevicePreferring` helper did) over
  disabling anything at the OS level.

## Related Issues

None yet — first entry in `troubleshooting/audio-device-issues/`.
