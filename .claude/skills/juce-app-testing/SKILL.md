---
name: juce-app-testing
description: "Trigger: JUCE unit test, DSP test, juce::UnitTest, audio processing test. Apply JUCE DSP unit testing workflow once the standalone app has DSP code to cover."
license: Apache-2.0
metadata:
  author: "Abaranasia"
  version: "1.0"
---

## Activation Contract
Apply once Berlin has DSP code worth covering: a class exposing `prepare`/`reset`/`process`, or `getNextAudioBlock` logic beyond a passthrough/clear. Not yet applicable to a bare `AudioAppComponent` scaffold.

## Hard Rules
- **Unit-test DSP logic, not JUCE itself.** Use `juce::UnitTest` (built into JUCE, no extra dependency) unless the project already depends on Catch2 — don't add a second test framework.
- **Test at multiple block sizes and sample rates** (e.g. 1, 7, 512, 4096 samples; 44100/48000/96000 Hz) — DSP tested at only one block size hides off-by-one and buffer-boundary bugs.
- **Test feedback/regeneration stability** on any DSP with internal state feeding back on itself: process a sustained signal for hundreds of blocks and assert peak amplitude stays bounded. This is the one failure mode a plain impulse-response test won't catch.
- **Test silent input** on any DSP component with internal state (delay lines, circular buffers): assert output stays near the noise floor. Catches uninitialized/uncleared buffers and denormal build-up.
- **Never assert real-time timing via wall-clock** in unit tests — flaky. Do assert a *relative* CPU budget (real-time factor = processing time / audio time) — a correctness property for a real-time system, not a timing flake — but only in a Release build; Debug builds run far slower and will false-positive it.

## Decision Gates
| Situation | Do this |
|---|---|
| New DSP algorithm added | Add a `juce::UnitTest` subclass exercising known input→output cases (impulse, silence, DC, Nyquist) |
| DSP has feedback/regeneration | Add the sustained-input stability test before wiring it into the app, not after |
| Preparing a build for real use | Run the full unit test suite; there is no pluginval-equivalent for a standalone app — manual multi-device/sample-rate/buffer-size testing (ASIO/WASAPI/CoreAudio) substitutes for host-compatibility validation |

## Execution Steps
1. Locate or create a test target running `juce::UnitTestRunner` (standalone console app or debug-build hook).
2. For DSP changes, write the failing test first, then implement.
3. Run any CPU-budget assertion in a Release build only.

## Output Contract
Before reporting testing work done, confirm: new DSP has unit tests, any regenerative/feedback path has a bounded-output stability test, any CPU-budget assertion ran against a Release build.

## References
- `references/dsp-stability-tests.md` — `juce::UnitTest` templates for the silent-input, feedback-stability, and CPU-budget checks called out above (generic placeholder class — adapt to Berlin's actual DSP classes once they exist)
