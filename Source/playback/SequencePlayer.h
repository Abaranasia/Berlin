/*
  ==============================================================================

   SequencePlayer - drives a Sequence off a Transport's step boundaries,
   emitting ordered StepEvents (playback-transport-clock spec,
   step-event-scheduling).

   JUCE-free: standard library only. Owns both the Sequence and the Transport
   BY VALUE (design.md's rationale: a reference-holding version makes
   correctness depend on member declaration order, silently, on the audio
   thread; by-value gives one owner, one immutable snapshot, no aliasing).
   Combines Transport's monotonic absolute step counter with the Sequence's
   length to compute the loop wrap (stepIndex = stepCounter % sequence.size())
   - Transport itself is sequence-length-agnostic and has no wrap concept.

   process() is the audio-thread, RT-safe entry point: clears the sink first,
   then for each Transport boundary in the block pushes the pending note-off
   (if any) BEFORE computing the new stepIndex and pushing that step's
   note-on (if active) - this ordering IS the note-off-before-note-on
   contract. The atomic playhead is updated every iteration and is the only
   cross-thread observability seam. transport.advance() commits once, after
   the whole loop. Definitions live in SequencePlayer.cpp so a forgotten
   <FILE> registration in either .jucer project fails loudly as an
   unresolved-external link error, per design.md's ".h/.cpp for types with
   out-of-line definitions" decision.

   The std::atomic<int> playhead member makes SequencePlayer non-copyable and
   non-movable by construction - this is intentional (design.md): it forces
   in-place initialiser-list construction in MainComponent, so the compiler
   enforces "fully built before setAudioChannels" rather than a comment.

  ==============================================================================
*/

#pragma once

#include <atomic>

#include "core/Sequence.h"
#include "playback/StepEventBuffer.h"
#include "playback/Transport.h"

namespace berlin
{

class SequencePlayer
{
public:
    SequencePlayer (Sequence sequenceToPlay, Transport transportToUse);

    void prepare (double sampleRate) noexcept;   // forwards + reset(); preserves running state
    void start() noexcept;
    void stop() noexcept;
    void reset() noexcept;

    void process (int numSamples, StepEventBuffer& out) noexcept;   // AUDIO THREAD, RT-safe

    // Clears `out`, then pushes at most one StepEvent { 0, pendingStep, pendingNote, false }
    // if a note is currently sounding, and clears the pending-note state. Idempotent: a
    // second call with no processing in between emits nothing and returns false. NOT for
    // the audio callback (midi-output-routing spec); the single production call site is
    // MainComponent::releaseResources(). MUST be called BEFORE reset(), which discards
    // pendingNote; stop() preserves it, so flush-after-stop still emits. Returns true iff
    // it emitted.
    bool flushPendingNoteOff (StepEventBuffer& out) noexcept;

    int getPlayheadStep() const noexcept;   // atomic load, any thread; the ONLY observability seam

private:
    const Sequence sequence;   // immutable after construction
    Transport transport;
    int pendingNote { -1 };    // -1 = nothing sounding
    int pendingStep { 0 };
    std::atomic<int> playhead { 0 };
};

} // namespace berlin
