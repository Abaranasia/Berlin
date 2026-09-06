/*
  ==============================================================================

   SynthVoice tests (internal-synth-voice spec, roadmap Phase 8 / internal-synth
   Phase 2). RED first: Source/synth/SynthPatch.h and Source/synth/SynthVoice.h
   do not exist yet, so this suite must fail to compile until Phase 2's
   production files are created.

   Covers: silence while idle; non-silence after noteOn; ADSR attack stage is
   non-decreasing; exact silence after release completes and stays silent;
   bounded output (gain-staging guard); each of the four waveforms produces a
   periodic, distinguishable signal; filter cutoff sweep brightens; filter
   resonance emphasizes energy near cutoff. No LFO here (Phase 4).

  ==============================================================================
*/

#include <array>
#include <cmath>
#include <limits>
#include <vector>

#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>

#include "synth/SynthPatch.h"
#include "synth/SynthVoice.h"

namespace
{
    juce::dsp::ProcessSpec makeSpec (double sampleRate, int blockSize, int numChannels = 2)
    {
        return { sampleRate, static_cast<juce::uint32> (blockSize), static_cast<juce::uint32> (numChannels) };
    }
}

class SynthVoiceTests final : public juce::UnitTest
{
public:
    SynthVoiceTests() : juce::UnitTest ("SynthVoice", "Berlin") {}

    void runTest() override
    {
        constexpr double sampleRate = 44100.0;

        beginTest ("clampParameter clamps extremes and passes through in-range values");
        {
            // In-range value passes through unchanged.
            expectEquals (berlin::clampParameter (1000.0f, berlin::kMinCutoffHz, berlin::kMaxCutoffHz), 1000.0f);

            // Extremes clamp to the documented bounds.
            expectEquals (berlin::clampParameter (1.0e9f, berlin::kMinCutoffHz, berlin::kMaxCutoffHz), berlin::kMaxCutoffHz);
            expectEquals (berlin::clampParameter (-100.0f, berlin::kMinCutoffHz, berlin::kMaxCutoffHz), berlin::kMinCutoffHz);

            // NaN-adjacent: +/-infinity, a value one float ULP inside the bound, and exactly-at-bound.
            expectEquals (berlin::clampParameter (std::numeric_limits<float>::infinity(), berlin::kMinResonance, berlin::kMaxResonance), berlin::kMaxResonance);
            expectEquals (berlin::clampParameter (-std::numeric_limits<float>::infinity(), berlin::kMinResonance, berlin::kMaxResonance), berlin::kMinResonance);
            expectEquals (berlin::clampParameter (berlin::kMinResonance, berlin::kMinResonance, berlin::kMaxResonance), berlin::kMinResonance);
            expectEquals (berlin::clampParameter (berlin::kMaxResonance, berlin::kMinResonance, berlin::kMaxResonance), berlin::kMaxResonance);

            // A second parameter's bounds, to triangulate against a different range.
            expectEquals (berlin::clampParameter (0.5f, berlin::kMinLfoDepth, berlin::kMaxLfoDepth), 0.5f);
            expectEquals (berlin::clampParameter (-5.0f, berlin::kMinLfoDepth, berlin::kMaxLfoDepth), berlin::kMinLfoDepth);
            expectEquals (berlin::clampParameter (5.0f, berlin::kMinLfoDepth, berlin::kMaxLfoDepth), berlin::kMaxLfoDepth);
        }

        beginTest ("freshly prepared, never-triggered voice renders exact silence");
        {
            berlin::SynthVoice voice;
            voice.prepare (makeSpec (sampleRate, 512), berlin::kDefaultPatch);

            std::vector<float> left (512, 1.0f), right (512, 1.0f);   // poison with non-zero
            voice.render (left.data(), right.data(), 512);

            for (int i = 0; i < 512; ++i)
            {
                expectEquals (left[(size_t) i], 0.0f);
                expectEquals (right[(size_t) i], 0.0f);
            }
        }

        beginTest ("note-on produces non-silent output");
        {
            berlin::SynthVoice voice;
            voice.prepare (makeSpec (sampleRate, 128), berlin::kDefaultPatch);
            voice.noteOn (440.0f);

            std::vector<float> left (128, 0.0f), right (128, 0.0f);
            voice.render (left.data(), right.data(), 128);

            bool anyNonZero = false;
            for (float sample : left)
                if (sample != 0.0f)
                    anyNonZero = true;

            expect (anyNonZero);
        }

        beginTest ("envelope rises monotonically (non-decreasing) through the attack stage");
        {
            berlin::SynthPatch patch = berlin::kDefaultPatch;
            patch.waveform  = berlin::Waveform::square;
            patch.cutoffHz  = 20000.0f;
            patch.resonance = 0.7071068f;
            patch.attack    = 0.05f;
            patch.decay     = 0.001f;
            patch.sustain   = 1.0f;

            berlin::SynthVoice voice;
            voice.prepare (makeSpec (sampleRate, 2048), patch);
            voice.noteOn (1000.0f);   // short period (~44 samples) so each window covers several cycles

            constexpr int windowSize = 100;
            constexpr int numWindows = 20;   // 2000 samples, well within the 2205-sample attack
            std::vector<float> peaks (static_cast<size_t> (numWindows), 0.0f);

            for (int w = 0; w < numWindows; ++w)
            {
                std::vector<float> left (windowSize, 0.0f), right (windowSize, 0.0f);
                voice.render (left.data(), right.data(), windowSize);

                float peak = 0.0f;
                for (float sample : left)
                    peak = juce::jmax (peak, std::abs (sample));

                peaks[(size_t) w] = peak;
            }

            for (int w = 1; w < numWindows; ++w)
                expect (peaks[(size_t) w] >= peaks[(size_t) (w - 1)] - 1.0e-4f);

            // Sanity: the envelope actually moved (not stuck at 0 for the whole window range).
            expect (peaks.back() > peaks.front());
        }

        beginTest ("note-off releases to exact silence and stays silent thereafter");
        {
            berlin::SynthPatch patch = berlin::kDefaultPatch;
            patch.attack  = 0.001f;
            patch.decay   = 0.001f;
            patch.sustain = 0.7f;
            patch.release = 0.05f;

            berlin::SynthVoice voice;
            voice.prepare (makeSpec (sampleRate, 4096), patch);
            voice.noteOn (440.0f);

            std::vector<float> warm (200, 0.0f), warmR (200, 0.0f);
            voice.render (warm.data(), warmR.data(), 200);   // reach sustain

            voice.noteOff();

            const int releaseSamples = (int) (patch.release * sampleRate) + 200;   // release duration + margin
            std::vector<float> tail (static_cast<size_t> (releaseSamples), 0.0f), tailR (static_cast<size_t> (releaseSamples), 0.0f);
            voice.render (tail.data(), tailR.data(), releaseSamples);

            std::vector<float> after (256, 1.0f), afterR (256, 1.0f);   // poison with non-zero
            voice.render (after.data(), afterR.data(), 256);

            for (float sample : after)
                expectEquals (sample, 0.0f);
        }

        beginTest ("output stays bounded for a sustained full-level note (gain-staging guard)");
        {
            berlin::SynthVoice voice;
            voice.prepare (makeSpec (sampleRate, 2048), berlin::kDefaultPatch);
            voice.noteOn (220.0f);

            std::vector<float> left (2048, 0.0f), right (2048, 0.0f);
            voice.render (left.data(), right.data(), 2048);

            for (float sample : left)
                expect (std::abs (sample) <= 1.5f);
        }

        beginTest ("each of the four waveforms produces a periodic, distinguishable signal");
        {
            constexpr float        pitchHz   = 100.0f;   // 44100 / 100 = 441 samples/period exactly
            static constexpr int   periodLen = 441;      // static: usable in a lambda's trailing return type
            static constexpr int   settleLen = periodLen;   // discard one period of filter transient

            auto capture = [&] (berlin::Waveform waveform) -> std::array<float, periodLen>
            {
                berlin::SynthPatch patch = berlin::kDefaultPatch;
                patch.waveform = waveform;
                patch.attack   = 0.0f;
                patch.decay    = 0.0f;
                patch.sustain  = 1.0f;

                // A 50%-duty pulse is byte-identical to the square generator - use an
                // asymmetric duty cycle so pulse is distinguishable from square, the way
                // PWM is actually used.
                if (waveform == berlin::Waveform::pulse)
                    patch.pulseWidth = 0.25f;

                berlin::SynthVoice voice;
                voice.prepare (makeSpec (sampleRate, periodLen * 3), patch);
                voice.noteOn (pitchHz);

                std::vector<float> discard (settleLen, 0.0f), discardR (settleLen, 0.0f);
                voice.render (discard.data(), discardR.data(), settleLen);

                std::array<float, periodLen> steady {};
                std::vector<float> right (periodLen, 0.0f);
                voice.render (steady.data(), right.data(), periodLen);
                return steady;
            };

            const auto saw      = capture (berlin::Waveform::saw);
            const auto square   = capture (berlin::Waveform::square);
            const auto triangle = capture (berlin::Waveform::triangle);
            const auto pulse    = capture (berlin::Waveform::pulse);

            // Periodicity: one more full cycle from a fresh voice must repeat exactly.
            {
                berlin::SynthPatch patch = berlin::kDefaultPatch;
                patch.waveform = berlin::Waveform::square;
                patch.attack = 0.0f; patch.decay = 0.0f; patch.sustain = 1.0f;

                berlin::SynthVoice voice;
                voice.prepare (makeSpec (sampleRate, periodLen * 4), patch);
                voice.noteOn (pitchHz);

                std::vector<float> discard (settleLen, 0.0f), discardR (settleLen, 0.0f);
                voice.render (discard.data(), discardR.data(), settleLen);

                std::vector<float> cycleA (periodLen, 0.0f), cycleAR (periodLen, 0.0f);
                voice.render (cycleA.data(), cycleAR.data(), periodLen);
                std::vector<float> cycleB (periodLen, 0.0f), cycleBR (periodLen, 0.0f);
                voice.render (cycleB.data(), cycleBR.data(), periodLen);

                // Tolerance wider than raw float epsilon: the phase accumulator's floating-point
                // rounding drifts by a few ULPs per sample, which a resonant filter's frequency
                // response can amplify to ~1e-3 over a 441-sample cycle without indicating any
                // actual non-periodicity bug.
                for (int i = 0; i < periodLen; ++i)
                    expectWithinAbsoluteError (cycleB[(size_t) i], cycleA[(size_t) i], 2.0e-3f);
            }

            auto sumAbsDiff = [] (const std::array<float, periodLen>& a, const std::array<float, periodLen>& b)
            {
                float total = 0.0f;
                for (int i = 0; i < periodLen; ++i)
                    total += std::abs (a[(size_t) i] - b[(size_t) i]);
                return total;
            };

            constexpr float kDistinctThreshold = 5.0f;   // generous - waveforms differ by far more than this

            auto checkDistinct = [&] (const char* label, const std::array<float, periodLen>& a, const std::array<float, periodLen>& b)
            {
                const float diff = sumAbsDiff (a, b);
                expect (diff > kDistinctThreshold, juce::String (label) + ": sumAbsDiff = " + juce::String (diff));
            };

            checkDistinct ("saw vs square",      saw,      square);
            checkDistinct ("saw vs triangle",     saw,      triangle);
            checkDistinct ("saw vs pulse",        saw,      pulse);
            checkDistinct ("square vs triangle",  square,   triangle);
            checkDistinct ("square vs pulse",     square,   pulse);
            checkDistinct ("triangle vs pulse",   triangle, pulse);
        }

        beginTest ("table-free generator stays audibly equivalent to the prior table-based rendering "
                   "(approval test: harmonics 1-8 and RMS within 3% of a fixed 128-point-table reference)");
        {
            constexpr float pitchHz   = 100.0f;   // 44100 / 100 = 441 samples/period exactly
            constexpr int   periodLen = 441;
            constexpr int   settleLen = periodLen;   // discard one period of ADSR/filter transient

            // Fixed baseline, independent of SynthVoice: the exact Phase 8 128-point-table
            // lambdas, so this stays a stable reference regardless of how SynthVoice's own
            // generator evolves.
            auto referenceSamples = [&] (berlin::Waveform waveform) -> std::vector<float>
            {
                juce::dsp::Oscillator<float> reference;

                switch (waveform)
                {
                    case berlin::Waveform::saw:
                        reference.initialise ([] (float x) { return x / juce::MathConstants<float>::pi; }, 128);
                        break;
                    case berlin::Waveform::square:
                        reference.initialise ([] (float x) { return x < 0.0f ? -1.0f : 1.0f; }, 128);
                        break;
                    case berlin::Waveform::triangle:
                        reference.initialise ([] (float x)
                        {
                            const float absX = x < 0.0f ? -x : x;
                            return (2.0f / juce::MathConstants<float>::pi) * absX - 1.0f;
                        }, 128);
                        break;
                    default:
                        break;
                }

                reference.prepare (makeSpec (sampleRate, periodLen * 3, 1));
                reference.setFrequency (pitchHz, true);

                for (int i = 0; i < settleLen; ++i)
                    reference.processSample (0.0f);

                std::vector<float> steady (static_cast<size_t> (periodLen), 0.0f);
                for (int i = 0; i < periodLen; ++i)
                    steady[(size_t) i] = reference.processSample (0.0f);
                return steady;
            };

            // Under test: SynthVoice's own rendering path (filter maxed toward transparent,
            // ADSR neutralised to sustain=1) so only the oscillator shapes the comparison.
            auto voiceSamples = [&] (berlin::Waveform waveform) -> std::vector<float>
            {
                berlin::SynthPatch patch = berlin::kDefaultPatch;
                patch.waveform  = waveform;
                patch.cutoffHz  = berlin::kMaxCutoffHz;
                patch.resonance = berlin::kMinResonance;
                patch.attack    = berlin::kMinAttackSeconds;
                patch.decay     = berlin::kMinDecaySeconds;
                patch.sustain   = 1.0f;

                berlin::SynthVoice voice;
                voice.prepare (makeSpec (sampleRate, periodLen * 3), patch);
                voice.noteOn (pitchHz);

                std::vector<float> discard (settleLen, 0.0f), discardR (settleLen, 0.0f);
                voice.render (discard.data(), discardR.data(), settleLen);

                std::vector<float> steady (static_cast<size_t> (periodLen), 0.0f), right (static_cast<size_t> (periodLen), 0.0f);
                voice.render (steady.data(), right.data(), periodLen);
                return steady;
            };

            auto harmonicMagnitude = [] (const std::vector<float>& samples, int harmonic) -> float
            {
                double real = 0.0, imag = 0.0;
                const int n = static_cast<int> (samples.size());

                for (int i = 0; i < n; ++i)
                {
                    const double angle = 2.0 * juce::MathConstants<double>::pi * harmonic * i / (double) n;
                    real += (double) samples[(size_t) i] * std::cos (angle);
                    imag += (double) samples[(size_t) i] * std::sin (angle);
                }

                return (float) (2.0 * std::sqrt (real * real + imag * imag) / (double) n);
            };

            auto rms = [] (const std::vector<float>& samples) -> float
            {
                double sumSquares = 0.0;
                for (float s : samples)
                    sumSquares += (double) s * (double) s;
                return (float) std::sqrt (sumSquares / (double) samples.size());
            };

            constexpr berlin::Waveform waveformsUnderTest[] = { berlin::Waveform::saw, berlin::Waveform::square, berlin::Waveform::triangle };

            for (auto waveform : waveformsUnderTest)
            {
                const std::vector<float> reference = referenceSamples (waveform);
                const std::vector<float> underTest  = voiceSamples (waveform);

                const float refRms  = rms (reference);
                const float testRms = rms (underTest);
                expect (std::abs (testRms - refRms) <= 0.03f * refRms,
                        "RMS mismatch for waveform " + juce::String ((int) waveform)
                            + ": ref=" + juce::String (refRms) + " test=" + juce::String (testRms));

                for (int harmonic = 1; harmonic <= 8; ++harmonic)
                {
                    const float refMag  = harmonicMagnitude (reference, harmonic);
                    const float testMag = harmonicMagnitude (underTest, harmonic);

                    // Harmonics with negligible reference energy (e.g. even harmonics of a
                    // triangle/square wave) are skipped - a 3% relative tolerance against a
                    // near-zero reference is meaningless noise, not a real equivalence check.
                    if (refMag < 1.0e-3f)
                        continue;

                    expect (std::abs (testMag - refMag) <= 0.03f * refMag,
                            "Harmonic " + juce::String (harmonic) + " mismatch for waveform "
                                + juce::String ((int) waveform) + ": ref=" + juce::String (refMag)
                                + " test=" + juce::String (testMag));
                }
            }
        }

        beginTest ("waveform switch mid-note allocates nothing structurally (plain member write) "
                   "and produces no dropout, all-finite output");
        {
            berlin::SynthPatch patch = berlin::kDefaultPatch;
            patch.waveform = berlin::Waveform::saw;
            patch.attack  = 0.0f;
            patch.decay   = 0.0f;
            patch.sustain = 1.0f;

            berlin::SynthVoice voice;
            voice.prepare (makeSpec (sampleRate, 1024), patch);
            voice.noteOn (220.0f);

            std::vector<float> before (256, 0.0f), beforeR (256, 0.0f);
            voice.render (before.data(), beforeR.data(), 256);

            bool anyNonZeroBefore = false;
            for (float s : before)
                if (s != 0.0f)
                    anyNonZeroBefore = true;
            expect (anyNonZeroBefore);

            voice.setWaveform (berlin::Waveform::square);   // switch mid-note

            std::vector<float> after (256, 0.0f), afterR (256, 0.0f);
            voice.render (after.data(), afterR.data(), 256);

            bool anyNonZeroAfter = false;
            for (float s : after)
            {
                expect (std::isfinite (s));
                if (s != 0.0f)
                    anyNonZeroAfter = true;
            }
            expect (anyNonZeroAfter);   // no dropout

            // Triangulate: a second switch, to a third waveform, must also work.
            voice.setWaveform (berlin::Waveform::triangle);

            std::vector<float> after2 (256, 0.0f), after2R (256, 0.0f);
            voice.render (after2.data(), after2R.data(), 256);

            bool anyNonZeroAfter2 = false;
            for (float s : after2)
            {
                expect (std::isfinite (s));
                if (s != 0.0f)
                    anyNonZeroAfter2 = true;
            }
            expect (anyNonZeroAfter2);
        }

        beginTest ("filter cutoff sweep: higher cutoff yields more high-frequency content (brighter)");
        {
            auto brightnessProxy = [&] (float cutoffHz) -> float
            {
                berlin::SynthPatch patch = berlin::kDefaultPatch;
                patch.waveform  = berlin::Waveform::square;   // harmonic-rich
                patch.cutoffHz  = cutoffHz;
                patch.resonance = 0.7071068f;
                patch.attack = 0.0f; patch.decay = 0.0f; patch.sustain = 1.0f;

                berlin::SynthVoice voice;
                voice.prepare (makeSpec (sampleRate, 2048), patch);
                voice.noteOn (200.0f);

                std::vector<float> discard (500, 0.0f), discardR (500, 0.0f);
                voice.render (discard.data(), discardR.data(), 500);

                std::vector<float> left (1000, 0.0f), right (1000, 0.0f);
                voice.render (left.data(), right.data(), 1000);

                float sumSquaredDiff = 0.0f;
                for (int i = 1; i < 1000; ++i)
                {
                    const float diff = left[(size_t) i] - left[(size_t) (i - 1)];
                    sumSquaredDiff += diff * diff;
                }
                return sumSquaredDiff / 999.0f;
            };

            const float dimProxy    = brightnessProxy (400.0f);
            const float brightProxy = brightnessProxy (15000.0f);

            expect (brightProxy > dimProxy);
        }

        beginTest ("filter resonance emphasizes energy near cutoff (peak amplitude increases)");
        {
            auto peakAmplitude = [&] (float resonance) -> float
            {
                berlin::SynthPatch patch = berlin::kDefaultPatch;
                patch.waveform  = berlin::Waveform::saw;   // rich harmonics
                patch.cutoffHz  = 1000.0f;
                patch.resonance = resonance;
                patch.attack = 0.0f; patch.decay = 0.0f; patch.sustain = 1.0f;

                berlin::SynthVoice voice;
                voice.prepare (makeSpec (sampleRate, 2048), patch);
                voice.noteOn (100.0f);

                std::vector<float> discard (500, 0.0f), discardR (500, 0.0f);
                voice.render (discard.data(), discardR.data(), 500);

                std::vector<float> left (1000, 0.0f), right (1000, 0.0f);
                voice.render (left.data(), right.data(), 1000);

                float peak = 0.0f;
                for (float sample : left)
                    peak = juce::jmax (peak, std::abs (sample));
                return peak;
            };

            const float lowResPeak  = peakAmplitude (0.7071068f);   // Butterworth, no peaking
            const float highResPeak = peakAmplitude (5.0f);         // strong resonance

            expect (highResPeak > lowResPeak);
        }

        auto brightnessOf = [] (const std::vector<float>& samples) -> float
        {
            float sumSquaredDiff = 0.0f;
            for (size_t i = 1; i < samples.size(); ++i)
            {
                const float diff = samples[i] - samples[i - 1];
                sumSquaredDiff += diff * diff;
            }
            return sumSquaredDiff / (float) (samples.size() - 1);
        };

        beginTest ("live cutoff change mid-note brightens without waiting for the next note-on");
        {
            berlin::SynthPatch patch = berlin::kDefaultPatch;
            patch.waveform  = berlin::Waveform::square;
            patch.cutoffHz  = 400.0f;
            patch.resonance = berlin::kMinResonance;
            patch.attack = 0.0f; patch.decay = 0.0f; patch.sustain = 1.0f;

            berlin::SynthVoice voice;
            voice.prepare (makeSpec (sampleRate, 2048), patch);
            voice.noteOn (200.0f);

            std::vector<float> discard (500, 0.0f), discardR (500, 0.0f);
            voice.render (discard.data(), discardR.data(), 500);

            std::vector<float> dim (1000, 0.0f), dimR (1000, 0.0f);
            voice.render (dim.data(), dimR.data(), 1000);
            const float dimBrightness = brightnessOf (dim);

            voice.setCutoffHz (15000.0f);   // live change mid-note, no new note-on

            std::vector<float> settle (200, 0.0f), settleR (200, 0.0f);
            voice.render (settle.data(), settleR.data(), 200);

            std::vector<float> bright (1000, 0.0f), brightR (1000, 0.0f);
            voice.render (bright.data(), brightR.data(), 1000);
            const float brightBrightness = brightnessOf (bright);

            expect (brightBrightness > dimBrightness);
        }

        beginTest ("live resonance change mid-note emphasizes energy near cutoff without waiting for the next note-on");
        {
            berlin::SynthPatch patch = berlin::kDefaultPatch;
            patch.waveform  = berlin::Waveform::saw;
            patch.cutoffHz  = 1000.0f;
            patch.resonance = berlin::kMinResonance;
            patch.attack = 0.0f; patch.decay = 0.0f; patch.sustain = 1.0f;

            berlin::SynthVoice voice;
            voice.prepare (makeSpec (sampleRate, 2048), patch);
            voice.noteOn (100.0f);

            std::vector<float> discard (500, 0.0f), discardR (500, 0.0f);
            voice.render (discard.data(), discardR.data(), 500);

            std::vector<float> low (1000, 0.0f), lowR (1000, 0.0f);
            voice.render (low.data(), lowR.data(), 1000);
            float lowPeak = 0.0f;
            for (float s : low) lowPeak = juce::jmax (lowPeak, std::abs (s));

            voice.setResonance (5.0f);   // live change mid-note

            std::vector<float> settle (200, 0.0f), settleR (200, 0.0f);
            voice.render (settle.data(), settleR.data(), 200);

            std::vector<float> high (1000, 0.0f), highR (1000, 0.0f);
            voice.render (high.data(), highR.data(), 1000);
            float highPeak = 0.0f;
            for (float s : high) highPeak = juce::jmax (highPeak, std::abs (s));

            expect (highPeak > lowPeak);
        }

        beginTest ("live release change mid-stage retargets release to the new (shorter) duration");
        {
            berlin::SynthPatch patch = berlin::kDefaultPatch;
            patch.attack  = 0.001f;
            patch.decay   = 0.001f;
            patch.sustain = 0.7f;
            patch.release = 2.0f;   // long initial release

            berlin::SynthVoice voice;
            voice.prepare (makeSpec (sampleRate, 4096), patch);
            voice.noteOn (440.0f);

            std::vector<float> warm (200, 0.0f), warmR (200, 0.0f);
            voice.render (warm.data(), warmR.data(), 200);   // reach sustain

            constexpr float newRelease = 0.05f;
            voice.setReleaseSeconds (newRelease);   // live change while sustaining - Decision 4's gate must let a real change through

            voice.noteOff();

            const int expectedReleaseSamples = (int) (newRelease * sampleRate);
            const int margin = 128;   // several control blocks of slack

            std::vector<float> tail (static_cast<size_t> (expectedReleaseSamples + margin), 0.0f);
            std::vector<float> tailR (tail.size(), 0.0f);
            voice.render (tail.data(), tailR.data(), (int) tail.size());

            std::vector<float> after (256, 1.0f), afterR (256, 1.0f);   // poison
            voice.render (after.data(), afterR.data(), 256);

            for (float sample : after)
                expectEquals (sample, 0.0f);   // silent by now: proves the NEW short release applied, not the stale 2s one
        }

        beginTest ("switching LFO destination away from cutoff mid-note re-parks cutoff to its base value");
        {
            berlin::SynthPatch patch = berlin::kDefaultPatch;
            patch.waveform  = berlin::Waveform::square;
            patch.cutoffHz  = 1000.0f;
            patch.resonance = berlin::kMinResonance;
            patch.attack = 0.0f; patch.decay = 0.0f; patch.sustain = 1.0f;
            patch.lfoDestination = berlin::LfoDestination::cutoff;
            patch.lfoRateHz = 0.1f;   // slow, so a quarter-cycle lands at a known off-centre phase
            patch.lfoDepth  = 1.0f;

            berlin::SynthVoice voice;
            voice.prepare (makeSpec (sampleRate, 8192), patch);
            voice.noteOn (200.0f);

            const int quarterCycleSamples = (int) (sampleRate / patch.lfoRateHz / 4.0);
            std::vector<float> warm (static_cast<size_t> (quarterCycleSamples), 0.0f), warmR (static_cast<size_t> (quarterCycleSamples), 0.0f);
            voice.render (warm.data(), warmR.data(), quarterCycleSamples);

            voice.setLfoDestination (berlin::LfoDestination::amplitude);   // abandon cutoff mid-note

            std::vector<float> afterSwitch (1000, 0.0f), afterSwitchR (1000, 0.0f);
            voice.render (afterSwitch.data(), afterSwitchR.data(), 1000);

            // Reference: a voice that never modulates cutoff (destination = amplitude with depth 0
            // from the very start) - its cutoff sits at the unmodulated base forever.
            berlin::SynthPatch referencePatch = patch;
            referencePatch.lfoDestination = berlin::LfoDestination::amplitude;
            referencePatch.lfoDepth = 0.0f;

            berlin::SynthVoice referenceVoice;
            referenceVoice.prepare (makeSpec (sampleRate, 8192), referencePatch);
            referenceVoice.noteOn (200.0f);

            std::vector<float> refWarm (static_cast<size_t> (quarterCycleSamples), 0.0f), refWarmR (static_cast<size_t> (quarterCycleSamples), 0.0f);
            referenceVoice.render (refWarm.data(), refWarmR.data(), quarterCycleSamples);

            std::vector<float> refAfter (1000, 0.0f), refAfterR (1000, 0.0f);
            referenceVoice.render (refAfter.data(), refAfterR.data(), 1000);

            const float switchedBrightness  = brightnessOf (afterSwitch);
            const float referenceBrightness = brightnessOf (refAfter);

            expect (std::abs (switchedBrightness - referenceBrightness) <= 0.1f * referenceBrightness,
                    "switched=" + juce::String (switchedBrightness) + " reference=" + juce::String (referenceBrightness));
        }

        beginTest ("switching LFO destination away from pitch mid-note re-parks frequency to baseFrequencyHz");
        {
            berlin::SynthPatch patch = berlin::kDefaultPatch;
            patch.waveform = berlin::Waveform::square;
            patch.attack = 0.0f; patch.decay = 0.0f; patch.sustain = 1.0f;
            patch.lfoDestination = berlin::LfoDestination::pitch;
            patch.lfoRateHz = 0.1f;
            patch.lfoDepth  = 1.0f;

            constexpr float pitchHz = 100.0f;   // 441 samples/period at 44100Hz when unmodulated

            berlin::SynthVoice voice;
            voice.prepare (makeSpec (sampleRate, 8192), patch);
            voice.noteOn (pitchHz);

            const int quarterCycleSamples = (int) (sampleRate / patch.lfoRateHz / 4.0);
            std::vector<float> warm (static_cast<size_t> (quarterCycleSamples), 0.0f), warmR (static_cast<size_t> (quarterCycleSamples), 0.0f);
            voice.render (warm.data(), warmR.data(), quarterCycleSamples);

            voice.setLfoDestination (berlin::LfoDestination::amplitude);   // abandon pitch mid-note

            std::vector<float> settle (200, 0.0f), settleR (200, 0.0f);
            voice.render (settle.data(), settleR.data(), 200);   // one control block is enough; generous margin

            // At the unmodulated base frequency (100Hz), a 4410-sample (0.1s) window spans exactly
            // 10 periods of a square wave -> 20 sign changes.
            constexpr int windowLen = 4410;
            std::vector<float> window (windowLen, 0.0f), windowR (windowLen, 0.0f);
            voice.render (window.data(), windowR.data(), windowLen);

            int crossings = 0;
            for (int i = 1; i < windowLen; ++i)
                if ((window[(size_t) (i - 1)] < 0.0f) != (window[(size_t) i] < 0.0f))
                    ++crossings;

            expect (crossings >= 18 && crossings <= 22, "crossings=" + juce::String (crossings));
        }

        beginTest ("driving every setter to extremes keeps output finite (clamped, no jassert trip)");
        {
            berlin::SynthVoice voice;
            voice.prepare (makeSpec (sampleRate, 2048), berlin::kDefaultPatch);
            voice.noteOn (440.0f);

            voice.setCutoffHz (1.0e9f);
            voice.setResonance (-100.0f);
            voice.setPulseWidth (1.0e9f);
            voice.setAttackSeconds (-100.0f);
            voice.setDecaySeconds (0.0f);
            voice.setSustain (1.0e9f);
            voice.setReleaseSeconds (0.0f);
            voice.setLfoRateHz (-100.0f);
            voice.setLfoDepth (1.0e9f);
            voice.setLfoDestination (berlin::LfoDestination::cutoff);
            voice.setWaveform (berlin::Waveform::pulse);

            std::vector<float> left (2048, 0.0f), right (2048, 0.0f);
            voice.render (left.data(), right.data(), 2048);

            for (float sample : left)
                expect (std::isfinite (sample));

            // Triangulate with the opposite extreme.
            voice.setCutoffHz (-100.0f);
            voice.setResonance (1.0e9f);
            voice.setPulseWidth (-100.0f);
            voice.setAttackSeconds (1.0e9f);
            voice.setDecaySeconds (1.0e9f);
            voice.setSustain (-100.0f);
            voice.setReleaseSeconds (1.0e9f);
            voice.setLfoRateHz (1.0e9f);
            voice.setLfoDepth (-100.0f);

            std::vector<float> left2 (2048, 0.0f), right2 (2048, 0.0f);
            voice.render (left2.data(), right2.data(), 2048);

            for (float sample : left2)
                expect (std::isfinite (sample));
        }
    }
};

static SynthVoiceTests synthVoiceTests;
