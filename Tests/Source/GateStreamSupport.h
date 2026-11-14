/*
  ==============================================================================

   GateStreamSupport - shared test helper for the step-gate SequencePlayer
   suites. Header-only so no project registration is needed.

  ==============================================================================
*/

#pragma once

#include <algorithm>
#include <vector>

#include "playback/SequencePlayer.h"
#include "playback/StepEvent.h"
#include "playback/StepEventBuffer.h"

namespace
{
    // Runs `total` samples through the player in blocks of `block` and returns
    // every event with its ABSOLUTE sample (block start + offset). `ok` turns
    // false if any block's offsets decrease or leave [0, numSamples).
    std::vector<berlin::StepEvent> drain (berlin::SequencePlayer& player, long long& position,
                                          int total, int block, bool& ok)
    {
        std::vector<berlin::StepEvent> events;
        berlin::StepEventBuffer buffer;

        for (int done = 0; done < total;)
        {
            const int n = std::min (block, total - done);
            player.process (n, buffer);

            int previous = 0;

            for (int i = 0; i < buffer.size(); ++i)
            {
                auto e = buffer[i];

                if (e.sampleOffset < previous || e.sampleOffset >= n)
                    ok = false;

                previous = e.sampleOffset;
                e.sampleOffset += static_cast<int> (position);
                events.push_back (e);
            }

            position += n;
            done += n;
        }

        return events;
    }
}
