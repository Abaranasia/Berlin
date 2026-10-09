# Embedded UI Assets Specification

## Purpose

Defines the generated embedded asset table, the gui-free lookup/MIME/path helpers, and the pre-build pipeline that produces them.

## Requirements

### Requirement: Asset Lookup

A gui-free helper (`juce_core` only) MUST resolve a request path against the embedded table. `/` MUST map to `index.html`. Unknown paths MUST return `std::nullopt`. Lookup MUST tolerate an empty table (`{nullptr, 0}`), which the unit tests use as a synthetic table; the shipped table is never empty.

#### Scenario: Root maps to index
- GIVEN a table containing `index.html`
- WHEN `/` is looked up
- THEN the `index.html` asset is returned

#### Scenario: Unknown path
- GIVEN any table
- WHEN `/missing.js` is looked up
- THEN `std::nullopt` is returned

#### Scenario: Empty table
- GIVEN a synthetic empty table `{nullptr, 0}`
- WHEN any path is looked up
- THEN `std::nullopt` is returned and nothing crashes

### Requirement: Path Traversal Rejected

Lookup MUST reject any path containing a `..` segment, any `\`, any `:`, or any `%` (which covers encoded equivalents), returning `std::nullopt`.

#### Scenario: Traversal attempt
- GIVEN a table containing `index.html`
- WHEN `/../secret`, `/assets/../../x`, `\x`, `/c:x`, or `/%2e%2e/x` is looked up
- THEN `std::nullopt` is returned

### Requirement: MIME By Extension

A helper MUST map file extension to MIME type (at least html, js, css, svg, json, png, woff2, ico) case-insensitively, with js mapped to `text/javascript`, falling back to `application/octet-stream`.

#### Scenario: Known extension
- GIVEN `app.JS`
- WHEN the MIME is requested
- THEN `text/javascript` is returned

#### Scenario: Unknown extension
- GIVEN `file.xyz`
- WHEN the MIME is requested
- THEN `application/octet-stream` is returned

### Requirement: User Data Folder Path Helper

A gui-free helper MUST build the WebView2 data path as `userApplicationDataDirectory/Berlin/WebView2`.

#### Scenario: Path built
- GIVEN a base directory
- WHEN the helper is called
- THEN the result ends with `Berlin/WebView2` under that base

### Requirement: Native Arguments Adapter

A gui-free adapter MUST convert native-function `var` arguments into a `UiBridge::dispatch` call. Missing, wrongly typed, or malformed arguments MUST return an error result and MUST NOT crash: no `cmd` argument or empty-string `cmd` → `missing arg: command`; non-string `cmd` → `invalid type: command`; `args` absent or void → treated as `{}` (no error); `args` present but not an object → `invalid type: args`.

#### Scenario: Valid args
- GIVEN a `cmd` string and an args object
- WHEN the adapter runs
- THEN `UiBridge::dispatch` is called and its result returned

#### Scenario: Invalid args
- GIVEN an empty argument array, an empty-string `cmd`, a non-string `cmd`, or a non-object `args`
- WHEN the adapter runs
- THEN an error result with `missing arg: command`, `missing arg: command`, `invalid type: command`, or `invalid type: args` respectively is returned

#### Scenario: Absent args treated as empty object
- GIVEN a valid `cmd` string and no `args`
- WHEN the adapter runs
- THEN `UiBridge::dispatch` is called with `{}` and no adapter error is returned

### Requirement: Generated Assets And Pre-Build Pipeline

`Source/ui/generated/EmbeddedAssets.{h,cpp}` MUST be generated and gitignored. Pre-build (all configurations of both `.jucer` files) MUST always run the full pipeline: `pnpm --dir ui install --frozen-lockfile`, `pnpm --dir ui run build`, then `node ui/scripts/embed-assets.mjs` (no `--jucer` argument). The pipeline MUST NOT read any flag, MUST NOT have a stub mode and MUST always produce the full asset table, whose generated header defines `BERLIN_EMBEDDED_ASSETS_FULL 1`. Any failure MUST exit non-zero with a clear message; a missing node MUST fail with the existing BRL001 "node not found on PATH" diagnostic from the pre-build command, and a missing pnpm with the existing BRL002 "pnpm not found on PATH" diagnostic from the embed script. The pipeline MAY skip install, build, and embed when the hash of its inputs (`ui/src/**`, `ui/index.html`, `ui/package.json`, `ui/pnpm-lock.yaml`, `ui/vite.config.ts`, `ui/tsconfig*.json`, `ui/scripts/*`) is unchanged and the generated files exist. The README MUST document node and pnpm as build prerequisites.

#### Scenario: Full pipeline [B]
- GIVEN pnpm and node are installed and no generated assets exist
- WHEN the project builds
- THEN install, build, and embed run in order and the table contains `index.html`

#### Scenario: Pipeline ignores any flag [V]
- GIVEN the embed library and script tests
- WHEN the embed entry point runs with any arguments or `.jucer` contents
- THEN it always produces the full table and exposes no `readWebUiFlag`, mode or `--jucer` handling

#### Scenario: Generated header guard [V]
- GIVEN a completed embed run
- WHEN the generated header is read
- THEN it contains `BERLIN_EMBEDDED_ASSETS_FULL 1`

#### Scenario: Unchanged inputs skipped [V]
- GIVEN a previous successful build, an unchanged input hash, and existing generated files
- WHEN the pre-build runs again
- THEN install, build, and embed are skipped and the build succeeds

#### Scenario: pnpm missing [B]
- GIVEN pnpm is absent
- WHEN the project builds
- THEN the build fails with non-zero exit and the BRL002 "pnpm not found on PATH" diagnostic

#### Scenario: node missing [B]
- GIVEN node is absent
- WHEN the pre-build command runs
- THEN the build fails with non-zero exit and the BRL001 "node not found on PATH" diagnostic

#### Scenario: Step failure aborts [B]
- GIVEN `pnpm run build` fails
- WHEN the pre-build runs
- THEN the embed step is not run and the build fails

#### Scenario: Empty build output fails [V]
- GIVEN `pnpm run build` succeeds but its output contains no `index.html`
- WHEN the pre-build runs
- THEN it fails with the BRL008 "ui build produced no index.html" diagnostic and writes neither the generated files nor the stamp

#### Scenario: Generated files untracked [B]
- GIVEN a completed build
- WHEN `git status` is run
- THEN `Source/ui/generated/EmbeddedAssets.*` do not appear as changes

#### Scenario: README documents prerequisites [M]
- GIVEN the top-level `README.md`
- WHEN read
- THEN a "Build prerequisites" section lists node and pnpm (and the WebView2 runtime and NuGet package)
