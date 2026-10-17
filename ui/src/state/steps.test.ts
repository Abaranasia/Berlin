import { describe, it, expect } from 'vitest';
import { stepRow, ROW_SIZE } from './steps';

describe('stepRow', () => {
  it('has one entry per step and rows are 16 wide', () => {
    expect(ROW_SIZE).toBe(16);
    expect(stepRow(0, true, 16)).toHaveLength(16);
    expect(stepRow(0, true, 24)).toHaveLength(24);
    expect(stepRow(0, true, 1)).toHaveLength(1);
  });

  it('marks exactly the playhead step active while playing', () => {
    for (const [step, count] of [[0, 16], [7, 16], [15, 16], [23, 24], [63, 64]]) {
      const row = stepRow(step, true, count);
      expect(row.filter(Boolean)).toHaveLength(1);
      expect(row[step]).toBe(true);
    }
  });

  it('has no active step when stopped', () => {
    expect(stepRow(5, false, 16).some(Boolean)).toBe(false);
  });

  it.each([-1, 16, 99, 1.5, NaN])('has no active step for out-of-range step %s on 16 steps', (step) => {
    expect(stepRow(step, true, 16).some(Boolean)).toBe(false);
  });

  it('a playhead past the end of a shorter sequence highlights nothing (length shrank)', () => {
    expect(stepRow(20, true, 8).some(Boolean)).toBe(false);
    expect(stepRow(20, true, 24)[20]).toBe(true);
  });
});
