/*
  ==============================================================================

   Transport - a JUCE-free, sample-accurate step clock
   (playback-transport-clock spec).

   Converts a bpm/stepsPerBeat into samplesPerStep from the audio device's
   sample rate, and reports step boundaries crossed within each audio
   callback using drift-free ORIGIN-RELATIVE arithmetic (playback-transport
   spec's "Runtime BPM Mutation With Origin-Rebased Boundary Math"
   requirement, design.md Decision 2): boundary k lands at
   llround(originSample + (k - originStep) * samplesPerStep) - the single
   boundarySampleFor() chokepoint - never accumulated by repeated addition of
   a rounded integer, so cumulative error cannot grow across irregular block
   sizes. Defaults (originSample=0, originStep=0) make this bit-identical to
   the pre-tempo-control absolute formula k*samplesPerStep whenever setBpm is
   never called - the existing TransportTests.cpp drift suites are the
   regression proof (tasks.md 2.3). Sequence-length-agnostic: counts absolute
   step boundaries only, with no concept of a loop length, step position, or
   playhead - that belongs entirely to SequencePlayer (step-event-scheduling
   spec, Phase 4).

   setBpm(double) (design.md Decision 3) is AUDIO-THREAD-ONLY and legal ONLY
   at block start, i.e. before countBoundaries()/getBoundary() are queried
   for that block (SequencePlayer::process is the only call site - see
   design.md Decision 4). It performs a PHASE-PRESERVING REBASE of the
   origin, never a naive substitution into k*samplesPerStep (which would
   retime already-reported boundary history): the fraction of the current
   step already elapsed under the OLD tempo is preserved and re-scaled by the
   NEW tempo, so the next boundary is guaranteed >= the current position -
   never in the past, never skipped, never doubled. This call-site contract
   is UNENFORCED at compile time (Transport is JUCE-free, no jassert
   available) - see design.md's residual-risk note.

   Block protocol: countBoundaries()/getBoundary() are pure const queries
   against the CURRENT position; advance() is the single commit, computed
   against the position BEFORE it moves. Definitions live in Transport.cpp so
   a forgotten <FILE> registration in either .jucer project fails loudly as
   an unresolved-external link error, per design.md's ".h/.cpp for types with
   out-of-line definitions" decision.

  ==============================================================================
*/

#pragma once

namespace berlin
{

struct StepBoundary
{
    int       sampleOffset = 0;   // samples from the START of the current block
    long long stepCounter  = 0;   // absolute step index this boundary lands on
};

class Transport
{
public:
    Transport (double bpm, int stepsPerBeat) noexcept;   // bpm <= 0 / stepsPerBeat <= 0 -> never prepared

    void prepare (double sampleRate) noexcept;   // samplesPerStep = sampleRate*60/(bpm*stepsPerBeat); resets
    void reset()  noexcept;                      // position 0, counter 0; preserves running state
    void start()  noexcept;
    void stop()   noexcept;

    bool      isRunning()  const noexcept;
    bool      isPrepared() const noexcept;       // samplesPerStep >= 1.0 (guards div-by-zero AND runaway counts)
    double    getSamplesPerStep()  const noexcept;
    long long getSamplePosition()  const noexcept;
    long long getNextStepCounter() const noexcept;
    double    getBpm() const noexcept;

    // AUDIO THREAD, block start ONLY (before countBoundaries()/getBoundary()
    // are queried for this block - see class doc). Cheap early-return no-op
    // when newBpm <= 0 or unchanged (steady-state cost: one compare).
    // Otherwise performs the phase-preserving origin rebase (design.md
    // Decision 3): recomputes samplesPerStep from the cached sample rate and
    // re-anchors the boundary grid so the in-flight step's remaining phase
    // is re-scaled by the new tempo. Allocation-, lock-, log-free.
    void setBpm (double newBpm) noexcept;

    // --- block protocol: pure queries, then one commit. Allocation-free, noexcept. ---
    int          countBoundaries (int numSamples) const noexcept;   // 0 if stopped or !isPrepared()
    StepBoundary getBoundary (int index) const noexcept;            // 0 <= index < countBoundaries()
    void         advance (int numSamples) noexcept;                 // no-op when stopped

private:
    // Single chokepoint (design.md Decision 2): boundary k lands at
    // llround(originSample + (k - originStep) * samplesPerStep). Defaults
    // (0.0, 0) reduce this to the original k*samplesPerStep formula.
    long long boundarySampleFor (long long k) const noexcept;

    double    samplesPerStep { 0.0 };   // 0 until prepare()
    double    bpm;
    int       stepsPerBeat;
    double    sampleRate { 0.0 };       // cached by prepare(); needed to recompute samplesPerStep in setBpm
    long long position { 0 };
    long long nextStepCounter { 0 };
    bool      running { false };

    double    originSample { 0.0 };     // origin anchor for boundarySampleFor; reset to 0 by reset()/prepare()
    long long originStep { 0 };
};

} // namespace berlin
