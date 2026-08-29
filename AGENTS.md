# Agent Skills — Berlin (JUCE Standalone App)

Project-scoped skills under `.claude/skills/`, applied automatically by trigger context.

| Skill | Applies to | Covers |
|---|---|---|
| [juce-app-dev](.claude/skills/juce-app-dev/SKILL.md) | `MainComponent`/`AudioAppComponent`, DSP classes, `getNextAudioBlock`, `audioDeviceIOCallback` | Real-time audio-thread safety, audio/GUI cross-thread data sharing, persisted app state |
| [juce-app-testing](.claude/skills/juce-app-testing/SKILL.md) | Test setup, DSP unit tests (once DSP code exists) | `juce::UnitTest` DSP tests: silent-input, feedback-stability, CPU-budget |
| [troubleshooting-docs](.claude/skills/troubleshooting-docs/SKILL.md) | Right after a non-trivial JUCE/DSP/build bug is confirmed fixed | Captures the fix as a categorized, schema-validated doc under `troubleshooting/` so it's searchable next session |

Adapted from the sibling JUCE plugin project [Millenia](../Millenia/AGENTS.md) — plugin-specific concerns (APVTS, pluginval, parameter-automation compatibility) were dropped since Berlin is a standalone application with no host/DAW.
