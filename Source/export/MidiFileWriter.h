/*
  ==============================================================================

   MidiFileWriter - thin juce::MidiFile/juce::MidiMessageSequence wrapper that
   translates an ALREADY-CORRECT MidiExportTimeline into Standard MIDI File
   bytes (midi-file-output spec, roadmap Phase 6 / midi-export). No musical
   decisions of its own - all musical logic lives in MidiExportTimeline.

   JUCE-aware (juce_audio_basics via juce::MidiFile): Tests/BerlinTests.jucer
   stays juce_core-only by design, so no automated test can reach this file -
   manual-gate-only, same accepted tradeoff as Source/midi/MidiEventTranslator
   and Source/midi/MidiOutputSink in Phases 4-5. Registered in Berlin.jucer
   ONLY.

   Decision 8 (design.md): depends on Source/core/ only, NOT on
   midi/MidiMessageBytes.h - velocity and channel are injected via the
   constructor from MainComponent's single definitions, so exactly one
   definition of each constant exists project-wide. Note-off is emitted as
   juce::MidiMessage::noteOff (channel, note, (juce::uint8) 0) - a REAL 0x80
   status with release velocity 0, distinguishable from a velocity-0 note-on.

   Decision 7: addEvent() is called strictly in the timeline's own order
   (verified against the pinned JUCE checkout's juce_MidiMessageSequence.cpp:
   addEvent scans backward for the last existing event whose timestamp <= the
   new one and inserts immediately after it, which preserves insertion order
   for equal timestamps) - sort()/updateMatchedPairs() are NEVER called.

  ==============================================================================
*/

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

#include "export/MidiExportTimeline.h"

namespace berlin
{

enum class MidiFileWriteResult { ok, invalidTimeline, pathUnavailable, writeFailed };

class MidiFileWriter
{
public:
    MidiFileWriter (int outputChannel, int velocity) noexcept;   // both clamped, Decision 8

    // No musical decisions: translates an ALREADY-CORRECT timeline 1:1.
    // Track order: tempoMetaEvent(0), timeSignatureMetaEvent(4,4)@0, events, endOfTrack@endTick.
    void buildFile (const MidiExportTimeline& timeline, double bpm, juce::MidiFile& destination) const;

    bool writeTo (const MidiExportTimeline& timeline, double bpm, juce::OutputStream& out) const;

    // MESSAGE THREAD. Atomic via juce::TemporaryFile (Decision 4). Creates the parent
    // directory if needed. Never throws, never asserts.
    MidiFileWriteResult writeToFile (const MidiExportTimeline& timeline,
                                     double bpm,
                                     const juce::File& destination) const;

private:
    const int channel;    // [1, 16]
    const int velocity;   // [0, 127]
};

} // namespace berlin
