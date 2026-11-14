import { describe, it, expect } from 'vitest';
import { VELOCITY_LEVELS, nearestLevel, nextLevel } from './dynamics';

describe('velocity levels', () => {
  it('has the four Franke-style levels with their MIDI values', () => {
    expect(VELOCITY_LEVELS.map((l) => [l.name, l.value])).toEqual([['ghost', 40], ['soft', 70], ['normal', 100], ['accent', 127]]);
  });

  it('nearestLevel snaps to the closest level, ties to the higher one', () => {
    expect(nearestLevel(40).name).toBe('ghost');
    expect(nearestLevel(55).name).toBe('soft'); // 15 from 40 and 15 from 70: the tie goes up
    expect(nearestLevel(54).name).toBe('ghost');
    expect(nearestLevel(85).name).toBe('normal'); // 15 from 70 and 15 from 100
    expect(nearestLevel(113).name).toBe('normal');
    expect(nearestLevel(114).name).toBe('accent');
    expect(nearestLevel(1).name).toBe('ghost');
    expect(nearestLevel(127).name).toBe('accent');
  });

  it('nextLevel advances ghost, soft, normal, accent and wraps accent to ghost', () => {
    expect(nextLevel(40)).toBe(70);
    expect(nextLevel(70)).toBe(100);
    expect(nextLevel(100)).toBe(127);
    expect(nextLevel(127)).toBe(40);
  });

  it('nextLevel starts from the nearest level when the velocity is off the table', () => {
    expect(nextLevel(90)).toBe(127); // 90 is nearest to normal (100), so the next is accent
    expect(nextLevel(55)).toBe(100); // tie goes up to soft (70), so the next is normal
  });
});
