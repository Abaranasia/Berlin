import { describe, it, expect } from 'vitest';
import { isValidSeed, SEED_ERROR } from './seed';

describe('isValidSeed (spec "Seed Validation": -?digits)', () => {
  it.each(['0', '-5', '42', '9223372036854770000', '-9223372036854775808', '1234567890123456789'])('accepts %s', (seed) => {
    expect(isValidSeed(seed)).toBe(true);
  });

  it.each(['', '1.5', 'abc', '12a', '--1', '-', ' 5', '5 ', '+3', '1e3'])('rejects %j', (seed) => {
    expect(isValidSeed(seed)).toBe(false);
  });

  it('the error message is the legacy wording', () => {
    expect(SEED_ERROR).toBe('Seed must be a whole number.');
  });
});
