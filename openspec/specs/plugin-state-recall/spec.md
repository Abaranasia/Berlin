# Plugin State Recall Specification

## Purpose

Defines how a DAW session persists and restores `BerlinAudioProcessor` state via `getStateInformation`/`setStateInformation`, reusing `PresetManager`'s versioned `ValueTree` so plugin state and preset files share one schema. Restoring a session re-derives the sequence from its seed; it does not replay mutation history.

## Requirements

### Requirement: State Save Serializes Patch And Seed

`getStateInformation` MUST serialize the current patch and seed using `PresetManager::toValueTree` into the host-provided `juce::MemoryBlock`, with no other engine state (mutation count, live sequence) included.

#### Scenario: Saved state matches PresetManager's tree

- GIVEN a processor with a current patch and seed
- WHEN `getStateInformation` is called
- THEN the resulting block deserializes to the same `ValueTree` `PresetManager::toValueTree` would produce for that patch/seed

### Requirement: State Restore Re-Derives The Sequence From The Seed

`setStateInformation` MUST call `PresetManager::fromValueTree` on the restored data, then regenerate the sequence deterministically from the restored seed. The system MUST NOT attempt to restore mutation count or the exact pre-save mutated sequence; this is an accepted, documented consequence, not a defect.

#### Scenario: Session reload restores patch and seed, not the mutated chain

- GIVEN a session saved after several mutations increased `mutationCount`
- WHEN the host restores that state in a new instance
- THEN the patch and seed match the saved values, and the resulting sequence equals a fresh regenerate from that seed, not the mutated-in-place sequence

#### Scenario: Malformed or empty state does not crash

- GIVEN `setStateInformation` receives an empty or corrupt block
- WHEN it is processed
- THEN the processor's current patch and seed remain untouched, without crashing or asserting

### Requirement: Round-Trip Determinism

Given the same seed, saving then restoring state MUST produce a sequence bit-identical to a fresh regenerate from that same seed, independent of how many mutations occurred before the save.

#### Scenario: Two restores from the same saved state agree

- GIVEN one saved state block
- WHEN it is restored into two separate processor instances
- THEN both instances produce identical sequences
