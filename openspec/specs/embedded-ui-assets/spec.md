# Embedded UI Assets Specification

## Purpose

Defines the generated embedded asset table, the gui-free lookup/MIME/path helpers, and the pre-build pipeline that produces them.

## Requirements

### Requirement: Asset Lookup

A gui-free helper (`juce_core` only) MUST resolve a request path against the embedded table. `/` MUST map to `index.html`. Unknown paths MUST return `std::nullopt`.

#### Scenario: Root maps to index
- GIVEN a table containing `index.html`
- WHEN `/` is looked up
- THEN the `index.html` asset is returned

#### Scenario: Unknown path
- GIVEN any table
- WHEN `/missing.js` is looked up
- THEN `std::nullopt` is returned

#### Scenario: Empty stub table
- GIVEN the stub (flag-off) empty table
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

`Source/ui/generated/EmbeddedAssets.{h,cpp}` MUST be generated and gitignored. Pre-build (all configurations of both `.jucer` files) MUST, with the flag off, run only `node ui/scripts/embed-assets.mjs` in stub mode, producing an empty table with no pnpm, network, or Vite. With the flag on, it MUST run `pnpm --dir ui install --frozen-lockfile`, `pnpm --dir ui run build`, then the embed script. Any failure MUST exit non-zero with a clear message, including "pnpm not found" and missing node. The script's flag state MUST stay in sync with the `BERLIN_WEB_UI` define. The pipeline MAY skip install, build, and embed when the hash of its inputs (mode, `ui/src/**`, `ui/index.html`, `ui/package.json`, `ui/pnpm-lock.yaml`, `ui/vite.config.ts`, `ui/tsconfig*.json`, `ui/scripts/*`) is unchanged and the generated files exist.

#### Scenario: Flag off, no pnpm
- GIVEN pnpm is not installed and `BERLIN_WEB_UI` is off
- WHEN the project builds
- THEN the build succeeds with an empty asset table

#### Scenario: Flag on full pipeline
- GIVEN pnpm and node are installed and the flag is on
- WHEN the project builds
- THEN install, build, and embed run in order and the table contains `index.html`

#### Scenario: Unchanged inputs skipped
- GIVEN a previous successful build, an unchanged input hash, and existing generated files
- WHEN the pre-build runs again
- THEN install, build, and embed are skipped and the build succeeds

#### Scenario: Flag on, pnpm missing
- GIVEN the flag is on and pnpm is absent
- WHEN the project builds
- THEN the build fails with non-zero exit and a message containing "pnpm not found"

#### Scenario: Step failure aborts
- GIVEN the flag is on and `pnpm run build` fails
- WHEN the pre-build runs
- THEN the embed step is not run and the build fails

#### Scenario: Generated files untracked
- GIVEN a completed build
- WHEN `git status` is run
- THEN `Source/ui/generated/EmbeddedAssets.*` do not appear as changes
