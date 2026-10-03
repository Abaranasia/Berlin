import { describe, it, expect } from 'vitest';
import { stepRow, STEP_COUNT } from './steps';

describe('stepRow', () => {
  it('has 16 entries', () => {
    expect(STEP_COUNT).toBe(16);
    expect(stepRow(0, true)).toHaveLength(16);
  });

  it('marks exactly the playhead step active while playing', () => {
    for (const step of [0, 7, 15]) {
      const row = stepRow(step, true);
      expect(row.filter(Boolean)).toHaveLength(1);
      expect(row[step]).toBe(true);
    }
  });

  it('has no active step when stopped', () => {
    expect(stepRow(5, false).some(Boolean)).toBe(false);
  });

  it.each([-1, 16, 99, 1.5, NaN])('has no active step for out-of-range step %s', (step) => {
    expect(stepRow(step, true).some(Boolean)).toBe(false);
  });
});
