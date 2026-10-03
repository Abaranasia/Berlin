import { describe, it, expect } from 'vitest';
import fs from 'node:fs';
import { LIMITS } from '../src/bridge/limits';

const header = fs.readFileSync(new URL('../../Source/synth/SynthPatch.h', import.meta.url), 'utf8');

// `kMinFoo = 1.0f, kMaxFoo = 2.0f` pairs; the one irregular name is the delay time upper bound.
const value = (name) => new RegExp(name + /\s*=\s*(-?[0-9]*\.?[0-9]+)f?/.source).exec(header)?.[1];

const cppLimits = () => {
  const out = {};
  for (const [, name] of header.matchAll(/\bkMin(\w+)\s*=/g)) {
    const maxName = name === 'DelayTimeSeconds' ? 'DelaySeconds' : name;
    out[name[0].toLowerCase() + name.slice(1)] = { min: Number(value('kMin' + name)), max: Number(value('kMax' + maxName)) };
  }
  return out;
};

describe('limits.ts mirrors SynthPatch.h', () => {
  const expected = cppLimits();

  it('parsed a plausible set of constants (guards a broken regex)', () => {
    expect(Object.keys(expected)).toHaveLength(18);
    expect(expected.bpm).toEqual({ min: 40, max: 240 });
  });

  it('has exactly the same keys', () => {
    expect(Object.keys(LIMITS).sort()).toEqual(Object.keys(expected).sort());
  });

  it.each(Object.keys(expected))('%s min/max are equal', (key) => {
    expect(LIMITS[key]).toEqual(expected[key]);
  });
});
