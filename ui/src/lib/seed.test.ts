import { describe, it, expect } from 'vitest';
import { isValidSeed, normalizeSeed, SEED_ERROR } from './seed';

describe('isValidSeed (spec "Seed Validation": -?digits)', () => {
  it.each(['0', '-5', '42', '9223372036854770000', '-9223372036854775808', '1234567890123456789'])('accepts %s', (seed) => {
    expect(isValidSeed(seed)).toBe(true);
  });

  it.each(['', '1.5', 'abc', '12a', '--1', '-', '+3', '1e3', '   ', '- 5'])('rejects %j', (seed) => {
    expect(isValidSeed(seed)).toBe(false);
  });

  it.each([[' 5', '5'], ['5 ', '5'], ['  -42	', '-42'], ['7', '7']])('normalizeSeed trims %j to %j', (raw, trimmed) => {
    expect(normalizeSeed(raw)).toBe(trimmed);
    expect(isValidSeed(normalizeSeed(raw))).toBe(true);
  });

  it('the error message is the legacy wording', () => {
    expect(SEED_ERROR).toBe('Seed must be a whole number.');
  });
});
