# Delta for unit-test-harness

Scenario tags: [V] Vitest, [T] BerlinTests, [M] manual, [B] build.

## MODIFIED Requirements

### Requirement: Console Test Runner Project

The system MUST provide a sibling Projucer project `Tests/BerlinTests.jucer` with `projectType="consoleapp"` and the preprocessor definition `JUCE_UNIT_TESTS=1`, built independently from `Berlin.jucer`, sharing all harness-eligible source tiers — currently `Source/core/*`, `Source/generation/*`, `Source/playback/*`, `Source/midi/*` (excluding `MidiOutputSink.*`), the JUCE-free half of `Source/export/*`, the DSP math of `Source/synth/*`, and `Source/plugin/BerlinAudioProcessor.{h,cpp}` — and additionally the gui-free `Source/ui/` helpers (asset lookup, MIME, path-traversal rejection, user-data-folder path, native-args adapter), using only `juce_core`, and `Source/bridge/*` (`UiBridge`), all by relative-path file registration (no source duplication). The harness's module list MUST include `juce_dsp`, `juce_audio_formats`, `juce_audio_basics`, and `juce_audio_processors_headless` in addition to `juce_core`. The harness MUST stay headless: it MUST NOT register any editor, `WebEditor.{h,cpp}`, `EditorFactory.*` or `Source/ui/generated/EmbeddedAssets.*`, which stay manual-gate-only. A source tier or file that depends on any module beyond this set (e.g. `Source/midi/MidiOutputSink.*` or `WebEditor.*`, requiring `juce_audio_devices`/`juce_gui_basics`) MUST NOT be registered in this project.

(Previously: the list excluded `BerlinAudioProcessorEditor.{h,cpp}` ("GUI, manual-gate-only") and named it as an example of a file that MUST NOT be registered. That class no longer exists; `Tests/BerlinTests.jucer` needs no change.)

#### Scenario: Test project regenerates and builds [B]

- GIVEN `Tests/BerlinTests.jucer` with its registered source files
- WHEN `Projucer.exe --resave Tests/BerlinTests.jucer` is run and the generated project is built
- THEN the build succeeds and produces a console executable

#### Scenario: BerlinAudioProcessor is registered, no editor is [T]

- GIVEN `Source/plugin/BerlinAudioProcessor.{h,cpp}`
- WHEN `Tests/BerlinTests.jucer`'s file list is inspected
- THEN `BerlinAudioProcessor.{h,cpp}` is registered by relative path with `juce_audio_processors_headless` in the module list
- AND no editor, `WebEditor` or `EditorFactory` file is registered, and a headless processor's `createEditor()` still returns `nullptr`

#### Scenario: Bridge and helpers registered [T]

- GIVEN `Tests/BerlinTests.jucer`
- WHEN its file list is inspected
- THEN `Source/bridge/*` and the gui-free `Source/ui/` helpers are registered by relative path

#### Scenario: GUI and generated files excluded [B]

- GIVEN `WebEditor.{h,cpp}` and `EmbeddedAssets.{h,cpp}`
- WHEN the file list is inspected
- THEN none are present, and the project builds with no `juce_gui_*` module and no pnpm

## REMOVED Requirements

(No whole requirements are removed.)
