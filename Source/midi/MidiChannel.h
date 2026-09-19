/*
  ==============================================================================

   MidiChannel - the single fixed MIDI channel Berlin outputs and guards on
   (vst3-au-plugin followup-fixes cleanup).

   Before this file existed, BerlinAudioProcessor.h and MainComponent.h each
   declared their own `kMidiChannel = 1` constant: one fed
   MidiEventTranslator/MidiFileWriter (processor-owned), the other fed
   MainComponent's standalone-only MidiOutputSink CC123 panic guard. Both
   values happened to agree, but nothing enforced that - an edit to one site
   silently drifting from the other would have gone unnoticed. This header is
   the single source of truth both now reference instead.

   JUCE-free: a bare int constant, no dependency to pull in.

  ==============================================================================
*/

#pragma once

namespace berlin
{

constexpr int kMidiChannel = 1;

} // namespace berlin
