/*
  ==============================================================================

   MidiFileWriter - out-of-line definitions (midi-file-output spec).

  ==============================================================================
*/

#include "MidiFileWriter.h"

#include <cmath>

namespace
{
    // Local clamps, NOT midi/MidiMessageBytes.h's - Decision 8's tier boundary:
    // Source/export/ depends on Source/core/ only.
    constexpr int clampChannel (int channel) noexcept
    {
        return channel < 1 ? 1 : (channel > 16 ? 16 : channel);
    }

    // Note-on velocity 0 is a note-off on the wire, so the floor is 1.
    constexpr int clampNoteOnVelocity (int velocity) noexcept
    {
        return velocity < 1 ? 1 : (velocity > 127 ? 127 : velocity);
    }
}

namespace berlin
{

MidiFileWriter::MidiFileWriter (int outputChannel) noexcept
    : channel (clampChannel (outputChannel))
{
}

void MidiFileWriter::buildFile (const MidiExportTimeline& timeline, double bpm, juce::MidiFile& destination) const
{
    destination.setTicksPerQuarterNote (timeline.ticksPerQuarterNote);

    juce::MidiMessageSequence sequence;

    const int microsecondsPerQuarterNote = (int) std::llround (60'000'000.0 / bpm);
    sequence.addEvent (juce::MidiMessage::tempoMetaEvent (microsecondsPerQuarterNote), 0.0);
    sequence.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0.0);

    // Translate 1:1, in the timeline's own order - NEVER sort()/updateMatchedPairs()
    // (Decision 7): addEvent() preserves insertion order for equal timestamps.
    for (const auto& event : timeline.events)
    {
        const auto message = event.noteOn
            ? juce::MidiMessage::noteOn (channel, event.note, (juce::uint8) clampNoteOnVelocity (event.velocity))
            : juce::MidiMessage::noteOff (channel, event.note, (juce::uint8) 0);

        sequence.addEvent (message, (double) event.tick);
    }

    sequence.addEvent (juce::MidiMessage::endOfTrack(), (double) timeline.endTick);

    destination.addTrack (sequence);
}

bool MidiFileWriter::writeTo (const MidiExportTimeline& timeline, double bpm, juce::OutputStream& out) const
{
    juce::MidiFile file;
    buildFile (timeline, bpm, file);
    return file.writeTo (out);
}

MidiFileWriteResult MidiFileWriter::writeToFile (const MidiExportTimeline& timeline,
                                                 double bpm,
                                                 const juce::File& destination) const
{
    if (timeline.ticksPerQuarterNote <= 0 || timeline.ticksPerStep <= 0)
        return MidiFileWriteResult::invalidTimeline;

    const juce::File parentDir = destination.getParentDirectory();

    if (parentDir == juce::File())
        return MidiFileWriteResult::pathUnavailable;

    if (! parentDir.exists() && parentDir.createDirectory().failed())
        return MidiFileWriteResult::pathUnavailable;

    juce::TemporaryFile temp (destination);
    bool writeOk = false;

    {
        juce::FileOutputStream stream (temp.getFile());

        if (stream.openedOk())
        {
            writeOk = writeTo (timeline, bpm, stream);
            stream.flush();
            writeOk = writeOk && ! stream.getStatus().failed();
        }
    }   // stream closed here - required before overwriteTargetFileWithTemporary()

    if (! writeOk)
        return MidiFileWriteResult::writeFailed;

    return temp.overwriteTargetFileWithTemporary() ? MidiFileWriteResult::ok
                                                    : MidiFileWriteResult::writeFailed;
}

} // namespace berlin
