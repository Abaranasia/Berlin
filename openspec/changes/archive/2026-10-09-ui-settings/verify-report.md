# Verify Report: ui-settings (UI Redesign Slice 4/6)

Mode: hybrid, Strict TDD. Branch feat/ui-settings @ 48cec2c. Verdict: PASS WITH WARNINGS.

## Completeness
All tasks checked (0 unchecked). 6.5, 6.6, 7.2 and Phase 8 ticked on the user's 2026-10-09 manual pass (flag-ON rebuild and embedded dialog, device/rate/buffer switches, MIDI port/None/hot-unplug, channel change with no hung notes, restart persistence, corrupt settings.xml fallback, Sonar plugin with no Settings button and channel 1).
Commits: 1d75e83 model, 04092cb host/MIDI, 4fbb0c1 bridge, 5ad0584 dialog, 48cec2c fix (saved MIDI port kept when unavailable at launch).
Reviews: review-acc09592b0f00182 approved (4R), review-bd429c6947bf40f2 approved for fix.

## Runtime evidence (run by verify)
| Command | Exit | Result |
|---|---|---|
| BerlinTests.exe --category=Berlin (direct, not piped) | 0 | 410 completed, "All tests completed successfully"; output sha256 f6b55ba4e68c5a8281410687fbf26bdb9815de2efd1ff9fb6c46198ddd4b9cbb |
| pnpm test (ui) | 0 | 37 files, 495 passed; output sha256 d381dddacb963eef1046fff666a1ffcdf16313341fd1493c69dd95ea5858e58e |
| pnpm typecheck (tsc --noEmit) | 0 | no errors |
| pnpm build | 0 | built, no mock chunk; output sha256 6c42e4fb5731c11d01edf2d14136c4f23f0f960871474357174eb564be9c89fc |
Counts match expected (410 / 495). Tree clean after run. Coverage tool: not available.

## Spec compliance
- [V] scenarios (standalone detection, fields populated, None selectable, re-list on open, cancel/close/escape discard, dependent options, dB readout/zero, level via dispatch and shared with TopBar, valid apply, error keeps edits, get/apply shapes, dispatch surface unchanged, detection true/false/no host, Settings button enabled/hidden, keyboard/focus): COMPLIANT, covered by passing SettingsDialog.test.tsx (11), native.test.ts, db.test.ts, App.test.tsx, mock.test.ts.
- [T] scenarios (parse/validate tokens and order, channel range, XML round trip, None explicit, missing/corrupt/version, invalid channel, resolveMidiPort 5 branches, write failure tolerated, payload shape, channel remap, default channel, export/plugin unaffected): COMPLIANT, 25 StandaloneSettings + 5 MidiChannelRemap + MidiMessageBytes tests passed. portToPersist (fix) has 3 tests.
- [M] scenarios (hardware apply, relaunch survives, missing device, plugin registers no functions, callback after teardown, reopen bracketed, failed reopen degrades, flag-off unaffected): user-verified 2026-10-09; no automated test (no headless seam, accepted in tasks/design).
- ui-bridge "Registered only with a host [T]" and "Panic uses current channel [T]": no automated test (host/sink glue lacks a seam); covered by build, review and manual.

## TDD compliance
apply-progress (id 405) records the fix cycle (RED compile failure, GREEN) and earlier batches; no per-task evidence table was seen in the retrieved observation (latest revision is the fix note). Assertion audit of the settings tests: no tautologies, ghost loops or smoke-only tests found.

## Issues
CRITICAL: none.
WARNING:
1. Strict-TDD "TDD Cycle Evidence" table not present in apply-progress #405 as retrieved (only fix note plus summary of earlier batches). Process-evidence gap, tests themselves pass.
2. Two spec scenarios ([T] labelled "Registered only with a host", "Panic uses current channel") have no automated covering test; rely on manual and review evidence.
3. Task 7.2 (ASIO second-driver behavior) has no separately recorded observation; ticked on the user's blanket manual pass.
SUGGESTION:
1. JSX indentation in SettingsDialog.tsx near line 195 (review follow-up).
2. Standalone clipping follow-up remains pending (out of scope).
