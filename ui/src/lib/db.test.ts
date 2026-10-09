import { describe, it, expect } from 'vitest';
import { formatDb } from './db';

describe('formatDb', () => {
  it('shows minus infinity for a zero level', () => {
    expect(formatDb(0)).toBe('−∞ dB');
  });
  it('shows minus infinity for a negative level', () => {
    expect(formatDb(-0.5)).toBe('−∞ dB');
  });
  it('shows 0.0 dB at unity', () => {
    expect(formatDb(1)).toBe('0.0 dB');
  });
  it('shows -6.0 dB at half level', () => {
    expect(formatDb(0.5)).toBe('-6.0 dB');
  });
  it('shows -80.0 dB at 0.0001', () => {
    expect(formatDb(0.0001)).toBe('-80.0 dB');
  });
});
