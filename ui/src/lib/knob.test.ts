import { describe, it, expect } from 'vitest';
import { KNOB_TRAVEL_PX, dragToPosition, knobGeometry, positionOf, rangeAttrs, valueAt } from './knob';
import { SKEWS } from './skew';

describe('rangeAttrs', () => {
  it('linear: the real range, step and value', () => {
    expect(rangeAttrs(0.25, 0, 1, 0.05)).toEqual({ min: 0, max: 1, step: 0.05, value: 0.25 });
  });

  it('skewed: a 0..1 position range with step "any" and the value mapped to its position', () => {
    const skew = { toPosition: (v: number) => Math.sqrt(v / 100), toValue: (p: number) => p * p * 100 };
    expect(rangeAttrs(25, 0, 100, undefined, skew)).toEqual({ min: 0, max: 1, step: 'any', value: 0.5 });
  });
});

describe('positionOf and valueAt', () => {
  it('linear: round trip, endpoints and clamping', () => {
    expect(positionOf(5, 0, 10)).toBe(0.5);
    expect(valueAt(0.5, 0, 10)).toBe(5);
    expect(positionOf(-3, 0, 10)).toBe(0);
    expect(positionOf(30, 0, 10)).toBe(1);
    expect(valueAt(-1, 0, 10)).toBe(0);
    expect(valueAt(2, 0, 10)).toBe(10);
  });

  it('linear: snaps to the step from min, without float noise', () => {
    expect(valueAt(0.57, 0, 1, 0.01)).toBe(0.57);
    expect(valueAt(0.504, 0, 1, 0.01)).toBe(0.5);
    expect(valueAt(0.5, 2, 12, 4)).toBe(6); // position 0.5 is 7, the nearest step from min is 6
    expect(valueAt(1, 0, 1, 0.4)).toBe(1); // 1.2 would be the nearest step: clamped back into the range
  });

  it('skewed: the cutoff midpoint position maps to 1000 Hz and back', () => {
    const { cutoffHz } = SKEWS;
    expect(valueAt(0.5, 20, 20000, undefined, cutoffHz)).toBeCloseTo(1000, 3);
    expect(positionOf(1000, 20, 20000, cutoffHz)).toBeCloseTo(0.5, 6);
  });

  it('skewed: no step snapping, endpoints and clamping', () => {
    const { release } = SKEWS;
    expect(valueAt(0.3, 0.005, 8, 0.5, release)).toBe(release.toValue(0.3)); // a step is ignored when skewed
    expect(positionOf(99, 0.005, 8, release)).toBe(1);
    expect(valueAt(2, 0.005, 8, undefined, release)).toBeCloseTo(8, 6);
  });
});

describe('dragToPosition', () => {
  it('one full travel (200 px) up moves from min to max', () => {
    expect(KNOB_TRAVEL_PX).toBe(200);
    expect(valueAt(dragToPosition(0, 200), 0, 10)).toBe(10);
  });

  it('is relative to the start position and clamps at the top', () => {
    expect(dragToPosition(0.5, 50)).toBe(0.75);
    expect(dragToPosition(0.5, 100)).toBe(1);
    expect(dragToPosition(0.5, 400)).toBe(1);
  });

  it('dragging down lowers the position and clamps at the bottom', () => {
    expect(dragToPosition(0.5, -50)).toBe(0.25);
    expect(dragToPosition(0.1, -100)).toBe(0);
  });
});

describe('knobGeometry', () => {
  it('at 0 the pointer aims at the lower left and no value arc is drawn', () => {
    const { pointer, arc, track } = knobGeometry(0);
    expect(arc).toBe('');
    expect(track.startsWith('M 12.44 43.56 A 22 22 0 1 1 ')).toBe(true);
    const [x1, y1, x2, y2] = pointer;
    expect(x2).toBeLessThan(x1);
    expect(y2).toBeGreaterThan(y1);
  });

  it('at 1 the pointer aims at the lower right and the arc covers the whole track', () => {
    const { pointer, arc, track } = knobGeometry(1);
    expect(arc).toBe(track);
    const [x1, y1, x2, y2] = pointer;
    expect(x2).toBeGreaterThan(x1);
    expect(y2).toBeGreaterThan(y1);
  });

  it('at 0.5 the pointer is vertical and the arc ends at the top', () => {
    const { pointer, arc } = knobGeometry(0.5);
    const [x1, y1, x2, y2] = pointer;
    expect(x1).toBeCloseTo(x2, 6);
    expect(y2).toBeLessThan(y1);
    expect(arc.endsWith('28 6')).toBe(true);
    expect(arc).toContain(' 0 0 1 '); // a half sweep of 135 degrees is a small arc
  });
});
