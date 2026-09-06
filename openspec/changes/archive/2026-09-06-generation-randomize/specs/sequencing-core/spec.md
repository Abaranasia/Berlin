# Delta for Sequencing Core

## ADDED Requirements

### Requirement: Sequence Supports O(1) Swap

The system MUST provide `Sequence::swap(Sequence&)`, exchanging the contents of two `Sequence` instances in constant time with no heap allocation or deallocation. This exists specifically so the audio thread can adopt a newly-published `Sequence` (per `realtime-audio-wiring`'s handoff contract) without ever calling into the allocator.

#### Scenario: Swap exchanges contents

- GIVEN two distinct `Sequence` instances, A and B, with different contents
- WHEN `A.swap(B)` is called
- THEN A now holds B's original contents and B now holds A's original contents

#### Scenario: Swap allocates nothing

- GIVEN two `Sequence` instances of any size
- WHEN `swap` is called
- THEN no heap allocation or deallocation occurs, and the call is `noexcept`
