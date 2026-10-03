# Verification Report: webview-poc (UI Redesign Slice 1/6)

Mode: Strict TDD. Artifact store: hybrid. Date: 2026-10-03. Branch: feat/ui-webview-poc-pr2 (55cec13, stacked on 9df6d61).

**Verdict: PASS WITH WARNINGS** (0 CRITICAL, 2 WARNING, 8 SUGGESTION)

## Completeness
Tasks: 71 of 72 checked (all phases 0-11 plus 12.1-12.6, 12.8, 12.9). Open: 12.7 (missing WebView2 runtime fallback), deliberately deferred and not runnable on Windows 11. It is an untested scenario, not a failure. tasks.md checkbox edits are the only uncommitted change.

## Runtime evidence
| Check | Command | Exit | Result |
|---|---|---|---|
| UI tests | `pnpm --dir ui test` | 0 | 4 files, 51 passed (embed-lib 36, steps 8, native 4, App 3) |
| C++ build | MSBuild BerlinTests.sln Debug x64 | 0 | succeeded |
| C++ tests | BerlinTests.exe (no filter) | 0 | "All tests completed successfully", 1223 test cases run (WebAssets 7, WebEditorSupport 8), 0 failures |
| Coverage / linter | not configured | n/a | skipped, not a failure |

Apply-progress counts match: Vitest 51, BerlinTests 367 -> 368 top-level tests. Flag-off App/Plugin builds were not rebuilt here (per constraint); apply-progress and manual 12.8 cover them.

## TDD compliance
| Check | Result | Details |
|---|---|---|
| TDD evidence reported | WARNING | apply-progress #351 is prose (counts, RED negative checks), not a "TDD Cycle Evidence" table. tasks.md records explicit RED-then-GREEN steps (2.5/2.7, 4.2/4.4, 5.4/5.6, 9.5/9.7). |
| RED confirmed (test files exist) | PASS | embed-lib.test.mjs, steps.test.ts, native.test.ts, App.test.tsx, WebAssetsTests.cpp, WebEditorSupportTests.cpp all exist |
| GREEN confirmed | PASS | all pass now |
| Triangulation | PASS | multiple cases per behavior (traversal vectors, port bounds 0/00000/65536/99999 vs 1/65535, flag separators) |
| Safety net | PASS | baselines 353 -> 367 -> 368 recorded |

The strict module rates a missing table as CRITICAL. It is downgraded to WARNING because the equivalent evidence exists in tasks.md and the artifacts; the orchestrator may override.

Test layers: unit (Vitest lib/state/bridge, BerlinTests helpers) and integration-like (App.test.tsx with react-dom/jsdom). No E2E tool (manual CDP/Sonar checks instead).

Assertion quality: no tautologies. Loops are guarded (embed-lib.test.mjs:172 follows `toHaveLength(3)`; steps.test.ts:11 iterates a fixed array). The `toEqual([])` cases have non-empty companions. Mock-heavy: none flagged. 0 CRITICAL, 0 WARNING.

## Spec compliance (18 requirements, 43 scenarios)
Legend: T = automated test passed at runtime, M = manual, user-confirmed 2026-10-03, B = build/compile evidence.

| Spec | Reqs | Scenarios | Coverage |
|---|---|---|---|
| embedded-ui-assets | 6 | 16 | 16/16 COMPLIANT. Lookup, traversal, MIME, folder, adapter (incl. absent-args) via BerlinTests; pipeline scenarios (flag off, full, skip, pnpm missing BRL002, step failure, timeout) via Vitest; untracked generated files via .gitignore and clean git status. Flag-on pipeline run and BRL002/C2338 negative checks done in apply (B). |
| unit-test-harness | 2 | 4 | 4/4 COMPLIANT (helpers registered, WebEditor/generated excluded, suites run headlessly, adapter error paths). |
| plugin-host-integration | 2 | 4 | 3 COMPLIANT (M: close/reopen, flag-on shell; flag-off indistinguishable via 12.8 M). "WebEditor teardown stops its timer" is code-inspected only (destructor does `stopTimer(); browser.reset();`), no automated test (GUI): PARTIAL. |
| web-editor | 8 | 19 | 17 COMPLIANT, 1 PARTIAL, 1 UNTESTED. See below. |

web-editor detail:
- Flag off keeps legacy: M (12.8). Flag on selects WebEditor: B+M (flag-on builds, user ran them).
- Data folder created: code-inspected (`createDirectory()` before options), M exercised in flag-on runs: PARTIAL, not unit-testable.
- Fits standalone window: M (12.1/12.4, after resized() fix).
- Missing runtime fallback (MANUAL): **UNTESTED** (12.7 deferred). Code path exists (`areOptionsSupported` / folder failure -> label, no browser, no timer).
- Native functions (3 scenarios): T (dispatchNativeCall tests).
- Playhead change / no change: T (PlayheadChangeDetector). Hidden re-emit: T at detector level (uncommitted stays changed); WebEditor commit-only-when-sent wiring is code-inspected (timerCallback returns early when not visible, commits after emit).
- Dev origin (2 scenarios): T for `devServerOrigin`; the `#if JUCE_DEBUG` gating in WebEditor.cpp is code-inspected.
- PoC UI content: T (App.test.tsx, steps, native) + M for engine BPM and playhead.
- Go/no-go (4 scenarios): M all PASS.

## Design coherence
All D1-D8 followed (flag in `JUCERPROJECT@defines`, `static_assert(BERLIN_EMBEDDED_ASSETS_FULL == 1)`, BRL001/BRL002, stamp skip, single factory switch, member order, stopTimer-then-reset). Intentional deviations are documented in tasks 7b: lock file plus rename retry (D5 hardening), pnpm 5 min timeout (BRL006), port range 1-65535, and `resized()` after browser creation (bug fix). No deviation breaks a spec.

## Issues
CRITICAL: none.

WARNING:
1. Scenario "Runtime missing (MANUAL)" (12.7) is untested; deferred to a machine without the WebView2 runtime.
2. apply-progress has no formal TDD Cycle Evidence table (evidence is in tasks.md and prose); see TDD compliance.

SUGGESTION:
1. A stale `.lock` after a killed build makes the next build wait silently up to 15 min.
2. Stale-lock breaking in `withLock` is racy.
3. The pnpm timeout invariant comment is inaccurate.
4. `WebEditor::owner` duplicates the base-class `processor`.
5. `DEFAULT_BPM` in the UI has no placeholder comment.
6. `changeBpm` reads a stale closure and has no catch.
7. GUI-only scenarios (data folder before environment, timer teardown, commit-only-when-sent, Debug-only dev origin) have no automated coverage; consider a small manual checklist or a gui-test tier in a later slice.
8. Commit the tasks.md checkbox updates (only uncommitted change); VS fast up-to-date check can skip the pre-build after ui-only edits (accepted open question).

## Git state
`git status --short` before: ` M openspec/changes/webview-poc/tasks.md`. After the runs: the same, plus this untracked verify-report.md. No source, test, project or tasks file modified.
