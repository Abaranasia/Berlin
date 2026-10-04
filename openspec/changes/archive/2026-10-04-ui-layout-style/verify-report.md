# Verify Report: ui-layout-style (UI Redesign Slice 3/6)

Date: 2026-10-04. Mode: Strict TDD, hybrid. HEAD cf3a7c8 on feat/add-ui-layout-pr2 (PR A 713c2d9, 8c0c5a9, e62f819; PR B 361888b, 1ecf180, cf3a7c8).

## Verdict: PASS WITH WARNINGS (0 CRITICAL, 6 WARNING, 3 SUGGESTION)

## Completeness
All tasks 0.x-9.x and R.1-R.6 ticked (9.16 ticked by this run). Phase 9 manual 9.1-9.15 PASS (user, 2026-10-04, standalone + Cakewalk Sonar, flag-on Debug build of cf3a7c8; flag-off standalone 800x952 not clipped, fixed; flag-off plugin unchanged). Note: clicking a delay chip only copies, by design.

## Runtime evidence
| Command | Result |
|---|---|
| `pnpm vitest run` (ui/) | exit 0, 35 files, 462 tests passed |
| `pnpm exec tsc --noEmit` | exit 0 |
| `pnpm build` | exit 0, 75 modules, no mock chunk, 4 local woff2, css 5.91 kB; built CSS has no http(s) URL, `--accent: #f0a238` |
| `BerlinTests.exe --category=Berlin` | exit 0, 379 tests completed, "All tests completed successfully" (suite unchanged by this slice) |
| C++ builds | Not rebuilt per instructions; apply runs: standalone + plugin flag-on/off Debug x64 0 errors; fresh flag-on plugin rebuild of cf3a7c8: 0 errors, 0 warnings |

## Strict TDD compliance
apply-progress carries RED/GREEN evidence per task (knob.ts, Knob.tsx, panel swap, DelayTimesCard, App chip, follow-ups; noteName/panelState/shell in PR A results). Test files exist and pass now. Triangulation adequate (12 knob lib, 18 Knob incl. 6 skew midpoints, 7 card, panelState 7 cases). Safety net recorded for Slider refactor (9/9). Not in the exact table format but evidence is complete. Assertion audit: no tautologies, no ghost loops (the only loops iterate fixed non-empty arrays: Knob.test.tsx L22, L150). Coverage tool not configured (skipped). C++ sizing has no headless seam (documented convention); covered by manual 9.x.

## Spec scenario compliance
| Scenario | Evidence | Status |
|---|---|---|
| Tokens defined, No network fonts [V] | styles.test.ts + built CSS scan | COMPLIANT |
| Focus ring, Offline rendering, Wide/Narrow/Minimum viewport [M] | 9.5-9.9 | COMPLIANT (manual) |
| Controls in regions, Parity, Disabled placeholder | App.test.tsx | COMPLIANT |
| Default closed, Toggle persists, Storage throws | panelState.test.ts + App.test.tsx | COMPLIANT |
| Drag math, Skewed knob, Keyboard, Disabled, No wheel or reset | knob.test.ts, Knob.test.tsx | COMPLIANT |
| Pointer feel [M] | 9.10 | COMPLIANT (manual) |
| Gate distinction, Playhead | noteName.test.ts, App.test.tsx | COMPLIANT |
| Copy success/failure, Rendered as buttons, At 120 BPM, Updates with BPM | DelayTimesCard.test.tsx, App.test.tsx, delayRecs.test.ts | COMPLIANT |
| Midpoint (6 skewed), Round trip and endpoints | Knob.test.tsx, knob.test.ts | COMPLIANT |
| Clipboard in DAW [M] | 9.11 | COMPLIANT (manual) |
| web-editor Initial size/limits, Resize in host [M] | WebEditor.cpp L44-46 + 9.1, 9.2 | COMPLIANT (manual) |
| web-editor Flag on selects / Data folder | unchanged behavior; build + BerlinTests | COMPLIANT |
| plugin-host-integration all four [M] | MainComponent.cpp, Main.cpp + 9.3, 9.4, 9.14, 9.15 | COMPLIANT (manual) |

## Design coherence
D1-D13 honored (CSS import order, hand-written @font-face, drawer as aside with hidden, Knob label+hidden range input, ref drag state, clipboard wrapper, panelState, card/StepCell, WebEditor limits, MainComponent unique_ptr editor + constrainer). Deviations documented in apply-progress (chips card full-width after card-grid; data-knob test hooks; no chip aria-label): none break a spec.

## Issues
CRITICAL: none.

WARNING (open review follow-ups, not fixed):
1. RES-002 Knob.tsx move() lacks a disabled / e.buttons guard and has no lostpointercapture handler (only pointerdown checks disabled/button; stray setPatch possible if disabled mid-drag or capture lost).
2. RES-001 (PR B review) window frame not re-applied after DPI/monitor change (Main.cpp applyEditorLimits only provisional + after setVisible).
3. READ-001 lib/knob.ts imports Skew type from components/controls/Slider (type-level cycle).
4. READ-002 positionOf computed twice in Knob.
5. READ-003 knob geometry constants duplicated between knob.ts and Knob.tsx.
6. Strict-TDD: READ-001 `declared()` fix was applied before test run (regression guard, never observed RED); C++ RES-001 window-frame fix is build/manual verified only.

SUGGESTION:
1. Note naming MIDI 60 = C4 (Sonar shows C5); consider a setting later.
2. Knob values use Slider numeric format; unit formatting deferred.
3. Add a headless seam for window sizing math to allow C++ tests.

Next: sdd-archive (no blockers).
