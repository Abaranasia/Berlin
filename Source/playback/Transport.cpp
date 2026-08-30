/*
  ==============================================================================

   Transport - out-of-line definitions (playback-transport-clock spec).

  ==============================================================================
*/

#include "Transport.h"

#include <cmath>

namespace berlin
{

Transport::Transport (double bpmIn, int stepsPerBeatIn) noexcept
    : bpm (bpmIn), stepsPerBeat (stepsPerBeatIn)
{
}

void Transport::prepare (double sampleRate) noexcept
{
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

int Transport::countBoundaries (int numSamples) const noexcept
{
    if (! running || ! isPrepared())
        return 0;

    long long count = 0;
    long long k = nextStepCounter;
    const long long blockEnd = position + numSamples;

    for (;;)
    {
        const long long boundarySample = std::llround (static_cast<double> (k) * samplesPerStep);

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
    const long long boundarySample = std::llround (static_cast<double> (k) * samplesPerStep);

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
