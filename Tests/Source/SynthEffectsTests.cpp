/*
  ==============================================================================

   SynthEffects tests (internal-synth-output spec, Phase 8, first-ever
   coverage). juce-app-testing skill: silent-input, feedback-stability, and
   clamp/NaN coverage are non-negotiable for any DSP with internal state
   feeding back on itself.

   No direct getters exist on SynthEffects (mirrors SynthVoice/SynthEngine's
   own black-box-via-process() testing convention) - every assertion below
   observes behaviour through process() on a juce::dsp::AudioBlock view of a
   real juce::AudioBuffer<float>, never internal state.

  ==============================================================================
*/

#include <cmath>
#include <functional>
#include <limits>
#include <vector>

#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>

#include "synth/SynthEffects.h"
#include "synth/SynthPatch.h"

namespace
{
    juce::dsp::ProcessSpec makeSpec (double sampleRate, int blockSize, int numChannels = 2)
    {
        return { sampleRate, static_cast<juce::uint32> (blockSize), static_cast<juce::uint32> (numChannels) };
    }

    // Runs `fx` over `totalSamples` worth of input (fed via `fill`, one value
    // per absolute sample index) in fixed-size chunks of `blockSize`, and
    // returns channel 0's output samples in order. Mirrors SynthVoice/
    // SynthEngineTests' block-at-a-time render() pattern.
    std::vector<float> processInChunks (berlin::SynthEffects& fx, int totalSamples, int blockSize,
                                         const std::function<float (int)>& fill)
    {
        std::vector<float> output;
        output.reserve (static_cast<size_t> (totalSamples));

        int cursor = 0;
        while (cursor < totalSamples)
        {
            const int chunk = juce::jmin (blockSize, totalSamples - cursor);

            juce::AudioBuffer<float> buffer (2, chunk);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < chunk; ++i)
                    buffer.setSample (ch, i, fill (cursor + i));

            juce::dsp::AudioBlock<float> block (buffer);
            fx.process (block);

            for (int i = 0; i < chunk; ++i)
                output.push_back (buffer.getSample (0, i));

            cursor += chunk;
        }

        return output;
    }
}

class SynthEffectsTests final : public juce::UnitTest
{
public:
    SynthEffectsTests() : juce::UnitTest ("SynthEffects", "Berlin") {}

    void runTest() override
    {
        constexpr double sampleRate = 48000.0;
        constexpr int    blockSize  = 512;

        beginTest ("silent input stays at exact silence indefinitely (no uninitialized/denormal build-up)");
        {
            berlin::SynthEffects fx;
            fx.prepare (makeSpec (sampleRate, blockSize), berlin::kDefaultPatch);

            const auto output = processInChunks (fx, blockSize * 20, blockSize, [] (int) { return 0.0f; });

            for (float sample : output)
                expectEquals (sample, 0.0f);
        }

        beginTest ("driving every setter to extremes (incl. NaN) keeps output finite, no jassert trip");
        {
            berlin::SynthEffects fx;
            fx.prepare (makeSpec (sampleRate, blockSize), berlin::kDefaultPatch);

            fx.setDelayTimeSeconds (1.0e9f);
            fx.setDelayFeedback (1.0e9f);
            fx.setDelayMix (1.0e9f);
            fx.setReverbRoomSize (1.0e9f);
            fx.setReverbDamping (1.0e9f);
            fx.setReverbWetLevel (1.0e9f);
            fx.setReverbDryLevel (1.0e9f);

            const auto extremeHigh = processInChunks (fx, blockSize * 4, blockSize,
                [] (int i) { return 0.2f * std::sin (0.1f * static_cast<float> (i)); });

            for (float sample : extremeHigh)
                expect (std::isfinite (sample));

            fx.setDelayTimeSeconds (-1.0e9f);
            fx.setDelayFeedback (-1.0e9f);
            fx.setDelayMix (-1.0e9f);
            fx.setReverbRoomSize (-1.0e9f);
            fx.setReverbDamping (-1.0e9f);
            fx.setReverbWetLevel (-1.0e9f);
            fx.setReverbDryLevel (-1.0e9f);

            const auto extremeLow = processInChunks (fx, blockSize * 4, blockSize,
                [] (int i) { return 0.2f * std::sin (0.1f * static_cast<float> (i)); });

            for (float sample : extremeLow)
                expect (std::isfinite (sample));

            // NaN reaching a message-thread setter must clamp to the bound's
            // low end (clampParameter's documented contract, SynthPatch.h),
            // never propagate NaN into the signal path.
            const float nan = std::numeric_limits<float>::quiet_NaN();
            fx.setDelayTimeSeconds (nan);
            fx.setDelayFeedback (nan);
            fx.setDelayMix (nan);
            fx.setReverbRoomSize (nan);
            fx.setReverbDamping (nan);
            fx.setReverbWetLevel (nan);
            fx.setReverbDryLevel (nan);

            const auto afterNan = processInChunks (fx, blockSize * 4, blockSize,
                [] (int i) { return 0.2f * std::sin (0.1f * static_cast<float> (i)); });

            for (float sample : afterNan)
                expect (std::isfinite (sample), "NaN setter input reached the audio output");
        }

        beginTest ("delay time reaches its target within a settling window (one-pole glide), "
                   "verified via impulse-echo position");
        {
            berlin::SynthEffects fx;
            fx.prepare (makeSpec (sampleRate, blockSize), berlin::kDefaultPatch);

            // Isolate a single, unambiguous echo: no feedback repeats, fully wet,
            // reverb passthrough (wet=0/dry=1) so it cannot smear the echo.
            fx.setDelayFeedback (0.0f);
            fx.setDelayMix (1.0f);
            fx.setReverbWetLevel (0.0f);
            fx.setReverbDryLevel (1.0f);

            constexpr float targetSeconds = 0.05f;
            fx.setDelayTimeSeconds (targetSeconds);

            // Let the one-pole glide settle: tau=0.1s, so 2.0s (20*tau) leaves
            // exp(-20) ~ 2e-9 of the initial gap remaining - effectively
            // exact, well beyond any reasonable UI-latency expectation for a
            // slider drag or tempo-synced recompute.
            const int settleSamples = static_cast<int> (2.0 * sampleRate);
            processInChunks (fx, settleSamples, blockSize, [] (int) { return 0.0f; });

            // Impulse at sample 0, then enough silence to see the echo land.
            const int captureSamples = static_cast<int> (targetSeconds * sampleRate) + blockSize * 4;
            const auto output = processInChunks (fx, captureSamples, blockSize,
                [] (int i) { return i == 0 ? 1.0f : 0.0f; });

            int peakIndex = 0;
            float peakValue = 0.0f;
            for (int i = 1; i < static_cast<int> (output.size()); ++i)   // skip index 0 (the impulse itself)
            {
                if (std::abs (output[static_cast<size_t> (i)]) > peakValue)
                {
                    peakValue = std::abs (output[static_cast<size_t> (i)]);
                    peakIndex = i;
                }
            }

            const int expectedSamples = static_cast<int> (std::round (targetSeconds * sampleRate));
            expect (std::abs (peakIndex - expectedSamples) <= 4,
                    "echo landed at " + juce::String (peakIndex) + ", expected near " + juce::String (expectedSamples));
        }

        beginTest ("a large delay-time jump mid-stream produces no sample-to-sample discontinuity "
                   "beyond the steady-state baseline (tail continuity)");
        {
            berlin::SynthEffects fx;
            fx.prepare (makeSpec (sampleRate, blockSize), berlin::kDefaultPatch);
            fx.setDelayFeedback (0.3f);
            fx.setDelayMix (0.5f);

            constexpr float sineHz = 220.0f;
            auto sine = [=] (int i)
            {
                return 0.3f * std::sin (juce::MathConstants<float>::twoPi * sineHz * static_cast<float> (i) / static_cast<float> (sampleRate));
            };

            // Warm up well past the initial delay-time glide/reverb settle.
            processInChunks (fx, static_cast<int> (0.5 * sampleRate), blockSize, sine);

            const int baselineOffset = static_cast<int> (0.5 * sampleRate);
            const auto baseline = processInChunks (fx, blockSize * 2, blockSize,
                [&] (int i) { return sine (baselineOffset + i); });

            float baselineMaxDerivative = 0.0f;
            for (size_t i = 1; i < baseline.size(); ++i)
                baselineMaxDerivative = juce::jmax (baselineMaxDerivative, std::abs (baseline[i] - baseline[i - 1]));

            // Large jump: 0.3s (default) -> near the opposite end of the useful range.
            fx.setDelayTimeSeconds (2.5f);

            const int jumpOffset = baselineOffset + blockSize * 2;
            const auto jumpWindow = processInChunks (fx, blockSize * 2, blockSize,
                [&] (int i) { return sine (jumpOffset + i); });

            float jumpMaxDerivative = 0.0f;
            for (size_t i = 1; i < jumpWindow.size(); ++i)
                jumpMaxDerivative = juce::jmax (jumpMaxDerivative, std::abs (jumpWindow[i] - jumpWindow[i - 1]));

            // Generous multiple: a genuine click/discontinuity spikes an order of
            // magnitude or more beyond a sine's own slope (a near-instantaneous jump
            // in sample value); the one-pole glide's continuous read-pointer motion
            // instead produces a bounded, smooth FM-like artefact - same order of
            // magnitude as steady-state, not a multiple-orders-of-magnitude spike.
            expect (jumpMaxDerivative <= 10.0f * baselineMaxDerivative + 0.02f,
                    "baseline=" + juce::String (baselineMaxDerivative) + " jump=" + juce::String (jumpMaxDerivative));
        }

        beginTest ("a large reverb-parameter jump mid-stream produces no sample-to-sample discontinuity "
                   "beyond the steady-state baseline, across several sine phase offsets "
                   "(followup-fixes review investigation - regression lock, not a fix)");
        {
            // followup-fixes review WARNING finding claimed reverb params
            // (unlike delay time/feedback/mix) have no smoothing and snap
            // instantly - investigated and found to be a FALSE POSITIVE:
            // juce::Reverb::setParameters() (juce_audio_basics/utilities/
            // juce_Reverb.h) stores roomSize/damping/wetLevel/dryLevel into
            // internal SmoothedValue<float> members with a 10ms smoothTime
            // (set in setSampleRate()) - processStereo()'s getNextValue()
            // calls already ramp every one of these 4 fields sample-by-sample
            // regardless of how abruptly our own setParameters() call is
            // made. This test proves it empirically (no production change
            // was needed): even an extreme jump on all 4 fields at once
            // (roomSize/damping 0.1->0.9, full dry->wet crossfade) produces
            // no discontinuity beyond the steady-state baseline.
            //
            // The transition itself is what needs checking (JUCE's internal
            // ramp means later samples are unaffected, but the very first
            // post-jump sample is exactly where a naive caller-side snap
            // WOULD show up if JUCE's own smoothing didn't already cover
            // it) - and whether that first sample happens to land on a large
            // mismatch between the pre-jump (dry) and post-jump (wet) signal
            // depends on the driving sine's instantaneous phase, so this
            // repeats the probe at several phase offsets across one full
            // cycle and keeps the worst case - avoiding a flaky pass/fail
            // from one lucky/unlucky phase.
            constexpr float sineHz = 220.0f;
            auto sine = [=] (int i)
            {
                return 0.3f * std::sin (juce::MathConstants<float>::twoPi * sineHz * static_cast<float> (i) / static_cast<float> (sampleRate));
            };

            const int samplesPerCycle = static_cast<int> (sampleRate / sineHz);
            constexpr int numTrials = 8;

            float worstJumpDerivative = 0.0f;
            float worstBaselineDerivative = 0.0f;

            for (int trial = 0; trial < numTrials; ++trial)
            {
                berlin::SynthEffects fx;
                fx.prepare (makeSpec (sampleRate, blockSize), berlin::kDefaultPatch);
                fx.setDelayMix (0.0f);   // isolate reverb: no delay contribution to the signal

                // Start fully dry (wetLevel=0/dryLevel=1) so the reverb tail
                // is silent-but-developing underneath a dry passthrough -
                // jumping straight to a fully-wet mix (below) crossfades
                // onto a reverb tail that can differ substantially from the
                // instantaneous dry sample.
                fx.setReverbWetLevel (0.0f);
                fx.setReverbDryLevel (1.0f);
                fx.setReverbRoomSize (0.1f);
                fx.setReverbDamping (0.1f);

                const int phase = trial * samplesPerCycle / numTrials;
                auto phasedSine = [&] (int i) { return sine (i + phase); };

                // Warm up well past the initial reverb-parameter ramp settle
                // (the ramp itself completes within one process() call/block
                // - see applyParameters() - no separate time constant like
                // the delay glide's tau).
                processInChunks (fx, static_cast<int> (0.5 * sampleRate), blockSize, phasedSine);

                const int baselineOffset = static_cast<int> (0.5 * sampleRate);
                const auto baseline = processInChunks (fx, blockSize * 2, blockSize,
                    [&] (int i) { return phasedSine (baselineOffset + i); });

                float baselineMaxDerivative = 0.0f;
                for (size_t i = 1; i < baseline.size(); ++i)
                    baselineMaxDerivative = juce::jmax (baselineMaxDerivative, std::abs (baseline[i] - baseline[i - 1]));

                // Large jump on all 4 reverb fields at once: roomSize/damping
                // 0.1 -> 0.9, and wetLevel/dryLevel crossfade dry -> fully wet.
                fx.setReverbRoomSize (0.9f);
                fx.setReverbDamping (0.9f);
                fx.setReverbWetLevel (1.0f);
                fx.setReverbDryLevel (0.0f);

                const int jumpOffset = baselineOffset + blockSize * 2;
                const auto jumpWindow = processInChunks (fx, blockSize * 2, blockSize,
                    [&] (int i) { return phasedSine (jumpOffset + i); });

                float jumpMaxDerivative = std::abs (jumpWindow.front() - baseline.back());
                for (size_t i = 1; i < jumpWindow.size(); ++i)
                    jumpMaxDerivative = juce::jmax (jumpMaxDerivative, std::abs (jumpWindow[i] - jumpWindow[i - 1]));

                worstJumpDerivative     = juce::jmax (worstJumpDerivative, jumpMaxDerivative);
                worstBaselineDerivative = juce::jmax (worstBaselineDerivative, baselineMaxDerivative);
            }

            expect (worstJumpDerivative <= 10.0f * worstBaselineDerivative + 0.02f,
                    "baseline=" + juce::String (worstBaselineDerivative) + " jump=" + juce::String (worstJumpDerivative));
        }

        beginTest ("sustained input at high feedback stays bounded across hundreds of blocks "
                   "(feedback-stability, per juce-app-testing skill)");
        {
            berlin::SynthEffects fx;
            fx.prepare (makeSpec (sampleRate, blockSize), berlin::kDefaultPatch);

            constexpr float inputAmplitude = 0.2f;
            fx.setDelayFeedback (berlin::kMaxDelayFeedback);   // 0.95: worst-case (but still < 1.0, provably stable)
            fx.setDelayMix (1.0f);
            fx.setReverbWetLevel (0.0f);   // isolate the delay's own feedback loop from reverb's energy
            fx.setReverbDryLevel (1.0f);

            constexpr int numBlocks = 400;   // "hundreds of blocks", per skill

            // Theoretical bound for a stable IIR comb (|feedback| < 1) fed a
            // bounded input |x[n]| <= A: |v[n]| <= A / (1 - feedback) for all n
            // (proof by induction on v[n] = x[n] + feedback * v[n-D]). A 2x
            // safety margin absorbs the dry/wet mix combination and any
            // floating-point rounding, without masking a genuine divergence.
            const float theoreticalBound = inputAmplitude / (1.0f - berlin::kMaxDelayFeedback) * 2.0f;

            float peak = 0.0f;

            for (int b = 0; b < numBlocks; ++b)
            {
                juce::AudioBuffer<float> buffer (2, blockSize);
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < blockSize; ++i)
                        buffer.setSample (ch, i, inputAmplitude);   // sustained DC, worst case for a feedback loop

                juce::dsp::AudioBlock<float> block (buffer);
                fx.process (block);

                for (int ch = 0; ch < 2; ++ch)
                {
                    for (int i = 0; i < blockSize; ++i)
                    {
                        const float sample = buffer.getSample (ch, i);
                        expect (std::isfinite (sample));
                        peak = juce::jmax (peak, std::abs (sample));
                    }
                }
            }

            expect (peak <= theoreticalBound,
                    "peak=" + juce::String (peak) + " bound=" + juce::String (theoreticalBound));

            // Sanity: the loop is genuinely accumulating energy (not trivially
            // silent), so the bound check above is meaningful, not vacuous.
            expect (peak > inputAmplitude);
        }
    }
};

static SynthEffectsTests synthEffectsTests;
