export const ROOT_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];

// MIDI note number to a name with the octave, using the MIDI 60 = C4 convention.
export function noteName(note: number): string {
  return `${ROOT_NAMES[((note % 12) + 12) % 12]}${Math.floor(note / 12) - 1}`;
}
