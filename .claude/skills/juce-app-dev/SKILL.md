---
name: juce-app-dev
description: "Trigger: JUCE, AudioAppComponent, getNextAudioBlock, prepareToPlay, audioDeviceIOCallback, DSP, audio callback. Apply real-time safety and audio/GUI thread rules for JUCE standalone application code."
license: Apache-2.0
metadata:
  author: "Abaranasia"
  version: "1.0"
---

## Activation Contract
Apply when writing or editing: `MainComponent` (`AudioAppComponent`), any DSP class, `getNextAudioBlock`/`audioDeviceIOCallback`, or code that shares data between the audio thread and the GUI thread.

## Hard Rules
- **`getNextAudioBlock`/`audioDeviceIOCallback` is real-time**: no heap allocation (`new`, `std::vector::push_back`, `String` construction), no `std::mutex`/`juce::CriticalSection` (not even `try_lock` — it can still block the audio thread), no blocking I/O, no exceptions, no logging. Pre-allocate all buffers in `prepareToPlay`.
- **Denormals**: call `juce::ScopedNoDenormals` at the top of every audio callback doing float DSP.
- **GUI never touches audio-thread state directly.** Share scalars via `std::atomic`; share block/generated data (levels, waveform, generated buffers) via a lock-free structure (`juce::AbstractFifo`, or `juce::SpinLock` only if a lock is truly unavoidable and held for a handful of instructions) — never a full mutex on the audio side.
- **Latency**: with no host to call `setLatencySamples()` on, if DSP introduces delay (lookahead, oversampling, FIR), expose it as an atomic the GUI/rest of the app can read, and account for it wherever the app measures or displays timing.
- **Device changes re-trigger `prepareToPlay`**: sample-rate/block-size/channel-count changes happen whenever the user changes audio device settings — resize buffers there, never assume they stay constant after construction.
- **Persisted app state** (settings, presets): serialize via one `juce::ValueTree`, not scattered member variables, with a schema version int — not `AudioProcessorValueTreeState` (no `AudioProcessor` exists here).

## Decision Gates
| Situation | Do this |
|---|---|
| DSP needs a buffer bigger than one block | Allocate/resize in `prepareToPlay`, never inside `getNextAudioBlock` |
| GUI needs data generated on the audio thread | `std::atomic` for scalars, `AbstractFifo`/ring buffer for block data |
| Tempted to reach for a mutex on the audio thread | Don't. Prefer lock-free; use `juce::SpinLock` only when a lock is unavoidable and trivially short |
| Adding persisted app settings/presets | One versioned `ValueTree`, not ad-hoc members |

## Execution Steps
1. Before editing `getNextAudioBlock`, scan the diff for any allocation, lock, or exception path introduced — remove it or move it to `prepareToPlay`.
2. Before adding cross-thread data sharing, check whether a plain `std::atomic` is enough before reaching for a FIFO.
3. Confirm `ScopedNoDenormals` is present in any new float DSP callback.

## Output Contract
Before reporting DSP/audio-callback work done, confirm: no allocation/lock/exception on the audio-thread path touched, cross-thread data uses atomics or lock-free structures, buffers are sized in `prepareToPlay`.

## References
- https://timur.audio/using-locks-in-real-time-audio-processing-safely — why `std::mutex` (even `try_lock`) is unsafe on the audio thread, `SpinLock` alternative
- https://docs.juce.com/master/classAudioAppComponent.html — `AudioAppComponent` callback contract
