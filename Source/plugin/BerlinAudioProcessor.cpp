/*
  ==============================================================================

   BerlinAudioProcessor - out-of-line definitions (roadmap Phase 11 /
   vst3-au-plugin, plugin-host-integration + plugin-state-recall +
   realtime-audio-wiring + midi-output-dispatch specs, design.md D1-D6 +
   Interfaces/Contracts).

  ==============================================================================
*/

#include "BerlinAudioProcessor.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "generation/MutationEngine.h"
#include "generation/DeterministicRandom.h"
#include "generation/SequenceBuilder.h"

namespace berlin
{

BerlinAudioProcessor::BerlinAudioProcessor (juce::File presetDirectory)
    : juce::AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      currentSeed (kDefaultSeed),
      currentSequence (buildSeededSequence (currentSeed, generationParams.mode, generationParams.pulses,
                                             generationParams.rotation, generationParams.stepProbability)),
      sequenceSeed (kDefaultSeed),
      player (currentSequence, Transport (kBpm, kStepsPerBeat)),
      midiTranslator (kMidiChannel),
      presetManager (std::move (presetDirectory))
{
    player.start();
}

BerlinAudioProcessor::~BerlinAudioProcessor()
{
    stopTimer();   // MUST be first (matches MainComponent's precedent): the timer callback touches `this`
}

//==============================================================================
void BerlinAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    player.prepare (sampleRate);

    const juce::dsp::ProcessSpec spec { sampleRate,
                                        static_cast<juce::uint32> (samplesPerBlock),
                                        static_cast<juce::uint32> (2) };
    synth.prepare (spec);
    pushPatchToSynth();   // re-seed after every (re)prepare, not just kDefaultPatch (mirrors MainComponent Decision 6)
}

void BerlinAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    const juce::ScopedNoDenormals noDenormals;

    buffer.clear();
    midiMessages.clear();   // host MIDI input is ignored, not consumed (locked decision)

    player.process (buffer.getNumSamples(), blockEvents);
    midiTranslator.translate (blockEvents, midiMessages);
    synth.render (blockEvents, buffer, 0, buffer.getNumSamples());
}

void BerlinAudioProcessor::releaseResources()
{
    juce::MidiBuffer discard;
    flushPendingNoteOff (discard);   // discard pending note-off tracking; plugin has no device to send it to
    synth.reset();
}

bool BerlinAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (! layouts.inputBuses.isEmpty())
        return false;   // no AUDIO input bus (unrelated to the MIDI/event input bus declared
                         // via acceptsMidi()==true + pluginWantsMidiIn, D8 - that bus's content
                         // is still ignored via processBlock's unconditional midiMessages.clear())

    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

double BerlinAudioProcessor::getTailLengthSeconds() const
{
    // SynthEffects exposes no tail-length query (Source/synth/* stays
    // unchanged/reused verbatim per design.md's File Changes table), so this
    // reads the owned SynthPatch fields themselves rather than guessing a
    // fixed constant (design.md's open item): release time, plus enough
    // delay-feedback repeats to decay below audibility, plus a bounded
    // reverb-roomSize-scaled allowance.
    double tail = static_cast<double> (currentPatch.release);

    if (currentPatch.delayFeedback > 0.0f && currentPatch.delayFeedback < 1.0f)
    {
        const double repeatsNeeded = std::log (0.001) / std::log (static_cast<double> (currentPatch.delayFeedback));
        tail += repeatsNeeded * static_cast<double> (currentPatch.delayTimeSeconds);
    }

    tail += 1.0 + 4.0 * static_cast<double> (currentPatch.reverbRoomSize);

    // delayFeedback isn't exposed via any UI slider today - the full [0,1)
    // range SynthPatch allows is only reachable through a saved/hand-edited
    // preset - so a value near 1.0 would otherwise drive repeatsNeeded above,
    // and therefore the reported tail, toward an arbitrarily large value.
    // 60s is a generous ceiling no legitimate patch's release+delay+reverb
    // decay could plausibly need (the longest configurable release is
    // kMaxReleaseSeconds == 8s, SynthPatch.h) while still comfortably
    // covering any host that pre-allocates a tail buffer from this value.
    constexpr double kMaxTailLengthSeconds = 60.0;
    return std::min (tail, kMaxTailLengthSeconds);
}

void BerlinAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    Preset preset;
    preset.patch = currentPatch;
    preset.seed  = currentSeed;

    if (auto xml = PresetManager::toValueTree (preset).createXml())
        copyXmlToBinary (*xml, destData);
}

void BerlinAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // MESSAGE THREAD ONLY (regenerate() below allocates - see this method's
    // declaration comment in BerlinAudioProcessor.h). JUCE's own VST3 wrapper
    // only debug-asserts that hosts call this from the UI/message thread, and
    // does not enforce it in release builds, so a misbehaving host could
    // theoretically race this against the auto-evolve Timer's mutate() call.
    // Debug-only fast-fail, mirroring JUCE's own assertion - not a runtime
    // lock; a real fix would be a bigger architectural change (out of scope
    // here, vst3-au-plugin followup-fixes cleanup).
    JUCE_ASSERT_MESSAGE_THREAD

    // Threat Matrix (design.md) / tasks.md 3.4: garbage/truncated/empty data,
    // or a structurally-invalid tree, leaves state UNTOUCHED - no crash, no
    // fallback-to-default mutation. (Reconciles design.md's Threat Matrix
    // table over one plugin-state-recall spec scenario's literal "falls back
    // to default patch and seed" wording; tasks.md's RED directive for this
    // exact row says "leaves state untouched".)
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        Preset preset;
        if (PresetManager::fromValueTree (juce::ValueTree::fromXml (*xml), preset) == PresetResult::ok)
        {
            setPatch (preset.patch);
            setSeed (preset.seed);
            regenerate (false);   // re-derive the sequence from the restored seed; does NOT replay mutation history (D4)
            sendChangeMessage();
        }
    }
}

//==============================================================================
bool BerlinAudioProcessor::regenerate (bool drawNewSeed)
{
    if (drawNewSeed && ! generationParams.lockSeed)
        currentSeed = juce::Random::getSystemRandom().nextInt64();

    auto next = buildSeededSequence (currentSeed, generationParams.mode, generationParams.pulses,
                                      generationParams.rotation, generationParams.stepProbability);

    // player.publishSequence swaps THROUGH its argument: on success `forPlayer`
    // ends up holding the stale, now-superseded buffer, not the freshly built
    // one. Publish a COPY so `next` stays intact as what `currentSequence`
    // must become.
    auto forPlayer = next;

    if (! player.publishSequence (forPlayer))
        return false;   // busy: state unchanged, retry reproduces the same candidate

    currentSequence = std::move (next);
    sequenceSeed = currentSeed;
    mutationCount = 0;
    return true;
}

bool BerlinAudioProcessor::mutate()
{
    DeterministicRandom rng (MutationEngine::mutationSeed (sequenceSeed, mutationCount + 1));
    auto next = MutationEngine::applyRandomTransform (currentSequence, rng);

    auto forPlayer = next;

    if (! player.publishSequence (forPlayer))
        return false;   // busy: currentSequence AND mutationCount unchanged, retry reproduces the same candidate

    currentSequence = std::move (next);
    ++mutationCount;
    return true;
}

PresetResult BerlinAudioProcessor::save (const juce::String& name)
{
    Preset preset;
    preset.name  = name;
    preset.patch = currentPatch;
    preset.seed  = currentSeed;
    return presetManager.save (preset);   // unconditional; editor owns the overwrite prompt (D2)
}

PresetResult BerlinAudioProcessor::loadPreset (const juce::String& name)
{
    Preset preset;
    const auto result = presetManager.load (name, preset);

    if (result != PresetResult::ok)
        return result;   // synth and sequence left untouched

    setPatch (preset.patch);
    setSeed (preset.seed);
    regenerate (false);   // drawNewSeed=false: the preset's saved seed applies even if Lock Seed is on

    return PresetResult::ok;
}

bool BerlinAudioProcessor::presetExists (const juce::String& name) const
{
    const auto file = presetManager.fileForName (name);
    return file != juce::File() && file.existsAsFile();
}

juce::StringArray BerlinAudioProcessor::listPresetNames() const
{
    return presetManager.listPresetNames();
}

MidiFileWriteResult BerlinAudioProcessor::exportMidiTo (const juce::File& destination) const
{
    MidiExportTimeline timeline;
    const auto status = buildMidiExportTimeline (currentSequence, kStepsPerBeat, kExportRepeats, timeline);

    if (status != MidiExportStatus::ok)
        return MidiFileWriteResult::invalidTimeline;

    const MidiFileWriter writer (kMidiChannel, MidiEventTranslator::kNoteVelocity);
    return writer.writeToFile (timeline, kBpm, destination);
}

//==============================================================================
void BerlinAudioProcessor::setPatch (const SynthPatch& patch)
{
    currentPatch = patch;
    pushPatchToSynth();
}

void BerlinAudioProcessor::pushPatchToSynth()
{
    synth.setWaveform       (currentPatch.waveform);
    synth.setCutoffHz       (currentPatch.cutoffHz);
    synth.setResonance      (currentPatch.resonance);
    synth.setPulseWidth     (currentPatch.pulseWidth);
    synth.setAttackSeconds  (currentPatch.attack);
    synth.setDecaySeconds   (currentPatch.decay);
    synth.setSustain        (currentPatch.sustain);
    synth.setReleaseSeconds (currentPatch.release);
    synth.setLfoRateHz      (currentPatch.lfoRateHz);
    synth.setLfoDepth       (currentPatch.lfoDepth);
    synth.setLfoDestination (currentPatch.lfoDestination);
}

void BerlinAudioProcessor::setAutoEvolveEnabled (bool shouldBeEnabled)
{
    autoEvolveEnabled = shouldBeEnabled;

    if (shouldBeEnabled)
    {
        autoEvolveSchedule.reset (player.getLoopCount());
        startTimerHz (60);
    }
    else
    {
        stopTimer();
    }
}

void BerlinAudioProcessor::timerCallback()
{
    const int loops = player.getLoopCount();
    autoEvolveSchedule.checkDue (loops, autoEvolveRate);

    if (! autoEvolveSchedule.isDue())
        return;

    // Gate on isPublishPending() BEFORE calling mutate() at all, so a busy
    // publish never reaches mutate()'s own "busy" rejection path for an
    // automatic attempt the user did not initiate - silently retry next tick.
    if (player.isPublishPending())
        return;

    const int before = mutationCount;

    if (! mutate() || mutationCount == before)
        return;   // rejected - stay due, retry next tick

    autoEvolveSchedule.markMutationSucceeded();
    sendChangeMessage();   // D3: broadcast after a successful auto-evolve mutate()
}

bool BerlinAudioProcessor::flushPendingNoteOff (juce::MidiBuffer& out) noexcept
{
    if (! player.flushPendingNoteOff (blockEvents))
        return false;

    midiTranslator.translate (blockEvents, out);
    return true;
}

} // namespace berlin

// createEditor()/hasEditor() (D5): deliberately defined OUTSIDE the
// `namespace berlin { ... }` block above. `#include`-ing
// BerlinAudioProcessorEditor.h (which itself opens `namespace berlin { ... }`
// and includes the full juce_audio_processors/juce_gui_basics headers) from
// INSIDE an already-open `namespace berlin { }` would nest every JUCE
// declaration those headers bring in under `berlin::juce::...` instead of the
// real top-level `::juce::...` - breaking name lookup for every other
// translation unit that already saw the real juce_gui_basics headers at
// global scope. Keeping the #include (and these two definitions) at file
// scope, fully-qualified, avoids that entirely.
#if BERLIN_HEADLESS
juce::AudioProcessorEditor* berlin::BerlinAudioProcessor::createEditor() { return nullptr; }
bool berlin::BerlinAudioProcessor::hasEditor() const { return false; }
#else
#include "plugin/BerlinAudioProcessorEditor.h"
juce::AudioProcessorEditor* berlin::BerlinAudioProcessor::createEditor() { return new berlin::BerlinAudioProcessorEditor (*this); }
bool berlin::BerlinAudioProcessor::hasEditor() const { return true; }
#endif
