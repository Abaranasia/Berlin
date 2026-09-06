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
    Sequence sequence;               // AUDIO-THREAD-EXCLUSIVE after construction
    Sequence pendingSequence;        // staging slot; message thread only while !sequencePending
    std::atomic<bool> sequencePending { false };
    Transport transport;
    int pendingNote { -1 };    // -1 = nothing sounding
    int pendingStep { 0 };
    std::atomic<int> playhead { 0 };
};

} // namespace berlin
