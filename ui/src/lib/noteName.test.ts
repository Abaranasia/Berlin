import { describe, it, expect } from 'vitest';
import { noteName } from './noteName';

describe('noteName (MIDI 60 = C4)', () => {
  it.each([
    [60, 'C4'], [57, 'A3'], [0, 'C-1'], [127, 'G9'], [48, 'C3'], [61, 'C#4'],
  ])('%i is %s', (note, name) => {
    expect(noteName(note)).toBe(name);
  });
});
