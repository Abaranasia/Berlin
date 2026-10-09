# Archive Report: ui-legacy-removal (UI Redesign Slice 5, final)

**Date**: 2026-10-09  
**Branch**: feat/ui-legacy-removal  
**HEAD**: bcde51b (fix: fail the build on empty UI output and name the WebView fallback cause)  
**Artifact Store**: hybrid (filesystem + Engram)

## Executive Summary

UI Redesign Slice 5 is complete and archived. The legacy JUCE editor has been deleted, the flag is removed, and the pipeline always runs full. All tasks passed verification with PASS WITH WARNINGS (0 CRITICAL, 2 WARNING, 2 SUGGESTION). Five main capability specs have been merged with delta requirements and are now the source of truth. This completes the entire UI Redesign (Slices 0–5).

---

## Specification Merge Summary

### Specs Updated

| Domain | File | Action | Requirements Before | Requirements After | Notes |
|--------|------|--------|---|---|---|
| web-editor | `openspec/specs/web-editor/spec.md` | MODIFIED | 12 | 12 | ADDED "WebEditor Is The Only Editor"; REMOVED "Compile-Time Editor Selection"; MODIFIED "Missing Runtime Fallback", "UI Content Defers To web-ui-controls" |
| plugin-host-integration | `openspec/specs/plugin-host-integration/spec.md` | MODIFIED | 5 | 5 | MODIFIED "Processor Owns All Engine State", "Editor Attach/Detach Does Not Affect Engine State" (added "Plugin editor has no host settings" scenario), "Standalone Shell Preserves Today's Observable Behavior" |
| embedded-ui-assets | `openspec/specs/embedded-ui-assets/spec.md` | MODIFIED | 6 | 6 | MODIFIED "Asset Lookup", "Generated Assets And Pre-Build Pipeline"; added scenarios "Empty build output fails", "README documents prerequisites", "Pipeline ignores any flag", "Generated header guard" |
| unit-test-harness | `openspec/specs/unit-test-harness/spec.md` | MODIFIED | 6 | 6 | MODIFIED "Console Test Runner Project" (removed BerlinAudioProcessorEditor references); updated scenario "BerlinAudioProcessor is registered, no editor is" |
| web-ui-controls | `openspec/specs/web-ui-controls/spec.md` | MODIFIED | 27 | 26 | MODIFIED "Status Line And Error Messages", "Skewed Sliders"; REMOVED "Manual Parity Checks (MANUAL)" requirement and its 2 scenarios |

**Total requirements before**: 56  
**Total requirements after**: 55  
**Net change**: −1 (1 requirement removed, 1 added, net effect on 5 specs)

### Purpose Line Changes

- **web-editor**: "Defines the opt-in `WebEditor`... Selected at compile time by `BERLIN_WEB_UI` (default off)." → "Defines the `WebEditor`... It is the only editor of the standalone app and the plugin."
- **plugin-host-integration**: "`BerlinAudioProcessor`/`BerlinAudioProcessorEditor` pair" → "`BerlinAudioProcessor`/`WebEditor` pair"
- **web-ui-controls**: "gives full legacy-editor parity through `UiBridge` only" → "provides all engine controls through `UiBridge` only"

---

## Implementation Evidence

### Commits

| SHA | Message |
|-----|---------|
| 547da36 | refactor(ui): always embed the full web UI |
| 8adac76 | refactor: make WebEditor the only editor |
| 8ac9db7 | refactor: delete the legacy JUCE editor |
| 105cdbd | docs: add build prerequisites and drop legacy-editor comments |
| bcde51b | fix: fail the build on empty UI output and name the WebView fallback cause |

### Review

- **review-e33d59c07ee40918**: approved (changes 1–4)
- **review-0ebe822f8d1fffa1**: approved (fix commit, BRL008 + webViewFallbackMessage tests)

### Verification

**Verdict**: PASS WITH WARNINGS (0 CRITICAL, 2 WARNING, 2 SUGGESTION)

| Test | Status | Evidence |
|------|--------|----------|
| BerlinTests (Debug x64) | PASS | 411 tests, exit 0; SHA e56fdd85... |
| Vitest (ui) | PASS | 490 tests, exit 0; SHA 4d66bfcf... |
| TypeScript (tsc) | PASS | Clean, exit 0 |
| Vite build | PASS | Built, no mock chunk, SHA 2841e72f... |
| Grep gate | PASS | No BERLIN_WEB_UI (except _DEV_URL), no BerlinAudioProcessorEditor, no readWebUiFlag |
| Manual Phase 7 [M] | PASS | User report 2026-10-09; 7.6 WebView2 fallback reproduced |

**Warning 1**: apply-progress lacks formal "TDD Cycle Evidence" table; compliance verified by code review.  
**Warning 2**: Test baseline moved 410→411 (fallback test), Vitest 495→490 (embed-lib rewrite); explained and recorded.

**Suggestions**:
1. Record Phase 7 [M] results in apply-progress as task 7.8 requests.
2. Add headless seam or smoke script for WebEditor fallback in a future slice.

---

## Capabilities Updated

**Canonical deltas applied from**: `openspec/changes/ui-legacy-removal/specs/{web-editor,plugin-host-integration,embedded-ui-assets,unit-test-harness,web-ui-controls}/spec.md`

All five domain specs now reflect:
- No compile-time flag; WebEditor is always the editor
- Pipeline always runs full, ignores any flag, requires node and pnpm
- Fallback message differentiates runtime missing vs. user-data-folder failure
- Settings dialog hidden in plugin, available in standalone via StandaloneHost
- Standalone window always resizable 960x680 (min 720x520, max 1920x1200)
- No manual legacy-editor parity checks (already verified before removal)
- Status messages wording corrected (legacy function references dropped)

---

## Follow-Ups and Notes

### Known Issues (Low Priority)

1. **TDD Evidence Table**: apply-progress has no formal "TDD Cycle Evidence" table. `embed-lib.test.mjs` RED→GREEN, BRL008 RED→GREEN, webViewFallbackMessage RED→GREEN verified by inspection. Recommend tabular format in future SDD tasks.

2. **WebEditor Fallback Seam**: No headless seam or smoke test for the Runtime-missing / user-data-folder-failure path. The path is covered by unit tests (webViewFallbackMessage) and manual verification (user 7.6), but integration automation would improve confidence. Defer to Phase 12 or later slice.

3. **Slice 3 Follow-Ups**: Spec scenarios tagged [V]/[T]/[M]/[B] in Slice 3 (ui-layout-style, commit d5b0ab5) remain open:
   - RES-002 "Resolution adjustment follows display scaling" [M, deferred]
   - "Keyboard focus in DAW" [M, verified]
   - "DPI scaling 100% and 150%+" [M, verified]

4. **Settings Dialog Indentation**: `ui/src/components/SettingsDialog.tsx` has mixed indentation (tabs/spaces). Minor nit, no functional issue. Consider normalizing in a future cleanup slice.

5. **Remaining "Legacy" Wording**: Comments in `ui/src/` may still reference "legacy" in casual prose (e.g., "was the legacy behavior"). The requirement-level "legacy" references have been removed. Recommend a minor grep/comment-cleanup slice if precision is desired.

---

## Completion Status

- ✅ **Tasks**: All 7 phases (0–6) completed by apply. Phase 7 [M] manual verification completed by user 2026-10-09.
- ✅ **Verification**: PASS WITH WARNINGS; no CRITICAL blockers.
- ✅ **Specs Merged**: Five domains updated, requirements in sync with implementation.
- ✅ **Review**: Two reviews approved; all feedback integrated.
- ⏳ **Archive**: This report replaces the change folder; folder move to `openspec/changes/archive/2026-10-09-ui-legacy-removal/` is a subsequent orchestrator step.

---

## Observation IDs (Engram Traceability)

- **Proposal** (id 412): sdd/ui-legacy-removal/proposal
- **Spec** (id 413): sdd/ui-legacy-removal/spec
- **Design** (id 414): sdd/ui-legacy-removal/design
- **Tasks** (id 415): sdd/ui-legacy-removal/tasks
- **Verify Report** (id 418): sdd/ui-legacy-removal/verify-report
- **Archive Report** (this document): sdd/ui-legacy-removal/archive-report

---

## What's Next

**UI Redesign is complete**: Slices 0–5 (Slice 0 layout, Slice 1 controls, Slice 2 settings, Slice 3 ui-layout-style, Slice 4 MIDI device/channel config, Slice 5 legacy removal) are all archived.

Recommended next phases (from roadmap):
- Phase 12+: Plugin parameter automation compatibility, VST3/AU host test, preset cloud sync, or backend integration.
- Or: Continue with Berlin School authenticity slices (Slice 6+).

No blocking technical debt; the codebase is clean and ready for new work.

---

**Archived by**: sdd-archive executor  
**Archive date**: 2026-10-09
