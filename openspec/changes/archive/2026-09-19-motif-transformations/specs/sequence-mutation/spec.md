# Delta for Sequence Mutation

## MODIFIED Requirements

### Requirement: Pure Mutation Transforms

`MutationEngine` MUST expose stateless free functions, each `(const Sequence&, DeterministicRandom&) -> Sequence`, for: transpose, reverse, rotate, change-note, add-step, remove-step, stretch, compress, invert, scaleIntervals, palindrome. Each transform MUST be total — no crash or UB for any valid `Sequence`, including size 0, size 1, or zero active steps.
(Previously: transform set had 8 entries; grows to 11 with invert, scaleIntervals, palindrome — same signature and totality guarantee.)

#### Scenario: Transpose shifts active note pitches only

- GIVEN a Sequence with active and inactive steps
- WHEN transpose is applied
- THEN every active step's `note` shifts by the same signed offset
- AND inactive steps remain unchanged (note value untouched)

#### Scenario: Degenerate input never crashes

- GIVEN a Sequence of size 0, size 1, or with zero active steps
- WHEN any transform is applied
- THEN a valid Sequence is returned with no crash or undefined behavior

#### Scenario: Invert mirrors active notes about the first active step's pitch

- GIVEN a Sequence with active steps at various notes
- WHEN invert is applied
- THEN each active step's note becomes `2a - n`, where `a` is the note of the first active step and `n` is that step's original note
- AND inactive steps and sequence size are unchanged

#### Scenario: Invert is an involution when no correction applies

- GIVEN a Sequence whose mirrored notes all fall within [0, 127]
- WHEN invert is applied twice in succession
- THEN the second application restores the original Sequence exactly

#### Scenario: scaleIntervals augments or diminishes each note's interval from the axis

- GIVEN a Sequence with active steps at various notes
- WHEN scaleIntervals is applied
- THEN each active step's note becomes `a + 2(n - a)` (augment) or `a + (n - a)/2` truncated toward the axis (diminish), where `a` is the note of the first active step
- AND sequence size and inactive steps are unchanged

#### Scenario: Palindrome mirrors the sequence with a non-shared pivot

- GIVEN a Sequence of `n` steps
- WHEN palindrome is applied
- THEN the result has `clamp(2n, 4, 64)` steps
- AND for `i < n` the result step equals input step `i`, and for `i >= n` it equals the wrap-safe mirrored input step, with no step shared between the forward and backward halves

### Requirement: Transform Bounds

Transpose MUST clamp the requested offset — once, against the minimum and maximum note among active steps — so that applying the (possibly reduced) offset uniformly to every active step never pushes any of them outside the valid MIDI note range (0-127). Individual notes MUST NOT be clamped independently of one another, since that would compress or expand the intervals between notes. Stretch and compress MUST clamp resulting sequence length to [4, 64] steps. Invert and scaleIntervals MUST restore the valid MIDI note range (0-127), when mirroring/scaling pushes notes outside it, using a single uniform corrective offset applied to every active note — never per-note clamping. If scaleIntervals' augmented span exceeds 127 with no uniform offset able to restore it, scaleIntervals MUST return an unmodified copy of the input rather than a partial or per-note-clamped result. Palindrome MUST clamp its output size to `clamp(2n, 4, 64)` steps, matching stretch/compress's length-bounding precedent.
(Previously: covered only transpose's offset clamp and stretch/compress's length clamp; now also covers invert's and scaleIntervals' uniform-offset correction, scaleIntervals' no-op fallback, and palindrome's length clamp.)

#### Scenario: Transpose clamps the offset, preserving intervals

- GIVEN active steps at notes 100 and 125, and a requested transpose offset of +12
- WHEN transpose is applied
- THEN the offset actually used is reduced to +2 (the largest offset that keeps the highest active note, 125, at or below 127)
- AND the resulting notes are 102 and 127 — the original 25-semitone interval between them is preserved

#### Scenario: Stretch respects the upper bound

- GIVEN a Sequence of 60 steps
- WHEN stretch is applied with a factor that would exceed 64 steps
- THEN the result is clamped to 64 steps

#### Scenario: Compress respects the lower bound

- GIVEN a Sequence of 6 steps
- WHEN compress is applied with a factor that would go below 4 steps
- THEN the result is clamped to 4 steps

#### Scenario: Invert applies one uniform corrective offset when mirroring exceeds range

- GIVEN active steps whose mirrored notes fall outside [0, 127]
- WHEN invert is applied
- THEN a single offset is added to every active step so the out-of-range mirrored note lands exactly at the boundary
- AND no active step is clamped independently of the others

#### Scenario: scaleIntervals no-ops when the augmented span cannot be corrected

- GIVEN active steps whose augmented interval span exceeds 127 with no single uniform offset able to fit it in [0, 127]
- WHEN scaleIntervals is applied with the augment factor
- THEN the result is an unmodified copy of the input
- AND no note is clamped individually

#### Scenario: Palindrome respects the [4, 64] length bound like stretch/compress

- GIVEN a Sequence of 40 steps
- WHEN palindrome is applied
- THEN the result is clamped to 64 steps, since `2 x 40 = 80` exceeds 64

#### Scenario: Palindrome's output size for a 16-step sequence stays within the existing dispatch-size set

- GIVEN a Sequence of 16 steps
- WHEN palindrome is applied
- THEN the result has exactly 32 steps, a value already covered by the existing dispatch-coverage assertion set `{8, 16, 32}`
