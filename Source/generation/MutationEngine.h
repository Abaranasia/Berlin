/*
  ==============================================================================

   MutationEngine - stateless mutation transforms for manual Mutate
   (evolution-mutation-engine spec, Manual Mutate, Slice 1).

   Depends on Sequence (Source/core/Sequence.h) and DeterministicRandom
   (Source/generation/DeterministicRandom.h) only - juce_core, not JUCE-free
   (design.md correction of the proposal's wording). Every transform has the
   uniform signature `Sequence (*) (const Sequence&, DeterministicRandom&)`
   (design.md Decision 1), even the RNG-free ones, so applyRandomTransform
   can dispatch through a plain function-pointer table. Every transform is
   TOTAL: degenerate input (size 0/1, zero active steps, clamp collapse)
   returns a well-formed Sequence, never crashes.

  ==============================================================================
*/

#pragma once

#include "../core/Sequence.h"
#include "DeterministicRandom.h"

#include <juce_core/juce_core.h>

namespace berlin::MutationEngine
{

using Transform = Sequence (*) (const Sequence&, DeterministicRandom&);

// +-12 semitones, offset clamped ONCE against the active-step min/max
// (design.md Decision 7) - never per-note, which would compress/expand
// intervals. Inactive steps are left untouched.
Sequence transpose (const Sequence& input, DeterministicRandom& random);

// No draw: reverses step order in full (active + note). An involution.
Sequence reverse (const Sequence& input, DeterministicRandom& random);

// Shifts by k in [1, size-1]; a no-op (returns a copy) for size < 2.
Sequence rotate (const Sequence& input, DeterministicRandom& random);

// Re-voices one active step with a pitch copied from another active step
// (design.md Decision 6 - never a fresh scale/range draw, never note = 0).
// No-op below two active steps.
Sequence changeNote (const Sequence& input, DeterministicRandom& random);

// Activates one inactive step, with a pitch copied from an active step.
// No-op with zero active steps (nothing to copy) or zero inactive steps
// (nothing to activate).
Sequence addNote (const Sequence& input, DeterministicRandom& random);

// Deactivates one active step. No-op with zero active steps.
Sequence removeNote (const Sequence& input, DeterministicRandom& random);

// x2 duplicate each step, clamped to [4, 64] steps (design.md Decision 8).
Sequence stretch (const Sequence& input, DeterministicRandom& random);

// /2 keep every other step, clamped to [4, 64] steps (design.md Decision 8).
Sequence compress (const Sequence& input, DeterministicRandom& random);

// Mirrors active notes about an axis: n' = 2a - n, where a is the note of the
// FIRST active step. Inactive steps untouched, size unchanged. No draw. Range
// restored by ONE uniform offset (never per-note). An involution when no
// corrective offset applies. No-op copy with zero active steps.
Sequence invert (const Sequence& input, DeterministicRandom& random);

// Augment/diminish as PITCH-INTERVAL scaling about the same axis a:
// augment n' = a + 2(n - a), diminish n' = a + (n - a) / 2 (C++ truncation,
// toward the axis). Exactly one draw (nextInt(2)), always, before any guard -
// same invariant as transpose. No-op copy with zero active steps OR when the
// scaled span exceeds 127, where no single uniform shift fits.
Sequence scaleIntervals (const Sequence& input, DeterministicRandom& random);

// Forward then backward mirror, pivot NOT shared: result[i] = input[j] with
// j = (i < n) ? i : 2n - 1 - i, wrapped via ((j % n) + n) % n. No draw.
// targetSize = std::clamp(n * 2, 4, 64), stretch/compress's precedent.
// n == 0 -> early-return copy (the modulo would be UB).
Sequence palindrome (const Sequence& input, DeterministicRandom& random);

// Exactly ONE selection draw (design.md Decision 2, mirrors RhythmGenerator's
// "one draw regardless of density" invariant), then delegates to the chosen
// transform.
Sequence applyRandomTransform (const Sequence& input, DeterministicRandom& random);

// splitmix64 finalisation over (baseSeed, generation) - plain addition would
// collide, e.g. (seed+1, gen 2) == (seed+2, gen 1); this does not (design.md).
juce::int64 mutationSeed (juce::int64 baseSeed, int generation) noexcept;

} // namespace berlin::MutationEngine
