## Verification Report: ui-legacy-removal (UI Redesign Slice 5, final)

Mode: Strict TDD, hybrid. Branch feat/ui-legacy-removal at bcde51b, clean tree. Date 2026-10-09.

### Verdict: PASS WITH WARNINGS (0 CRITICAL, 2 WARNING, 2 SUGGESTION)

### Completeness
All tasks checked (0.x-6.x by apply; 7.1-7.8 ticked by verify from the user's 2026-10-09 report). 7.6 (WebView2 fallback label) was reproduced by the user on 2026-10-09 and is correct.

### Runtime evidence (run by verify)
| Command | Exit | Result | Output sha256 |
|---|---|---|---|
| BerlinTests.exe --category=Berlin (Debug x64, not piped) | 0 | 411 tests, "All tests completed successfully" | e56fdd85c442476af81fd3c1cbf8977fbf62307d157cc83eecbc3bdc74b29342 |
| pnpm test (ui) | 0 | 37 files, 490 tests | 4d66bfcf4eab5889106fb2862f5c29b78c3be4f1f66bd15b49a6194ec92f636e |
| pnpm typecheck (tsc --noEmit) | 0 | clean | n/a |
| pnpm build (vite) | 0 | built, no mock chunk | 2841e72fd5b665af88f12f5798e288737d17458d85fe692d1b16a0234869a78a |

The C++ test binary was not rebuilt by verify; it is the existing Debug binary, whose count (411) and fallback-message tests match HEAD. The MSBuild builds were not re-run (apply reported them clean; the user-run manual builds in Phase 7 passed).

Grep gate (re-run, Source, ui/scripts, ui/src, both .jucer, README.md, plus Tests/BerlinTests.jucer): zero matches for BERLIN_WEB_UI not followed by _DEV_URL, BerlinAudioProcessorEditor, readWebUiFlag.

### Strict TDD compliance
- apply-progress (id 416) has no formal "TDD Cycle Evidence" table, only prose: embed-lib rewrite RED (14 failing) then GREEN; BRL008 and webViewFallbackMessage RED then GREEN. See WARNING 1.
- Reported test files exist and pass: ui/scripts/embed-lib.test.mjs (stale-stub test line 88, BRL008 block line 305), Tests/Source/WebEditorSupportTests.cpp (fallback message, line ~204).
- Assertion quality: spot-checked the new tests; they assert concrete values (FULL 1 content, thrown BRL008 regex, distinct runtime vs folder messages). No tautologies or ghost loops seen.
- Layers: Vitest unit (embed-lib and web), BerlinTests unit; no integration/E2E for the C++ GUI glue (no headless seam; covered by [M]).
- Coverage tool: not run (none configured); skipped.

### Spec compliance matrix
| Scenario | Evidence | Status |
|---|---|---|
| embedded-ui-assets: Root maps to index / Unknown path / Empty table [T] | existing WebAssetsTests inside BerlinTests 411 pass | COMPLIANT |
| Full pipeline [B] | builds, user 7.4 | COMPLIANT (manual) |
| Pipeline ignores any flag [V] | embed-lib tests (no readWebUiFlag/mode, runPipeline() no args), grep gate | COMPLIANT |
| Generated header guard [V] | embed-lib.test.mjs lines 5-27, 84-109 | COMPLIANT |
| Unchanged inputs skipped [V] | embed-lib skip tests pass | COMPLIANT |
| pnpm missing BRL002 / node missing BRL001 [B] | user 7.5 | COMPLIANT (manual, user-reported) |
| Step failure aborts [B] | existing runPipeline tests pass | COMPLIANT |
| Empty build output fails BRL008 [V] | embed-lib.test.mjs line 305 | COMPLIANT |
| Generated files untracked [B] | git status clean after builds; user 7.4 | COMPLIANT |
| README documents prerequisites [M] | README.md "Build prerequisites" + BRL table inspected | COMPLIANT |
| web-editor: Standalone gets WebEditor with host / Plugin without host [M] | user 7.1-7.3 | COMPLIANT (manual) |
| No legacy editor or flag remains [B] | grep gate re-run, 0 matches | COMPLIANT |
| Stale stub header fails loudly [B] | static_assert kept; user 7.4 | COMPLIANT (manual) |
| Runtime missing / User data folder cannot be created [M] | webViewFallbackMessage unit tests (runtime vs folder messages) pass; user 7.6 reproduced | COMPLIANT (user reproduced 2026-10-09) |
| Controls beyond BPM present [M] | user 7.3; Vitest 490 | COMPLIANT |
| plugin-host-integration: reopen / auto-evolve / teardown / no host settings [M] | user 7.3, 7.7 | COMPLIANT (manual) |
| Standalone window resizable / no fixed-size / content follows [M] | user 7.1 | COMPLIANT (manual) |
| unit-test-harness scenarios | BerlinTests builds and passes (411); no editor/WebEditor/EmbeddedAssets registered in Tests/BerlinTests.jucer (grep clean) | COMPLIANT |
| web-ui-controls: Token mapping, Unmapped, Busy, writeFailed, Midpoint, Round trip [V] | Vitest 490 pass | COMPLIANT |

Manual [M] and [B] scenarios rely on the user's 2026-10-09 report; verify did not re-execute them.

### Design coherence (D1-D6)
D1 thin createBerlinEditor wrapper, D2/D3 always-FULL 1 with fixed mode:full preamble, D4 const-ref constrainer and always-resizable window, D5 .jucer edits committed with Projucer output, D6 no --jucer arg: all consistent with the commit set (547da36, 8adac76, 8ac9db7, 105cdbd, bcde51b). Deviation: BRL008 and webViewFallbackMessage were added after review (not in design); both were reviewed (e33d59c07ee40918 approved; 0ebe822f8d1fffa1 approved for the fix) and are reflected in the spec and README.

### Issues
CRITICAL: none.

WARNING
1. apply-progress lacks the formal "TDD Cycle Evidence" table; TDD is described only in prose. RED/GREEN counts are consistent with the code, so this is a reporting gap, not a protocol failure.
2. Test baseline moved from 410 to 411 and Vitest from 495 to 490, which differs from the tasks text ("410 unchanged"); explained by the added fallback test and the embed-lib test rewrite, and recorded in 7.8.

SUGGESTION
1. Consider recording the Phase 7 [M] results in apply-progress as the task states (7.8 asks for it).
2. Add a headless seam or smoke script for the WebEditor fallback path in a future slice so Runtime missing / folder failure become automated.

### Next
sdd-archive (no CRITICAL).
