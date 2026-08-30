/*
  ==============================================================================

   Transport - a JUCE-free, sample-accurate step clock
   (playback-transport-clock spec).

   Converts a fixed bpm/stepsPerBeat into samplesPerStep from the audio
   device's sample rate, and reports step boundaries crossed within each
   audio callback using drift-free absolute-position arithmetic: boundary k
   lands at llround(k * samplesPerStep), never accumulated by repeated
   addition of a rounded integer, so cumulative error cannot grow across
   irregular block sizes. Sequence-length-agnostic: counts absolute step
   boundaries only, with no concept of a loop length, step position, or
   playhead - that belongs entirely to SequencePlayer (step-event-scheduling
   spec, Phase 4). No runtime bpm mutation API exists (spec.md's "No runtime
   BPM mutation API" requirement) - bpm is fixed at construction.

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

    // --- block protocol: pure queries, then one commit. Allocation-free, noexcept. ---
    int          countBoundaries (int numSamples) const noexcept;   // 0 if stopped or !isPrepared()
    StepBoundary getBoundary (int index) const noexcept;            // 0 <= index < countBoundaries()
    void         advance (int numSamples) noexcept;                 // no-op when stopped

private:
    double    samplesPerStep { 0.0 };   // 0 until prepare()
    double    bpm;
    int       stepsPerBeat;
    long long position { 0 };
    long long nextStepCounter { 0 };
    bool      running { false };
};

} // namespace berlin
