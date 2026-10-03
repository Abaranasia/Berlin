import { describe, it, expect } from 'vitest';
import { LIMITS } from '../bridge/limits';
import { makeSkew, SKEWS } from './skew';

// JUCE NormalisableRange::setSkewForCentre: skew = log(0.5) / log((centre - start) / (end - start)),
// position = ((v - start) / (end - start)) ^ skew.
describe('makeSkew (JUCE skew from midpoint)', () => {
  it('maps the midpoint to position 0.5 and back', () => {
    const skew = makeSkew(20, 20000, 1000);
    expect(skew.toPosition(1000)).toBeCloseTo(0.5, 12);
    expect(skew.toValue(0.5)).toBeCloseTo(1000, 9);
  });

  it('endpoints are exact', () => {
    const skew = makeSkew(20, 20000, 1000);
    expect(skew.toPosition(20)).toBe(0);
    expect(skew.toPosition(20000)).toBe(1);
    expect(skew.toValue(0)).toBe(20);
    expect(skew.toValue(1)).toBe(20000);
  });

  it('a centred midpoint is linear (skew 1)', () => {
    const skew = makeSkew(0, 10, 5);
    expect(skew.toPosition(2.5)).toBeCloseTo(0.25, 12);
    expect(skew.toValue(0.75)).toBeCloseTo(7.5, 12);
  });

  it('matches the JUCE formula at a quarter position', () => {
    const skew = makeSkew(20, 20000, 1000);
    const exponent = Math.log(0.5) / Math.log((1000 - 20) / (20000 - 20));
    expect(skew.toPosition(5000)).toBeCloseTo(Math.pow((5000 - 20) / (20000 - 20), exponent), 12);
  });

  it('clamps out-of-range values and positions', () => {
    const skew = makeSkew(20, 20000, 1000);
    expect(skew.toPosition(5)).toBe(0);
    expect(skew.toPosition(99999)).toBe(1);
    expect(skew.toValue(-0.5)).toBe(20);
    expect(skew.toValue(1.5)).toBe(20000);
  });

  it('round-trips values across the range', () => {
    const skew = makeSkew(20, 20000, 1000);
    for (const v of [20, 21, 100, 999, 1000, 4321, 19999, 20000]) expect(skew.toValue(skew.toPosition(v))).toBeCloseTo(v, 6);
  });
});

describe('SKEWS (legacy midpoints, spec "Skewed Sliders")', () => {
  it.each([
    ['cutoffHz', 1000],
    ['resonance', 2],
    ['attack', 0.2],
    ['decay', 0.3],
    ['release', 0.5],
    ['lfoRateHz', 2],
  ] as const)('%s: position 0.5 is %d', (name, midpoint) => {
    expect(SKEWS[name].toValue(0.5)).toBeCloseTo(midpoint, 9);
    expect(SKEWS[name].toPosition(midpoint)).toBeCloseTo(0.5, 9);
  });

  it.each([
    ['cutoffHz', LIMITS.cutoffHz],
    ['resonance', LIMITS.resonance],
    ['attack', LIMITS.attackSeconds],
    ['decay', LIMITS.decaySeconds],
    ['release', LIMITS.releaseSeconds],
    ['lfoRateHz', LIMITS.lfoRateHz],
  ] as const)('%s: endpoints are the protocol limits and it round-trips', (name, range) => {
    expect(SKEWS[name].toValue(0)).toBe(range.min);
    expect(SKEWS[name].toValue(1)).toBe(range.max);
    const inside = range.min + (range.max - range.min) * 0.3;
    expect(SKEWS[name].toValue(SKEWS[name].toPosition(inside))).toBeCloseTo(inside, 6);
  });
});
