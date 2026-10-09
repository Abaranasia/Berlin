# Delta for Sequence Mutation

Verification legend: [V] Vitest, [T] BerlinTests, [M] manual, [B] build.

## MODIFIED Requirements

### Requirement: Manual Mutate Trigger

`MainComponent` MUST expose a `mutate()` method, invoked either by the user clicking the Mutate button or automatically by Auto-Evolve's timer callback (after that callback has confirmed via `isPublishPending()` that no publish is currently pending — see `step-event-scheduling`'s "Publish-Pending Observability" and `auto-evolution`'s "Busy-Collision Check Precedes the Mutate Call"), that: builds a mutated copy of `currentSequence`, publishes a copy via `player.publishSequence()` (mirroring `regenerate()`'s copy-then-publish pattern), and on success sets `currentSequence` to the mutated result so MIDI export and preview reflect it. `mutate()`'s own internal logic MUST NOT differ based on which caller invoked it, including its unconditional "Busy, try again" status-label behavior on a rejected publish — callers that must avoid ever triggering that label (i.e. the automatic path) are responsible for not calling `mutate()` while a publish is pending, not `mutate()` itself suppressing it. While a state restore is pending (see `plugin-state-recall`), `mutate()` MUST return busy exactly as for a pending publish: it MUST NOT change `currentSequence` or the mutation generation counter, and the UI shows the existing "Busy, try again". The automatic path MUST NOT call `mutate()` while a restore is pending.
(Previously: busy meant only a pending publish; a pending state restore was not considered, so a mutation could run on the stale pre-restore sequence.)

#### Scenario: Successful mutate updates playback and export state [T]

- GIVEN a valid `currentSequence`, no pending unadopted publish and no pending restore
- WHEN `mutate()` runs, whether from a manual Mutate click or an automatic Auto-Evolve trigger
- THEN the mutated sequence is published to the player
- AND `currentSequence` becomes the mutated sequence
- AND the mutation generation counter advances by one

#### Scenario: Busy publish on a manual click leaves state unchanged and reports status [T]

- GIVEN a publish is already pending and unadopted
- WHEN the user clicks Mutate
- THEN `mutate()` is called directly (no pre-check), the publish is rejected, `currentSequence` is left unchanged, the mutation generation counter is left unchanged, and the status label shows "Busy, try again" (same contract as `regenerate()`)
- AND a subsequent Mutate click retries from the same generation, reproducing the same candidate mutation

#### Scenario: Busy publish never reaches mutate() on an automatic Auto-Evolve trigger [T]

- GIVEN a publish is already pending and unadopted, and Auto-Evolve's loop-count threshold has just been reached
- WHEN Auto-Evolve's timer callback runs
- THEN it detects the pending publish via `isPublishPending()` BEFORE calling `mutate()`, so `mutate()` is never invoked this tick, `currentSequence` and the mutation generation counter are left unchanged, and no status label is shown
- AND the due mutation is retried on a later timer tick once the publish is no longer pending, not skipped for the remainder of the current rate window

#### Scenario: Mutate returns busy while a restore is pending [T]

- GIVEN a state restore is pending and no publish is pending
- WHEN `mutate()` is called
- THEN it returns busy, `currentSequence` and the mutation generation counter are unchanged and the "Busy, try again" status is reported

#### Scenario: Mutate works again after the restore publishes [T]

- GIVEN a pending restore has published and been cleared
- WHEN `mutate()` is called
- THEN it mutates the restored seeded sequence and succeeds
