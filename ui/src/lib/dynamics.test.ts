import { describe, it, expect } from 'vitest';
import { GATE_LEVELS, VELOCITY_LEVELS, nearestLevel, nextLevel } from './dynamics';

describe('velocity levels', () => {
  it('has the four Franke-style levels with their MIDI values', () => {
    expect(VELOCITY_LEVELS.map((l) => [l.name, l.value])).toEqual([['ghost', 40], ['soft', 70], ['normal', 100], ['accent', 127]]);
  });

  it('nearestLevel snaps to the closest level, ties to the higher one', () => {
    expect(nearestLevel(VELOCITY_LEVELS, 40).name).toBe('ghost');
    expect(nearestLevel(VELOCITY_LEVELS, 55).name).toBe('soft'); // 15 from 40 and 15 from 70: the tie goes up
    expect(nearestLevel(VELOCITY_LEVELS, 54).name).toBe('ghost');
    expect(nearestLevel(VELOCITY_LEVELS, 85).name).toBe('normal'); // 15 from 70 and 15 from 100
    expect(nearestLevel(VELOCITY_LEVELS, 113).name).toBe('normal');
    expect(nearestLevel(VELOCITY_LEVELS, 114).name).toBe('accent');
    expect(nearestLevel(VELOCITY_LEVELS, 1).name).toBe('ghost');
    expect(nearestLevel(VELOCITY_LEVELS, 127).name).toBe('accent');
  });

  it('nextLevel advances ghost, soft, normal, accent and wraps accent to ghost', () => {
    expect(nextLevel(VELOCITY_LEVELS, 40)).toBe(70);
    expect(nextLevel(VELOCITY_LEVELS, 70)).toBe(100);
    expect(nextLevel(VELOCITY_LEVELS, 100)).toBe(127);
    expect(nextLevel(VELOCITY_LEVELS, 127)).toBe(40);
  });

  it('nextLevel starts from the nearest level when the velocity is off the table', () => {
    expect(nextLevel(VELOCITY_LEVELS, 90)).toBe(127); // 90 is nearest to normal (100), so the next is accent
    expect(nextLevel(VELOCITY_LEVELS, 55)).toBe(100); // tie goes up to soft (70), so the next is normal
  });
});

describe('gate levels', () => {
  it('has the four gate lengths with their percentages', () => {
    expect(GATE_LEVELS.map((l) => [l.name, l.value])).toEqual([['staccato', 25], ['short', 50], ['long', 75], ['full', 100]]);
  });

  it('nearestLevel snaps a gate to the closest level, ties to the higher one', () => {
    const names = [1, 37, 38, 62, 63, 87, 88, 100].map((g) => nearestLevel(GATE_LEVELS, g).name);
    expect(names).toEqual(['staccato', 'staccato', 'short', 'short', 'long', 'long', 'full', 'full']);
    expect(nearestLevel(GATE_LEVELS, 60).value).toBe(50);
    expect(nearestLevel(GATE_LEVELS, 63).value).toBe(75);
    expect(nearestLevel(GATE_LEVELS, 62.5).name).toBe('long'); // the tie goes up
  });

  it('nextLevel advances staccato, short, long, full and wraps full to staccato', () => {
    expect([25, 50, 75, 100].map((g) => nextLevel(GATE_LEVELS, g))).toEqual([50, 75, 100, 25]);
  });

  it('nextLevel starts from the nearest level when the gate is off the table', () => {
    expect(nextLevel(GATE_LEVELS, 90)).toBe(25); // 90 is nearest to full, so it wraps to staccato
    expect(nextLevel(GATE_LEVELS, 38)).toBe(75); // 38 is nearest to short, so the next is long
  });
});
