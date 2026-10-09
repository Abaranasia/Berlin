/*
  ==============================================================================

   Transport - out-of-line definitions (playback-transport spec, design.md
   Decisions 2/3: origin-rebased boundary grid + phase-preserving setBpm).

  ==============================================================================
*/

#include "Transport.h"

#include <algorithm>
#include <cmath>

namespace berlin
{

Transport::Transport (double bpmIn, int stepsPerBeatIn) noexcept
    : bpm (bpmIn), stepsPerBeat (stepsPerBeatIn)
{
}

void Transport::prepare (double sampleRateIn) noexcept
{
    sampleRate = sampleRateIn;

    if (bpm <= 0.0 || stepsPerBeat <= 0)
        samplesPerStep = 0.0;
    else
        samplesPerStep = sampleRate * 60.0 / (bpm * static_cast<double> (stepsPerBeat));

    reset();
}

void Transport::reset() noexcept
{
    position = 0;
    nextStepCounter = 0;
    originSample = 0.0;
    originStep = 0;
}

void Transport::start() noexcept
{
    running = true;
}

void Transport::stop() noexcept
{
    running = false;
}

bool Transport::isRunning() const noexcept
{
    return running;
}

bool Transport::isPrepared() const noexcept
{
    return samplesPerStep >= 1.0;
}

double Transport::getSamplesPerStep() const noexcept
{
    return samplesPerStep;
}

long long Transport::getSamplePosition() const noexcept
{
    return position;
}

long long Transport::getNextStepCounter() const noexcept
{
    return nextStepCounter;
}

double Transport::getBpm() const noexcept
{
    return bpm;
}

void Transport::setBpm (double newBpm) noexcept
{
    if (newBpm <= 0.0 || newBpm == bpm)
        return;   // cheapest path: called every block by SequencePlayer (design.md Decision 4)

    const double newSamplesPerStep = (sampleRate > 0.0 && stepsPerBeat > 0)
        ? sampleRate * 60.0 / (newBpm * static_cast<double> (stepsPerBeat))
        : 0.0;

    if (samplesPerStep <= 0.0)   // never prepared / degenerate: nothing to rebase, just adopt
    {
        bpm = newBpm;
        samplesPerStep = newSamplesPerStep;
        originSample = 0.0;
        originStep = 0;
        return;
    }

    // Fraction of the CURRENT step already elapsed, measured under the OLD tempo.
    const double prevBoundary = originSample
        + static_cast<double> (nextStepCounter - 1 - originStep) * samplesPerStep;
    double phase = (static_cast<double> (position) - prevBoundary) / samplesPerStep;
    phase = std::clamp (phase, 0.0, 1.0);

    originStep   = nextStepCounter - 1;                    // anchor = last EMITTED boundary
    originSample = static_cast<double> (position) - phase * newSamplesPerStep;
    samplesPerStep = newSamplesPerStep;
    bpm = newBpm;
}

long long Transport::boundarySampleFor (long long k) const noexcept
{
    return std::llround (originSample + static_cast<double> (k - originStep) * samplesPerStep);
}

int Transport::countBoundaries (int numSamples) const noexcept
{
    if (! running || ! isPrepared())
        return 0;

    long long count = 0;
    long long k = nextStepCounter;
    const long long blockEnd = position + numSamples;

    for (;;)
    {
        const long long boundarySample = boundarySampleFor (k);

        if (boundarySample >= blockEnd)
            break;

        ++count;
        ++k;
    }

    return static_cast<int> (count);
}

StepBoundary Transport::getBoundary (int index) const noexcept
{
    const long long k = nextStepCounter + index;
    const long long boundarySample = boundarySampleFor (k);

    StepBoundary boundary;
    boundary.sampleOffset = static_cast<int> (boundarySample - position);
    boundary.stepCounter = k;
    return boundary;
}

void Transport::advance (int numSamples) noexcept
{
    if (! running)
        return;

    if (isPrepared())
        nextStepCounter += countBoundaries (numSamples);

    position += numSamples;
}

} // namespace berlin
