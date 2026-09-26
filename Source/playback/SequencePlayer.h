/*
  ==============================================================================

   SequencePlayer - drives a Sequence off a Transport's step boundaries,
   emitting ordered StepEvents (playback-transport-clock spec,
   step-event-scheduling).

   JUCE-free: standard library only. Owns both the Sequence and the Transport
   BY VALUE (design.md's rationale: a reference-holding version makes
   correctness depend on member declaration order, silently, on the audio
   thread; by-value gives one owner, no aliasing). `sequence` is
   AUDIO-THREAD-EXCLUSIVE after construction: it is replaced only via the
   publish/adopt handoff below (design.md Decision 1) - the message thread
   never touches it directly, only `pendingSequence`. Combines Transport's
   monotonic absolute step counter with the Sequence's length to compute the
   loop wrap (stepIndex = stepCounter % sequence.size()) - Transport itself
   is sequence-length-agnostic and has no wrap concept.

   process() is the audio-thread, RT-safe entry point: clears the sink,
   then - BEFORE the boundary loop - checks whether a new Sequence is
   pending; if so it adopts it as a single atomic step (note-off for any
   sounding note first, `sequence.swap(pendingSequence)`, transport/playhead
   reset), so the rest of THIS SAME process() call already reads from the
   newly adopted Sequence. For each Transport boundary in the block it then
   pushes the pending note-off (if any) BEFORE computing the new stepIndex
   and pushing that step's note-on (if active) - this ordering IS the
   note-off-before-note-on contract. The atomic playhead is updated every
   iteration and is the only cross-thread observability seam besides the
   publish/adopt handoff itself. transport.advance() commits once, after the
   whole loop. Definitions live in SequencePlayer.cpp so a forgotten <FILE>
   registration in either .jucer project fails loudly as an
   unresolved-external link error, per design.md's ".h/.cpp for types with
   out-of-line definitions" decision.

   The std::atomic<int> playhead member makes SequencePlayer non-copyable and
   non-movable by construction - this is intentional (design.md): it forces
   in-place initialiser-list construction in MainComponent, so the compiler
   enforces "fully built before setAudioChannels" rather than a comment.

   loopCount (auto-evolution spec, step-event-scheduling "Loop Completion
   Counter") is a second observability seam mirroring playhead exactly:
   audio-thread relaxed store inside the boundary loop of process(), any-
   thread relaxed load via getLoopCount(). It increments exactly once per
   genuine wrap (stepIndex == 0 && previousStep != 0) and is monotonic -
   never reset or decremented, including by publish/adopt, since the adopt
   path already zeroes playhead before the boundary loop runs (so the first
   post-adopt boundary reads previousStep == 0 and is correctly excluded).
   isPublishPending() is a third, read-only seam letting a caller check
   busy state (sequencePending) before attempting a publish-dependent
   action, rather than only discovering it from that action's own
   rejection.

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

    // MESSAGE THREAD ONLY. Swaps `incoming` into the staging slot and
    // publishes it. Returns false iff a previous publish is still unadopted;
    // then nothing is published and `incoming` is left unmodified. The audio
    // thread adopts at the top of the next process(): it emits a note-off at
    // offset 0 for any sounding note, swaps the sequence in (O(1), no
    // alloc/free/lock), and restarts the pattern from step 0.
    bool publishSequence (Sequence& incoming) noexcept;

    // MESSAGE THREAD ONLY. Relaxed store into pendingBpm; adopted at the top
    // of the NEXT process() call, AFTER the pending-sequence adopt block and
    // BEFORE countBoundaries (design.md Decision 4) - a bpm change between
    // countBoundaries() and getBoundary()/advance() would desync the loop,
    // so the mutation is confined to this single point. The atomic lives
    // here, not on Transport (design.md Decision 1): Transport is taken BY
    // VALUE by this class's constructor, so an atomic member there would
    // delete its copy/move ctors.
    void setBpm (double newBpm) noexcept;

    void process (int numSamples, StepEventBuffer& out) noexcept;   // AUDIO THREAD, RT-safe

    // Clears `out`, then pushes at most one StepEvent { 0, pendingStep, pendingNote, false }
    // if a note is currently sounding, and clears the pending-note state. Idempotent: a
    // second call with no processing in between emits nothing and returns false. NOT for
    // the audio callback (midi-output-routing spec); the single production call site is
    // MainComponent::releaseResources(). MUST be called BEFORE reset(), which discards
    // pendingNote; stop() preserves it, so flush-after-stop still emits. Returns true iff
    // it emitted.
    bool flushPendingNoteOff (StepEventBuffer& out) noexcept;

    int getPlayheadStep() const noexcept;   // atomic load, any thread; one of three observability seams

    int  getLoopCount()     const noexcept;   // atomic load, any thread; monotonic, +1 per genuine wrap
    bool isPublishPending() const noexcept;   // atomic load, any thread; true while a publish awaits adoption

private:
    Sequence sequence;               // AUDIO-THREAD-EXCLUSIVE after construction
    Sequence pendingSequence;        // staging slot; message thread only while !sequencePending
    std::atomic<bool> sequencePending { false };
    Transport transport;
    int pendingNote { -1 };    // -1 = nothing sounding
    int pendingStep { 0 };
    std::atomic<int> playhead { 0 };
    std::atomic<int> loopCount { 0 };
    std::atomic<double> pendingBpm;   // initialised in the constructor body from transport.getBpm()
};

} // namespace berlin
