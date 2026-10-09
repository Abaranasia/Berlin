# Delta for Unit Test Harness

## MODIFIED Requirements

### Requirement: Console Test Runner Project

The harness-eligible tiers MUST additionally include the gui-free `Source/ui/` helpers (asset lookup, MIME, path-traversal rejection, user-data-folder path, native-args adapter), using only `juce_core`, and `Source/bridge/*` (`UiBridge`), registered by relative path with no source duplication. `WebEditor.{h,cpp}` and `Source/ui/generated/EmbeddedAssets.*` MUST NOT be registered; they stay manual-gate-only. The module list requirements are unchanged.

(Previously: `Source/ui/` helpers were not registered. `Tests/BerlinTests.jucer` already registers `../Source/bridge/UiBridge.{h,cpp}`; this delta only corrects the requirement text to list `Source/bridge/*`, and no `.jucer` change is needed for the bridge. Only the new gui-free `Source/ui/` helpers are newly registered.)

#### Scenario: Bridge and helpers registered
- GIVEN `Tests/BerlinTests.jucer`
- WHEN its file list is inspected
- THEN `Source/bridge/*` (already present) and the new gui-free `Source/ui/` helpers are registered by relative path

#### Scenario: GUI and generated files excluded
- GIVEN `WebEditor.{h,cpp}` and `EmbeddedAssets.{h,cpp}`
- WHEN the file list is inspected
- THEN none are present, and the project builds with no `juce_gui_*` module and no pnpm

## ADDED Requirements

### Requirement: Headless UI Helper Coverage

The harness MUST include tests, written before the helpers (Strict TDD), for asset lookup (`/` to `index.html`, unknown returns `nullopt`, empty table `{nullptr, 0}` returns `nullopt`, traversal rejected), MIME mapping, user-data-folder path, and the native-args adapter (valid dispatch, missing/invalid args produce an error result without crashing).

#### Scenario: Helper suites run headlessly
- GIVEN the test binary
- WHEN run with no filter
- THEN the UI helper suites execute and pass without a window or WebView2

#### Scenario: Adapter error paths covered
- GIVEN empty, non-string-cmd, and malformed-args inputs
- WHEN the adapter tests run
- THEN each yields an error result and no failure or crash
