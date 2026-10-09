import { describe, it, expect } from 'vitest';
import { accumulateWheel, classifyGesture, dragSteps, focusTarget, nextNote, noteAfter } from './stepEdit';

const SCALE = [48, 50, 51, 53];

describe('nextNote', () => {
  it('moves to the adjacent scale note in either direction', () => {
    expect(nextNote(SCALE, 50, 1)).toBe(51);
    expect(nextNote(SCALE, 50, -1)).toBe(48);
    expect(nextNote(SCALE, 51, -1)).toBe(50);
  });

  it('snaps an out-of-scale note by direction', () => {
    expect(nextNote([48, 50, 51], 49, 1)).toBe(50);
    expect(nextNote([48, 50, 51], 49, -1)).toBe(48);
  });

  it('stops at the ends without wrapping', () => {
    expect(nextNote(SCALE, 53, 1)).toBe(53);
    expect(nextNote(SCALE, 48, -1)).toBe(48);
    expect(nextNote(SCALE, 60, 1)).toBe(60);
  });
});

describe('noteAfter', () => {
  it('applies several scale steps, signed, clamped at the ends', () => {
    expect(noteAfter(SCALE, 48, 2)).toBe(51);
    expect(noteAfter(SCALE, 53, -3)).toBe(48);
    expect(noteAfter(SCALE, 50, 10)).toBe(53);
    expect(noteAfter(SCALE, 50, 0)).toBe(50);
  });
});

describe('classifyGesture', () => {
  it('is a click within the 4 px slop and a drag beyond it', () => {
    expect(classifyGesture(0)).toBe('click');
    expect(classifyGesture(4)).toBe('click');
    expect(classifyGesture(-4)).toBe('click');
    expect(classifyGesture(5)).toBe('drag');
    expect(classifyGesture(-5)).toBe('drag');
  });

  it('honours a custom slop', () => {
    expect(classifyGesture(7, 10)).toBe('click');
    expect(classifyGesture(11, 10)).toBe('drag');
  });
});

describe('dragSteps', () => {
  it('counts whole notes at 12 px each, upward is positive', () => {
    expect(dragSteps(100, 100)).toBe(0);
    expect(dragSteps(100, 89)).toBe(0);
    expect(dragSteps(100, 88)).toBe(1);
    expect(dragSteps(100, 64)).toBe(3);
    expect(dragSteps(100, 112)).toBe(-1);
    expect(dragSteps(100, 123)).toBe(-1);
  });

  it('honours a custom pixels-per-note', () => {
    expect(dragSteps(100, 80, 10)).toBe(2);
  });
});

describe('accumulateWheel', () => {
  it('one 100 px notch up is one note up (negative deltaY)', () => {
    expect(accumulateWheel(0, -100, 0)).toEqual({ acc: 0, steps: 1 });
    expect(accumulateWheel(0, 100, 0)).toEqual({ acc: 0, steps: -1 });
  });

  it('accumulates small deltas and carries the remainder', () => {
    const a = accumulateWheel(0, -40, 0);
    expect(a).toEqual({ acc: 0.4, steps: 0 });
    const b = accumulateWheel(a.acc, -40, 0);
    expect(b.steps).toBe(0);
    const c = accumulateWheel(b.acc, -40, 0);
    expect(c.steps).toBe(1);
    expect(c.acc).toBeCloseTo(0.2);
  });

  it('counts 3 lines per note in line mode', () => {
    expect(accumulateWheel(0, -3, 1)).toEqual({ acc: 0, steps: 1 });
    expect(accumulateWheel(0, 6, 1)).toEqual({ acc: 0, steps: -2 });
  });

  it('a big fast notch yields several notes', () => {
    expect(accumulateWheel(0, -250, 0).steps).toBe(2);
  });
});

describe('focusTarget', () => {
  it('moves with Left/Right, clamped to the row', () => {
    expect(focusTarget(5, 'ArrowRight', 16)).toBe(6);
    expect(focusTarget(5, 'ArrowLeft', 16)).toBe(4);
    expect(focusTarget(15, 'ArrowRight', 16)).toBe(15);
    expect(focusTarget(0, 'ArrowLeft', 16)).toBe(0);
  });

  it('jumps with Home and End', () => {
    expect(focusTarget(5, 'Home', 16)).toBe(0);
    expect(focusTarget(5, 'End', 16)).toBe(15);
  });

  it('ignores other keys', () => {
    expect(focusTarget(5, 'ArrowUp', 16)).toBeNull();
    expect(focusTarget(5, 'a', 16)).toBeNull();
  });
});
