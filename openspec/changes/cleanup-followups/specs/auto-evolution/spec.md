# Delta for Auto-Evolution

Verification legend: [V] Vitest, [T] BerlinTests, [M] manual, [B] build.

## MODIFIED Requirements

### Requirement: Auto-Evolve Toggle and Rate Control

The system MUST provide a new full-width UI row below `generationButtonRow` containing an Auto-Evolve enable/disable toggle (off by default) and a rate selector (loops-per-mutation: 1/2/4/8/16, default 4). While the toggle is off and no restore is pending, behavior MUST be byte-identical to today (no timer-driven mutation, no polling). A pending state restore (see `plugin-state-recall`) MAY keep the timer running while the toggle is off, but it MUST NOT cause any automatic mutation.
(Previously: while the toggle was off there was never any polling; the timer now also runs for a pending restore.)

#### Scenario: Off by default [T]

- GIVEN the application starts with no prior Auto-Evolve interaction
- WHEN playback runs for any number of loops
- THEN `mutate()` is never called automatically

#### Scenario: Enabling schedules future mutations only [T]

- GIVEN Auto-Evolve is toggled on with rate N
- WHEN loops complete after the toggle was enabled
- THEN the first automatic mutation fires after N loop completions counted from enable time, not retroactively for loops already completed

## ADDED Requirements

### Requirement: Timer Runs Only While Needed

The processor's timer MUST run if and only if auto-evolve is enabled OR a state restore is pending. Turning auto-evolve off while a restore is pending MUST keep the timer running until the restore publishes. Once auto-evolve is off and no restore is pending, the timer MUST stop. The timer state MUST be re-evaluated whenever auto-evolve is toggled and whenever a restore is retried successfully or cleared.

#### Scenario: Restore retry survives auto-evolve off [T]

- GIVEN auto-evolve is off and a restore is pending
- WHEN the pending publish is adopted and a tick runs
- THEN the restore publishes, `restorePending` is cleared and the timer stops

#### Scenario: Turning auto-evolve off keeps a pending restore alive [T]

- GIVEN auto-evolve is on and a restore is pending
- WHEN auto-evolve is turned off
- THEN the timer is still running and a later tick still retries the restore

#### Scenario: Timer stops when neither condition holds [T]

- GIVEN auto-evolve is off and a restore was pending
- WHEN the restore publishes (or is cleared by regenerate or `loadPreset`)
- THEN the timer is not running

### Requirement: Restore Retry Precedes And Replaces The Tick's Mutation

Each tick, while a restore is pending, MUST retry the restore regenerate and MUST do nothing else that tick: no loop-count evaluation and no automatic mutation, even if one is due. On success the tick MUST clear the pending flag, send a change message and re-evaluate the timer. On success the tick MUST also reset the auto-evolve schedule to the current loop count, so the restored sequence gets a full rate period before its first automatic mutation and a mutation that was due while pending is discarded. On a busy result the flag MUST stay set.

#### Scenario: Pending restore blocks a due mutation [T]

- GIVEN auto-evolve is on, a mutation is due and a restore is pending
- WHEN a tick runs
- THEN the restore is retried, `mutate()` is not called and the due threshold is not consumed

#### Scenario: Restore publish restarts the schedule [T]

- GIVEN auto-evolve is on, a mutation was due while the restore was pending, and the restore published on the previous tick
- WHEN the next tick runs with no publish pending
- THEN `mutate()` is not called; the first automatic mutation comes one full rate period after the restore published

#### Scenario: Successful retry sends a change message [T]

- GIVEN a restore is pending and the blocking publish was adopted
- WHEN a tick runs
- THEN a change message is sent so the UI refreshes its snapshot
