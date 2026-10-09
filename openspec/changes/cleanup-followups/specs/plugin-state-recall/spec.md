# Delta for Plugin State Recall

Verification legend: [V] Vitest, [T] BerlinTests, [M] manual, [B] build.

## MODIFIED Requirements

### Requirement: State Restore Re-Derives The Sequence From The Seed

`setStateInformation` MUST call `PresetManager::fromValueTree` on the restored data, then regenerate the sequence deterministically from the restored seed. The system MUST NOT attempt to restore mutation count or the exact pre-save mutated sequence; this is an accepted, documented consequence, not a defect. If the regenerate finds a publish pending, the restore MUST NOT be dropped: it MUST stay pending and be retried (see "Busy Restore Stays Pending Until It Publishes").
(Previously: the regenerate result was ignored, so a restore that hit a pending publish left the sequence, sequence seed and `edited` stale while the seed and patch were already restored.)

#### Scenario: Session reload restores patch and seed, not the mutated chain [T]

- GIVEN a session saved after several mutations increased `mutationCount`
- WHEN the host restores that state in a new instance
- THEN the patch and seed match the saved values, and the resulting sequence equals a fresh regenerate from that seed, not the mutated-in-place sequence

#### Scenario: Malformed or empty state does not crash [T]

- GIVEN `setStateInformation` receives an empty or corrupt block
- WHEN it is processed
- THEN the processor's current patch and seed remain untouched, without crashing or asserting
- AND no restore is left pending

## ADDED Requirements

### Requirement: Busy Restore Stays Pending Until It Publishes

When `setStateInformation` finds a publish pending, the processor MUST mark the restore as pending (`restorePending`). The restored seed and generation params MUST already be applied. `edited` MUST stay true and the previous sequence MUST keep playing until a retry publishes. Each retry MUST regenerate from the restored seed. On the first successful retry the processor MUST clear `restorePending`, publish the seeded sequence, clear `edited`, and send a change message. A successful explicit regenerate or `loadPreset` MUST clear `restorePending`; a busy one MUST leave it set.

#### Scenario: Busy restore publishes on a later retry [T]

- GIVEN a publish is pending and edits made `edited` true
- WHEN `setStateInformation` restores a valid state
- THEN the restored seed and params are applied, `restorePending` is set, `edited` is still true and the old sequence is still the live one
- AND once the pending publish is adopted and the retry runs, the sequence equals a fresh regenerate from the restored seed, `edited` is false, `restorePending` is cleared and a change message was sent

#### Scenario: Restore without a pending publish is unchanged [T]

- GIVEN no publish is pending
- WHEN `setStateInformation` restores a valid state
- THEN the sequence is regenerated immediately and `restorePending` is not set

#### Scenario: Retry still busy keeps the restore pending [T]

- GIVEN `restorePending` is set and the publish is still pending
- WHEN a retry runs
- THEN `restorePending` stays set and no change message is sent

#### Scenario: Successful regenerate clears the pending restore [T]

- GIVEN `restorePending` is set and no publish is pending
- WHEN an explicit regenerate succeeds
- THEN `restorePending` is cleared and no later retry publishes

#### Scenario: Busy regenerate leaves the pending restore set [T]

- GIVEN `restorePending` is set and a publish is pending
- WHEN an explicit regenerate is rejected as busy
- THEN `restorePending` stays set

#### Scenario: loadPreset clears the pending restore [T]

- GIVEN `restorePending` is set and no publish is pending
- WHEN `loadPreset` succeeds
- THEN `restorePending` is cleared and the loaded preset's sequence stays live
