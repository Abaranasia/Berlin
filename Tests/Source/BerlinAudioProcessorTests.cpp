/*
  ==============================================================================

   BerlinAudioProcessor tests (roadmap Phase 11 / vst3-au-plugin,
   plugin-host-integration + plugin-state-recall + realtime-audio-wiring +
   midi-output-dispatch specs). RED first: Source/plugin/BerlinAudioProcessor.h
   does not exist yet, so this suite must fail to compile until Phase 4's
   production file is created.

   Headless: no audio device, no editor (BERLIN_HEADLESS=1 in
   Tests/BerlinTests.jucer). Direct instantiation, driven by allocated
   AudioBuffer<float>/juce::MidiBuffer pairs only.

  ==============================================================================
*/

#include <cmath>
#include <map>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors_headless/juce_audio_processors_headless.h>
#include <juce_core/juce_core.h>

#include "plugin/BerlinAudioProcessor.h"

namespace
{
    struct TempPresetDir
    {
        TempPresetDir()
            : dir (juce::File::getSpecialLocation (juce::File::tempDirectory)
                       .getNonexistentChildFile ("BerlinAudioProcessorTests", "", false))
        {
        }

        ~TempPresetDir()
        {
            dir.deleteRecursively();
        }

        juce::File dir;
    };
}

class BerlinAudioProcessorTests final : public juce::UnitTest
{
public:
    BerlinAudioProcessorTests() : juce::UnitTest ("BerlinAudioProcessor", "Berlin") {}

    void runTest() override
    {
        beginTest ("regenerate() is deterministic for a given seed");
        {
            berlin::BerlinAudioProcessor a;
            berlin::BerlinAudioProcessor b;

            a.setSeed (777);
            b.setSeed (777);

            expect (a.regenerate (false));
            expect (b.regenerate (false));

            expect (a.getCurrentSequence() == b.getCurrentSequence());
        }

        beginTest ("regenerate() busy rejection leaves state unchanged");
        {
            berlin::BerlinAudioProcessor processor;
            processor.setSeed (111);
            expect (processor.regenerate (false));   // first publish: succeeds, now pending/unadopted

            const auto seedBefore = processor.getSeed();
            const auto sequenceBefore = processor.getCurrentSequence();

            // No processBlock ran in between, so the first publish is still
            // unadopted: this second call must be rejected (busy).
            // drawNewSeed=false deliberately: with drawNewSeed=true, a fresh
            // seed is drawn BEFORE the busy check (verbatim pre-existing
            // MainComponent::regenerate behavior) - only sequence/seed
            // state under drawNewSeed=false is unconditionally unchanged on
            // a busy rejection.
            expect (! processor.regenerate (false));

            expect (processor.getSeed() == seedBefore);
            expect (processor.getCurrentSequence() == sequenceBefore);
        }

        beginTest ("mutate() advances mutationCount only on success");
        {
            berlin::BerlinAudioProcessor processor;
            expectEquals (processor.getMutationCount(), 0);

            expect (processor.mutate());   // first publish: succeeds
            expectEquals (processor.getMutationCount(), 1);

            // Still unadopted (no processBlock ran): must be rejected, count unchanged.
            expect (! processor.mutate());
            expectEquals (processor.getMutationCount(), 1);
        }

        beginTest ("getStateInformation/setStateInformation round-trip patch and seed");
        {
            berlin::BerlinAudioProcessor source;

            berlin::SynthPatch patch;
            patch.waveform  = berlin::Waveform::square;
            patch.cutoffHz  = 1234.5f;
            patch.resonance = 3.0f;
            source.setPatch (patch);
            source.setSeed (98765);

            juce::MemoryBlock state;
            source.getStateInformation (state);

            berlin::BerlinAudioProcessor destination;
            destination.setStateInformation (state.getData(), (int) state.getSize());

            expect (destination.getPatch().waveform == patch.waveform);
            expectEquals (destination.getPatch().cutoffHz, patch.cutoffHz);
            expectEquals (destination.getPatch().resonance, patch.resonance);
            expect (destination.getSeed() == source.getSeed());
        }

        beginTest ("Two restores from the same saved state agree (round-trip determinism)");
        {
            berlin::BerlinAudioProcessor source;
            source.setSeed (55555);

            juce::MemoryBlock state;
            source.getStateInformation (state);

            berlin::BerlinAudioProcessor first, second;
            first.setStateInformation (state.getData(), (int) state.getSize());
            second.setStateInformation (state.getData(), (int) state.getSize());

            expect (first.getCurrentSequence() == second.getCurrentSequence());
        }

        beginTest ("setStateInformation with garbage bytes leaves state untouched, no crash");
        {
            berlin::BerlinAudioProcessor processor;
            processor.setSeed (24680);
            const auto seedBefore = processor.getSeed();
            const auto patchBefore = processor.getPatch();

            const unsigned char garbage[] = { 0x01, 0x02, 0x03, 0xFF, 0xEE, 0x00, 0x10, 0x20 };
            processor.setStateInformation (garbage, (int) sizeof (garbage));

            expect (processor.getSeed() == seedBefore);
            expect (processor.getPatch().waveform == patchBefore.waveform);
            expectEquals (processor.getPatch().cutoffHz, patchBefore.cutoffHz);
        }

        beginTest ("setStateInformation with empty/truncated bytes leaves state untouched, no crash");
        {
            berlin::BerlinAudioProcessor processor;
            processor.setSeed (13579);
            const auto seedBefore = processor.getSeed();

            processor.setStateInformation (nullptr, 0);
            expect (processor.getSeed() == seedBefore);

            juce::MemoryBlock validState;
            processor.getStateInformation (validState);
            processor.setStateInformation (validState.getData(), 3);   // truncated
            expect (processor.getSeed() == seedBefore);
        }

        beginTest ("Preset save/load round-trip via a temp directory");
        {
            TempPresetDir temp;
            berlin::BerlinAudioProcessor processor (temp.dir);

            berlin::SynthPatch patch;
            patch.cutoffHz = 4321.0f;
            processor.setPatch (patch);
            processor.setSeed (2468);

            expect (processor.save ("MyPreset") == berlin::PresetResult::ok);
            expect (processor.presetExists ("MyPreset"));
            expect (processor.listPresetNames().contains ("MyPreset"));

            berlin::BerlinAudioProcessor loader (temp.dir);
            expect (loader.loadPreset ("MyPreset") == berlin::PresetResult::ok);
            expectEquals (loader.getPatch().cutoffHz, patch.cutoffHz);
            expect (loader.getSeed() == 2468);
        }

        beginTest ("loadPreset applies scaleType/rootPitchClass/rangeLow/rangeHigh without clobbering staged mode/pulses/rotation/stepProbability");
        {
            // scale-aware-generation: Preset persists ONLY scaleType/
            // rootPitchClass/rangeLow/rangeHigh - mode/pulses/rotation/
            // stepProbability remain transient staged GUI configuration
            // (design.md's scoped exception to prior D4). This is the
            // BerlinAudioProcessor-level integration test tasks.md 6.1
            // describes; PresetManagerFileTests.cpp (PresetManager-only,
            // no GenerationParams ownership) is not the right home for it.
            TempPresetDir temp;
            berlin::BerlinAudioProcessor saver (temp.dir);

            berlin::GenerationParams savedParams;
            savedParams.scaleType      = berlin::ScaleType::dorian;
            savedParams.rootPitchClass = 2;    // D
            savedParams.rangeLow       = 40;
            savedParams.rangeHigh      = 64;
            saver.setGenerationParams (savedParams);
            saver.setSeed (13579);

            expect (saver.save ("ScalePreset") == berlin::PresetResult::ok);

            berlin::BerlinAudioProcessor loader (temp.dir);

            berlin::GenerationParams stagedParams;
            stagedParams.mode            = berlin::RhythmMode::euclidean;
            stagedParams.pulses          = 9;
            stagedParams.rotation        = 3;
            stagedParams.stepProbability = 0.7f;
            loader.setGenerationParams (stagedParams);

            expect (loader.loadPreset ("ScalePreset") == berlin::PresetResult::ok);

            const auto& result = loader.getGenerationParams();
            expect (result.scaleType == berlin::ScaleType::dorian);
            expectEquals (result.rootPitchClass, 2);
            expectEquals (result.rangeLow, 40);
            expectEquals (result.rangeHigh, 64);

            // Staged rhythm fields must be untouched by loadPreset - proves
            // loadPreset applies the 4 fields individually, NOT via a
            // whole-GenerationParams-struct assignment.
            expect (result.mode == berlin::RhythmMode::euclidean);
            expectEquals (result.pulses, 9);
            expectEquals (result.rotation, 3);
            expectEquals (result.stepProbability, 0.7f);

            expect (loader.getSeed() == 13579);
        }

        beginTest ("prepareToPlay -> N x processBlock -> releaseResources across block sizes and sample rates");
        {
            const int blockSizes[] = { 1, 7, 512, 4096 };
            const double sampleRates[] = { 44100.0, 48000.0, 96000.0 };

            for (double sr : sampleRates)
            {
                for (int blockSize : blockSizes)
                {
                    berlin::BerlinAudioProcessor processor;
                    processor.prepareToPlay (sr, blockSize);

                    for (int i = 0; i < 20; ++i)
                    {
                        juce::AudioBuffer<float> buffer (2, blockSize);
                        juce::MidiBuffer midi;
                        processor.processBlock (buffer, midi);

                        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                            for (int s = 0; s < buffer.getNumSamples(); ++s)
                                expect (std::isfinite (buffer.getSample (ch, s)));
                    }

                    processor.releaseResources();
                }
            }
        }

        beginTest ("processBlock before prepareToPlay: no crash, silent output");
        {
            berlin::BerlinAudioProcessor processor;

            juce::AudioBuffer<float> buffer (2, 256);
            buffer.clear();
            juce::MidiBuffer midi;

            processor.processBlock (buffer, midi);   // must not crash

            for (int ch = 0; ch < 2; ++ch)
                for (int s = 0; s < 256; ++s)
                    expectEquals (buffer.getSample (ch, s), 0.0f);
        }

        beginTest ("Pre-filled input MidiBuffer is cleared; host events never observed");
        {
            berlin::BerlinAudioProcessor processor;
            processor.prepareToPlay (44100.0, 512);

            juce::AudioBuffer<float> buffer (2, 512);
            juce::MidiBuffer midi;
            const auto hostMessage = juce::MidiMessage::noteOn (5, 99, (juce::uint8) 100);
            midi.addEvent (hostMessage, 10);

            processor.processBlock (buffer, midi);

            for (const auto metadata : midi)
            {
                const auto message = metadata.getMessage();
                const bool isTheHostMessage = message.getChannel() == 5 && message.getNoteNumber() == 99;
                expect (! isTheHostMessage);
            }
        }

        beginTest ("MIDI lands in the host MidiBuffer at expected sample offsets");
        {
            berlin::BerlinAudioProcessor processor;
            processor.prepareToPlay (44100.0, 1);

            juce::AudioBuffer<float> buffer (2, 1);
            juce::MidiBuffer midi;
            processor.processBlock (buffer, midi);

            // Step 0 is guaranteed active (SkipMaskGenerator's phase anchor) and the
            // transport's first boundary always lands at sample 0.
            expectEquals (midi.getNumEvents(), 1);

            for (const auto metadata : midi)
            {
                expectEquals (metadata.samplePosition, 0);
                expect (metadata.getMessage().isNoteOn());
            }
        }

        beginTest ("note-off precedes note-on at the same sample offset (loop wrap)");
        {
            berlin::BerlinAudioProcessor processor;
            processor.prepareToPlay (44100.0, 1);

            // samplesPerStep = 44100 * 60 / (120 * 4) = 5512.5; 17 boundaries
            // guarantee at least one full wrap back to step 0, where step 0's
            // guaranteed-active note-on collides with a still-pending note-off.
            constexpr int numSamples = 17 * 5513;
            juce::AudioBuffer<float> buffer (2, numSamples);
            juce::MidiBuffer midi;
            processor.processBlock (buffer, midi);

            std::map<int, std::vector<bool>> isNoteOnByTimestamp;   // emission order preserved per JUCE MidiBuffer::addEvent contract

            for (const auto metadata : midi)
            {
                const auto message = metadata.getMessage();
                if (message.isNoteOnOrOff())
                    isNoteOnByTimestamp[metadata.samplePosition].push_back (message.isNoteOn());
            }

            bool foundCollision = false;

            for (const auto& [timestamp, flags] : isNoteOnByTimestamp)
            {
                const auto offIt = std::find (flags.begin(), flags.end(), false);
                const auto onIt  = std::find (flags.begin(), flags.end(), true);

                if (offIt != flags.end() && onIt != flags.end())
                {
                    foundCollision = true;
                    expect (std::distance (flags.begin(), offIt) < std::distance (flags.begin(), onIt));
                }
            }

            expect (foundCollision);   // sanity: the scenario this test targets actually occurred
        }

        beginTest ("isBusesLayoutSupported rejects mono/5.1/with-input layouts, accepts stereo-out/no-input");
        {
            berlin::BerlinAudioProcessor processor;

            juce::AudioProcessor::BusesLayout monoOut;
            monoOut.outputBuses.add (juce::AudioChannelSet::mono());
            expect (! processor.isBusesLayoutSupported (monoOut));

            juce::AudioProcessor::BusesLayout surroundOut;
            surroundOut.outputBuses.add (juce::AudioChannelSet::create5point1());
            expect (! processor.isBusesLayoutSupported (surroundOut));

            juce::AudioProcessor::BusesLayout withInput;
            withInput.outputBuses.add (juce::AudioChannelSet::stereo());
            withInput.inputBuses.add (juce::AudioChannelSet::stereo());
            expect (! processor.isBusesLayoutSupported (withInput));

            juce::AudioProcessor::BusesLayout stereoOutNoInput;
            stereoOutNoInput.outputBuses.add (juce::AudioChannelSet::stereo());
            expect (processor.isBusesLayoutSupported (stereoOutNoInput));
        }

        beginTest ("Several hundred blocks with synth+FX enabled stay bounded and finite");
        {
            berlin::BerlinAudioProcessor processor;
            processor.setSynthEnabled (true);
            processor.setEffectsEnabled (true);
            processor.prepareToPlay (44100.0, 512);

            for (int i = 0; i < 500; ++i)
            {
                juce::AudioBuffer<float> buffer (2, 512);
                juce::MidiBuffer midi;
                processor.processBlock (buffer, midi);

                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                {
                    for (int s = 0; s < buffer.getNumSamples(); ++s)
                    {
                        const float sample = buffer.getSample (ch, s);
                        expect (std::isfinite (sample));
                        expect (std::abs (sample) <= 4.0f);
                    }
                }
            }
        }

        beginTest ("getTailLengthSeconds() stays finite and bounded when delayFeedback approaches 1.0");
        {
            // delayFeedback this close to 1.0 is reachable only via a saved/hand-edited
            // preset - no UI slider exposes this range today - but getTailLengthSeconds()
            // must never report an arbitrarily large or non-finite tail regardless of how
            // the patch arrived (vst3-au-plugin followup-fixes cleanup, obs #274).
            berlin::BerlinAudioProcessor processor;
            berlin::SynthPatch patch = processor.getPatch();
            patch.delayFeedback = 0.9999f;
            processor.setPatch (patch);

            const double tail = processor.getTailLengthSeconds();
            expect (std::isfinite (tail));
            expect (tail <= 60.0);
        }

        beginTest ("getTailLengthSeconds() with the default patch stays well under the clamp");
        {
            berlin::BerlinAudioProcessor processor;   // kDefaultPatch: delayFeedback == 0.3f
            const double tail = processor.getTailLengthSeconds();
            expect (std::isfinite (tail));
            expect (tail > 0.0);
            expect (tail < 60.0);
        }

        beginTest ("acceptsMidi/producesMidi/isMidiEffect report the locked contract");
        {
            // acceptsMidi() == true (D8, post-manual-gate correction): the VST3 SDK
            // requires an event input bus for any Instrument-category plugin, or hosts
            // reject the plugin outright (caught by Ableton Live's own load-time check).
            // The bus's existence does NOT change the ignore-content behavior -
            // "Pre-filled input MidiBuffer is cleared; host events never observed"
            // below still proves host MIDI input is never read/forwarded/acted upon.
            berlin::BerlinAudioProcessor processor;
            expect (processor.acceptsMidi());
            expect (processor.producesMidi());
            expect (! processor.isMidiEffect());
        }

        beginTest ("hasEditor()/createEditor() are headless-safe under BERLIN_HEADLESS");
        {
            berlin::BerlinAudioProcessor processor;
            expect (! processor.hasEditor());
            expect (processor.createEditor() == nullptr);
        }
    }
};

static BerlinAudioProcessorTests berlinAudioProcessorTests;
