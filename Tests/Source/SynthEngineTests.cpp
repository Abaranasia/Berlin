/*
  ==============================================================================

   SynthEngine tests (internal-synth-output spec, roadmap Phase 8 / internal-synth
   Phase 3). RED first: Source/synth/SynthEngine.h does not exist yet, so this
   suite must fail to compile until Phase 3's production file is created.

   Covers (Phase 3 scope): first non-zero sample lands at
   startSample + sampleOffset; setEnabled(false) leaves the destination
   silent; reset() silences a sounding voice; note-off with a mismatched note
   does not cut the voice; numSamples beyond the prepared block size is
   clamped, never resized.

   Phase 4 additions (internal-synth-output: Terminal Delay And Reverb,
   Mixing Without Clipping): effectsEnabled=false (default) produces
   bit-identical dry output vs. SynthVoice's own render; the true->false
   edge hard-clears an active tail with no residual bleed; reset() clears
   an active effect tail; a sustained full-level note with effects enabled
   stays within the valid output range (no-clip guard).

  ==============================================================================
*/

#include <cmath>
#include <functional>
#include <vector>

#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>

#include "synth/SynthEngine.h"
#include "synth/SynthPatch.h"
#include "synth/SynthVoice.h"
#include "playback/StepEventBuffer.h"

namespace
{
    juce::dsp::ProcessSpec makeSpec (double sampleRate, int blockSize, int numChannels = 2)
    {
        return { sampleRate, static_cast<juce::uint32> (blockSize), static_cast<juce::uint32> (numChannels) };
    }
}

class SynthEngineTests final : public juce::UnitTest
{
public:
    SynthEngineTests() : juce::UnitTest ("SynthEngine", "Berlin") {}

    void runTest() override
    {
        constexpr double sampleRate = 44100.0;

        beginTest ("note-on mid-block: first non-zero sample lands at startSample + sampleOffset");
        {
            berlin::SynthEngine engine;
            engine.prepare (makeSpec (sampleRate, 512));

            constexpr int startSample   = 0;
            constexpr int numSamples    = 400;
            constexpr int sampleOffset  = 100;

            berlin::StepEventBuffer events;
            events.push ({ sampleOffset, 0, 60, true });   // note-on mid-block

            juce::AudioBuffer<float> destination (2, startSample + numSamples);
            destination.clear();

            engine.render (events, destination, startSample, numSamples);

            for (int i = 0; i < sampleOffset; ++i)
                expectEquals (destination.getSample (0, startSample + i), 0.0f);

            bool anyNonZeroFromOffset = false;
            for (int i = sampleOffset; i < numSamples; ++i)
                if (destination.getSample (0, startSample + i) != 0.0f)
                    anyNonZeroFromOffset = true;

            expect (anyNonZeroFromOffset);
        }

        beginTest ("setEnabled(false) leaves the destination silent");
        {
            berlin::SynthEngine engine;
            engine.prepare (makeSpec (sampleRate, 512));
            engine.setEnabled (false);

            berlin::StepEventBuffer events;
            events.push ({ 0, 0, 60, true });   // note-on at block start

            juce::AudioBuffer<float> destination (2, 256);
            destination.clear();

            engine.render (events, destination, 0, 256);

            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 256; ++i)
                    expectEquals (destination.getSample (ch, i), 0.0f);
        }

        beginTest ("reset() silences a sounding voice");
        {
            berlin::SynthEngine engine;
            engine.prepare (makeSpec (sampleRate, 512));

            berlin::StepEventBuffer noteOnEvents;
            noteOnEvents.push ({ 0, 0, 60, true });

            juce::AudioBuffer<float> warm (2, 256);
            warm.clear();
            engine.render (noteOnEvents, warm, 0, 256);

            bool anyNonZero = false;
            for (int i = 0; i < 256; ++i)
                if (warm.getSample (0, i) != 0.0f)
                    anyNonZero = true;
            expect (anyNonZero);   // confirm the voice was actually sounding before reset

            engine.reset();

            berlin::StepEventBuffer noEvents;   // no new events - prior note must NOT resume
            juce::AudioBuffer<float> afterReset (2, 256);
            afterReset.clear();
            engine.render (noEvents, afterReset, 0, 256);

            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 256; ++i)
                    expectEquals (afterReset.getSample (ch, i), 0.0f);
        }

        beginTest ("note-off with a mismatched note does not cut the voice");
        {
            berlin::SynthEngine control;
            control.prepare (makeSpec (sampleRate, 512));

            berlin::SynthEngine mismatched;
            mismatched.prepare (makeSpec (sampleRate, 512));

            berlin::StepEventBuffer noteOnEvents;
            noteOnEvents.push ({ 0, 0, 60, true });

            juce::AudioBuffer<float> controlBuffer (2, 128);
            controlBuffer.clear();
            control.render (noteOnEvents, controlBuffer, 0, 128);

            juce::AudioBuffer<float> mismatchedBuffer (2, 128);
            mismatchedBuffer.clear();
            mismatched.render (noteOnEvents, mismatchedBuffer, 0, 128);

            // Second block: control gets no events; mismatched gets a note-off for a
            // DIFFERENT note than the one currently sounding (60). Since the note
            // does not match, the mismatched engine's voice must keep sounding
            // identically to the control (no note-matched cut occurred).
            berlin::StepEventBuffer noEvents;
            berlin::StepEventBuffer mismatchedNoteOff;
            mismatchedNoteOff.push ({ 0, 0, 61, false });   // note 61 != sounding note 60

            juce::AudioBuffer<float> controlBuffer2 (2, 128);
            controlBuffer2.clear();
            control.render (noEvents, controlBuffer2, 0, 128);

            juce::AudioBuffer<float> mismatchedBuffer2 (2, 128);
            mismatchedBuffer2.clear();
            mismatched.render (mismatchedNoteOff, mismatchedBuffer2, 0, 128);

            for (int i = 0; i < 128; ++i)
                expectEquals (mismatchedBuffer2.getSample (0, i), controlBuffer2.getSample (0, i));
        }

        beginTest ("numSamples beyond the prepared block size is clamped, never resized");
        {
            berlin::SynthEngine engine;
            constexpr int preparedBlockSize = 64;
            engine.prepare (makeSpec (sampleRate, preparedBlockSize));

            berlin::StepEventBuffer events;
            events.push ({ 0, 0, 60, true });

            constexpr int oversizedNumSamples = 1000;
            juce::AudioBuffer<float> destination (2, oversizedNumSamples);
            destination.clear();
            for (int i = preparedBlockSize; i < oversizedNumSamples; ++i)
            {
                destination.setSample (0, i, 1.0f);   // poison beyond the clamp boundary
                destination.setSample (1, i, 1.0f);
            }

            engine.render (events, destination, 0, oversizedNumSamples);   // must not crash

            for (int ch = 0; ch < 2; ++ch)
                for (int i = preparedBlockSize; i < oversizedNumSamples; ++i)
                    expectEquals (destination.getSample (ch, i), 1.0f);   // untouched: clamp held
        }

        beginTest ("effectsEnabled=false (default) produces bit-identical dry output vs. the voice's own render");
        {
            berlin::SynthEngine engine;
            engine.prepare (makeSpec (sampleRate, 512));   // effectsEnabled defaults to false

            berlin::SynthVoice voice;
            voice.prepare (makeSpec (sampleRate, 512), berlin::kDefaultPatch);

            berlin::StepEventBuffer events;
            events.push ({ 0, 0, 60, true });

            juce::AudioBuffer<float> engineOut (2, 256);
            engineOut.clear();
            engine.render (events, engineOut, 0, 256);

            std::vector<float> left (256, 0.0f), right (256, 0.0f);
            voice.noteOn (static_cast<float> (juce::MidiMessage::getMidiNoteInHertz (60)));
            voice.render (left.data(), right.data(), 256);

            for (int i = 0; i < 256; ++i)
            {
                expectEquals (engineOut.getSample (0, i), left[(size_t) i]);
                expectEquals (engineOut.getSample (1, i), right[(size_t) i]);
            }
        }

        beginTest ("disabling effects at the true->false edge hard-clears the tail with no residual bleed");
        {
            // Prepared block size must cover the largest single render() call below
            // (the full release block) - render() clamps numSamples to the prepared
            // scratch capacity rather than resizing (Decision 4).
            constexpr int preparedBlockSize = 16384;

            berlin::SynthEngine engine;
            engine.prepare (makeSpec (sampleRate, preparedBlockSize));
            engine.setEffectsEnabled (true);

            berlin::StepEventBuffer noteOnEvents;
            noteOnEvents.push ({ 0, 0, 60, true });

            juce::AudioBuffer<float> warm (2, 2048);
            warm.clear();
            engine.render (noteOnEvents, warm, 0, 2048);   // let delay/reverb build up energy

            berlin::StepEventBuffer noteOffEvents;
            noteOffEvents.push ({ 0, 0, 60, false });
            // Margin beyond kDefaultPatch's own release time so the VOICE's envelope
            // (not just any FX tail) has fully reached silence by the end of this
            // block - otherwise a still-sounding voice would be mistaken for FX tail.
            const int releaseSamples = (int) (berlin::kDefaultPatch.release * sampleRate) + 2000;
            juce::AudioBuffer<float> releaseBlock (2, releaseSamples);
            releaseBlock.clear();
            engine.render (noteOffEvents, releaseBlock, 0, releaseSamples);   // voice reaches silence by the end

            berlin::StepEventBuffer noEvents;

            // The VOICE is now silent (release completed above); any energy seen
            // from here on can only be an FX (delay/reverb) tail from the earlier
            // input, not the dry voice.
            juce::AudioBuffer<float> tailCheck (2, 512);
            tailCheck.clear();
            engine.render (noEvents, tailCheck, 0, 512);

            bool tailAudibleBeforeDisable = false;
            for (int i = 0; i < 512; ++i)
                if (std::abs (tailCheck.getSample (0, i)) > 1.0e-6f)
                    tailAudibleBeforeDisable = true;
            expect (tailAudibleBeforeDisable);   // confirm there WAS an audible delay/reverb tail while enabled

            engine.setEffectsEnabled (false);   // true->false edge: hard-clear the tail exactly once

            juce::AudioBuffer<float> afterDisable (2, 512);
            afterDisable.clear();
            engine.render (noEvents, afterDisable, 0, 512);

            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 512; ++i)
                    expectEquals (afterDisable.getSample (ch, i), 0.0f);
        }

        beginTest ("reset() clears an active effect tail");
        {
            berlin::SynthEngine engine;
            engine.prepare (makeSpec (sampleRate, 2048));   // must cover the warm-up render below (Decision 4 clamp)
            engine.setEffectsEnabled (true);

            berlin::StepEventBuffer noteOnEvents;
            noteOnEvents.push ({ 0, 0, 60, true });
            juce::AudioBuffer<float> warm (2, 2048);
            warm.clear();
            engine.render (noteOnEvents, warm, 0, 2048);

            engine.reset();

            berlin::StepEventBuffer noEvents;
            juce::AudioBuffer<float> afterReset (2, 512);
            afterReset.clear();
            engine.render (noEvents, afterReset, 0, 512);

            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 512; ++i)
                    expectEquals (afterReset.getSample (ch, i), 0.0f);
        }

        beginTest ("sustained full-level note with effects enabled stays within the valid output range");
        {
            berlin::SynthEngine engine;
            engine.prepare (makeSpec (sampleRate, 2048));
            engine.setEffectsEnabled (true);

            berlin::StepEventBuffer noteOnEvents;
            noteOnEvents.push ({ 0, 0, 60, true });

            juce::AudioBuffer<float> destination (2, 2048);
            destination.clear();
            engine.render (noteOnEvents, destination, 0, 2048);

            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 2048; ++i)
                    expect (std::abs (destination.getSample (ch, i)) <= 1.5f);
        }

        beginTest ("each SynthEngine setter forwarder produces identical rendered output to calling the voice setter directly");
        {
            auto compareForwarder = [&] (const char* label,
                                          std::function<void (berlin::SynthEngine&)> applyToEngine,
                                          std::function<void (berlin::SynthVoice&)> applyToVoice)
            {
                berlin::SynthEngine engine;
                engine.prepare (makeSpec (sampleRate, 512));
                applyToEngine (engine);

                berlin::SynthVoice voice;
                voice.prepare (makeSpec (sampleRate, 512), berlin::kDefaultPatch);
                applyToVoice (voice);

                berlin::StepEventBuffer events;
                events.push ({ 0, 0, 60, true });

                juce::AudioBuffer<float> engineOut (2, 512);
                engineOut.clear();
                engine.render (events, engineOut, 0, 512);

                std::vector<float> left (512, 0.0f), right (512, 0.0f);
                voice.noteOn (static_cast<float> (juce::MidiMessage::getMidiNoteInHertz (60)));
                voice.render (left.data(), right.data(), 512);

                for (int i = 0; i < 512; ++i)
                {
                    expectEquals (engineOut.getSample (0, i), left[(size_t) i], juce::String (label));
                    expectEquals (engineOut.getSample (1, i), right[(size_t) i], juce::String (label));
                }
            };

            compareForwarder ("setWaveform",
                [] (berlin::SynthEngine& e) { e.setWaveform (berlin::Waveform::square); },
                [] (berlin::SynthVoice& v)  { v.setWaveform (berlin::Waveform::square); });

            compareForwarder ("setCutoffHz",
                [] (berlin::SynthEngine& e) { e.setCutoffHz (2000.0f); },
                [] (berlin::SynthVoice& v)  { v.setCutoffHz (2000.0f); });

            compareForwarder ("setResonance",
                [] (berlin::SynthEngine& e) { e.setResonance (3.0f); },
                [] (berlin::SynthVoice& v)  { v.setResonance (3.0f); });

            compareForwarder ("setPulseWidth",
                [] (berlin::SynthEngine& e) { e.setWaveform (berlin::Waveform::pulse); e.setPulseWidth (0.25f); },
                [] (berlin::SynthVoice& v)  { v.setWaveform (berlin::Waveform::pulse); v.setPulseWidth (0.25f); });

            compareForwarder ("setAttackSeconds",
                [] (berlin::SynthEngine& e) { e.setAttackSeconds (0.5f); },
                [] (berlin::SynthVoice& v)  { v.setAttackSeconds (0.5f); });

            compareForwarder ("setDecaySeconds",
                [] (berlin::SynthEngine& e) { e.setDecaySeconds (0.5f); },
                [] (berlin::SynthVoice& v)  { v.setDecaySeconds (0.5f); });

            compareForwarder ("setSustain",
                [] (berlin::SynthEngine& e) { e.setSustain (0.3f); },
                [] (berlin::SynthVoice& v)  { v.setSustain (0.3f); });

            compareForwarder ("setReleaseSeconds",
                [] (berlin::SynthEngine& e) { e.setReleaseSeconds (1.0f); },
                [] (berlin::SynthVoice& v)  { v.setReleaseSeconds (1.0f); });

            compareForwarder ("setLfoRateHz",
                [] (berlin::SynthEngine& e) { e.setLfoRateHz (2.0f); },
                [] (berlin::SynthVoice& v)  { v.setLfoRateHz (2.0f); });

            compareForwarder ("setLfoDepth",
                [] (berlin::SynthEngine& e) { e.setLfoDepth (0.5f); },
                [] (berlin::SynthVoice& v)  { v.setLfoDepth (0.5f); });

            compareForwarder ("setLfoDestination",
                [] (berlin::SynthEngine& e) { e.setLfoDestination (berlin::LfoDestination::cutoff); },
                [] (berlin::SynthVoice& v)  { v.setLfoDestination (berlin::LfoDestination::cutoff); });
        }
    }
};

static SynthEngineTests synthEngineTests;
