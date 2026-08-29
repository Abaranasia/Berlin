# DSP Stability Tests: Silent Input, Feedback, CPU Budget

> Adapted from Millenia's `.claude/skills/juce-plugin-testing/references/dsp-stability-tests.md`
> (itself adapted from [glittercowboy/plugin-freedom-system](https://github.com/glittercowboy/plugin-freedom-system)),
> generalized here since Berlin has no concrete DSP classes yet. Replace `YourDspClass`
> with the real class name once it exists; these are templates to adapt, not drop-in tests.

These three checks aren't redundant with a plain impulse-response test — each one catches
a distinct, common failure mode in real-time DSP:

| Test | Catches | Why it matters |
|---|---|---|
| Silent input | Uncleared buffers, denormal build-up | Any DSP with delay lines/circular buffers carries state across blocks |
| Feedback/regeneration stability | Runaway gain, unstable coefficients | Any topology with cross-feeding or regen loops can explode when a coefficient is off |
| CPU budget | Real-time factor regressions | Worth tracking as DSP components are added |

## Silent input test

```cpp
class YourDspClassSilentInputTest : public juce::UnitTest
{
public:
    YourDspClassSilentInputTest() : juce::UnitTest ("YourDspClass: silent input", "DSP") {}

    void runTest() override
    {
        beginTest ("Output stays near noise floor for silent input");

        YourDspClass dsp;
        juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };
        dsp.prepare (spec);
        dsp.reset();

        juce::AudioBuffer<float> buffer (2, 512);
        buffer.clear();

        // Run several blocks — stateful DSP needs more than one silent
        // block to reveal buffer garbage or denormal creep.
        for (int i = 0; i < 20; ++i)
        {
            juce::dsp::AudioBlock<float> block (buffer);
            dsp.process (block);
        }

        float peak = buffer.getMagnitude (0, buffer.getNumSamples());
        expect (peak < 0.0001f, "Silent input produced output peak " + juce::String (peak));
    }
};

static YourDspClassSilentInputTest yourDspClassSilentInputTest;
```

If the DSP has real decay time (e.g. a reverb tail), assert the noise-floor threshold only
after enough blocks for the tail to fall below it — compute the block count from decay
time × sample rate, don't guess a fixed number.

## Feedback / regeneration stability test

```cpp
class YourDspClassStabilityTest : public juce::UnitTest
{
public:
    YourDspClassStabilityTest() : juce::UnitTest ("YourDspClass: bounded output", "DSP") {}

    void runTest() override
    {
        beginTest ("Sustained input does not cause runaway output");

        YourDspClass dsp;
        const double sampleRate = 44100.0;
        juce::dsp::ProcessSpec spec { sampleRate, 512, 2 };
        dsp.prepare (spec);
        dsp.reset();

        juce::AudioBuffer<float> buffer (2, 512);
        float maxAbsSeen = 0.0f;

        // ~11 seconds of continuous signal — long enough for a slow feedback
        // divergence to show up, short enough to stay a fast unit test.
        for (int i = 0; i < 1000; ++i)
        {
            fillWithSineWave (buffer, 440.0f, (float) sampleRate);

            juce::dsp::AudioBlock<float> block (buffer);
            dsp.process (block);

            maxAbsSeen = juce::jmax (maxAbsSeen, buffer.getMagnitude (0, buffer.getNumSamples()));

            if (maxAbsSeen > 100.0f)
            {
                expect (false, "Output exploding at iteration " + juce::String (i)
                                + ", peak " + juce::String (maxAbsSeen));
                return;
            }
        }

        expect (maxAbsSeen < 10.0f, "Sustained output peak " + juce::String (maxAbsSeen)
                                     + " exceeds safety margin");
    }

private:
    static void fillWithSineWave (juce::AudioBuffer<float>& buffer, float freqHz, float sampleRate)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                data[i] = std::sin (2.0f * juce::MathConstants<float>::pi * freqHz * (float) i / sampleRate);
        }
    }
};

static YourDspClassStabilityTest yourDspClassStabilityTest;
```

If this fails, check in order: any regen/feedback coefficient that can exceed 1.0 at the
parameter's extreme setting, filter coefficients that go unstable (poles outside the unit
circle) at extreme cutoff/resonance, and any gain multiply not paired with attenuation.
Add a hard-clip safety net (`juce::jlimit`) as a last line of defense — it should never be
the thing keeping the DSP stable, but it should exist.

## CPU budget test

```cpp
class YourDspClassCpuBudgetTest : public juce::UnitTest
{
public:
    YourDspClassCpuBudgetTest() : juce::UnitTest ("YourDspClass: CPU budget", "DSP") {}

    void runTest() override
    {
        beginTest ("Real-time factor stays within budget");

        YourDspClass dsp;
        const double sampleRate = 44100.0;
        const int blockSize = 512;
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) blockSize, 2 };
        dsp.prepare (spec);
        dsp.reset();

        juce::AudioBuffer<float> buffer (2, blockSize);
        fillWithSineWave (buffer, 1000.0f, (float) sampleRate);

        // Warm-up
        for (int i = 0; i < 100; ++i)
        {
            juce::dsp::AudioBlock<float> block (buffer);
            dsp.process (block);
        }

        const auto start = juce::Time::getHighResolutionTicks();
        constexpr int iterations = 1000;
        for (int i = 0; i < iterations; ++i)
        {
            juce::dsp::AudioBlock<float> block (buffer);
            dsp.process (block);
        }
        const auto end = juce::Time::getHighResolutionTicks();

        const double elapsedSeconds = juce::Time::highResolutionTicksToSeconds (end - start);
        const double audioSeconds = (iterations * blockSize) / sampleRate;
        const double realTimeFactor = elapsedSeconds / audioSeconds;

        logMessage ("Real-time factor: " + juce::String (realTimeFactor, 4));

        // Pick the budget row below for this DSP's category. Release build only —
        // Debug builds run far slower and will false-positive this.
        expect (realTimeFactor < 0.15, "Real-time factor " + juce::String (realTimeFactor, 4)
                                        + " exceeds 0.15 budget");
    }

private:
    static void fillWithSineWave (juce::AudioBuffer<float>& buffer, float freqHz, float sampleRate)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                data[i] = std::sin (2.0f * juce::MathConstants<float>::pi * freqHz * (float) i / sampleRate);
        }
    }
};

static YourDspClassCpuBudgetTest yourDspClassCpuBudgetTest;
```

**Run this in a Release build.** The Hard Rule against wall-clock assertions is about flaky
*absolute* timing; a relative budget check like this is fine as long as it only ever runs
against optimized code.

### Real-time factor budget by processing type

| Processing type | Budget (real-time factor) |
|---|---|
| Simple utility (gain, pan) | < 0.01 |
| Dynamics (compressor, gate) | < 0.05 |
| EQ (parametric, 4–8 bands) | < 0.08 |
| Time-based FX (delay, chorus) | < 0.10 |
| Algorithmic reverb | < 0.15 |
| Complex/spectral FX (convolution, phase vocoder) | < 0.30 |

For a DSP chain combining multiple stages (e.g. a reverb feeding a pitch shifter), budget
the *combined* engine against the more expensive stage's row, not each component separately.
