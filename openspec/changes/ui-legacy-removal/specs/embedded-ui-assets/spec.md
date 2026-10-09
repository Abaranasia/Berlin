# Delta for embedded-ui-assets

Scenario tags: [V] Vitest, [T] BerlinTests, [M] manual, [B] build.

## MODIFIED Requirements

### Requirement: Asset Lookup

A gui-free helper (`juce_core` only) MUST resolve a request path against the embedded table. `/` MUST map to `index.html`. Unknown paths MUST return `std::nullopt`. Lookup MUST tolerate an empty table (`{nullptr, 0}`), which the unit tests use as a synthetic table; the shipped table is never empty.

(Previously: the empty-table scenario described "the stub (flag-off) empty table".)

#### Scenario: Root maps to index [T]
- GIVEN a table containing `index.html`
- WHEN `/` is looked up
- THEN the `index.html` asset is returned

#### Scenario: Unknown path [T]
- GIVEN any table
- WHEN `/missing.js` is looked up
- THEN `std::nullopt` is returned

#### Scenario: Empty table [T]
- GIVEN a synthetic empty table `{nullptr, 0}`
- WHEN any path is looked up
- THEN `std::nullopt` is returned and nothing crashes

### Requirement: Generated Assets And Pre-Build Pipeline

`Source/ui/generated/EmbeddedAssets.{h,cpp}` MUST be generated and gitignored. Pre-build (all configurations of both `.jucer` files) MUST always run the full pipeline: `pnpm --dir ui install --frozen-lockfile`, `pnpm --dir ui run build`, then `node ui/scripts/embed-assets.mjs` (no `--jucer` argument). The pipeline MUST NOT read any flag, MUST NOT have a stub mode and MUST always produce the full asset table, whose generated header defines `BERLIN_EMBEDDED_ASSETS_FULL 1`. Any failure MUST exit non-zero with a clear message; a missing node MUST fail with the existing BRL001 "node not found on PATH" diagnostic from the pre-build command, and a missing pnpm with the existing BRL002 "pnpm not found on PATH" diagnostic from the embed script. The pipeline MAY skip install, build, and embed when the hash of its inputs (`ui/src/**`, `ui/index.html`, `ui/package.json`, `ui/pnpm-lock.yaml`, `ui/vite.config.ts`, `ui/tsconfig*.json`, `ui/scripts/*`) is unchanged and the generated files exist. The README MUST document node and pnpm as build prerequisites.

(Previously: flag off ran only the embed script in stub mode with an empty table and no pnpm; the flag state was read from the `BERLIN_WEB_UI` define via `--jucer`, and the input hash included the mode.)

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

#### Scenario: Generated files untracked [B]
- GIVEN a completed build
- WHEN `git status` is run
- THEN `Source/ui/generated/EmbeddedAssets.*` do not appear as changes

#### Scenario: README documents prerequisites [M]
- GIVEN the top-level `README.md`
- WHEN read
- THEN a "Build prerequisites" section lists node and pnpm (and the WebView2 runtime and NuGet package)

## REMOVED Requirements

(No whole requirements are removed. The scenarios "Flag off, no pnpm" and "Flag on, pnpm missing" are removed; the latter is replaced by "pnpm missing".)
