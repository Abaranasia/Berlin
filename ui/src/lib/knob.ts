// Pure math shared by Slider and Knob: range attributes, value <-> position mapping, drag and SVG geometry.
import type { Skew } from '../components/controls/Slider';

export const KNOB_TRAVEL_PX = 200; // a vertical drag of this many pixels covers the full range

const clamp01 = (v: number): number => Math.min(1, Math.max(0, v));
const num = (n: number): string => String(Number(n.toFixed(2)));

// A skewed control moves over a 0..1 position; a linear one over its real range.
export function rangeAttrs(value: number, min: number, max: number, step?: number, skew?: Skew) {
  return skew ? { min: 0, max: 1, step: 'any' as const, value: skew.toPosition(value) } : { min, max, step, value };
}

export function positionOf(value: number, min: number, max: number, skew?: Skew): number {
  return clamp01(skew ? skew.toPosition(value) : (value - min) / (max - min));
}

// Linear values snap to `step` counted from `min` (and never leave the range); skewed values are continuous.
export function valueAt(position: number, min: number, max: number, step?: number, skew?: Skew): number {
  const p = clamp01(position);
  if (skew) return skew.toValue(p);
  const raw = min + (max - min) * p;
  if (!step) return raw;
  const snapped = min + Math.round((raw - min) / step) * step;
  return Math.min(max, Math.max(min, Number(snapped.toFixed(10))));
}

export const dragToPosition = (startPos: number, upPx: number): number => clamp01(startPos + upPx / KNOB_TRAVEL_PX);

const CENTER = 28;
const RADIUS = 22;
const START_DEG = 135;
const SWEEP_DEG = 270;
const POINTER_INNER = 5;
const POINTER_OUTER = 13;

const pointAt = (deg: number, radius: number): [number, number] => [
  CENTER + radius * Math.cos((deg * Math.PI) / 180),
  CENTER + radius * Math.sin((deg * Math.PI) / 180),
];

const arcPath = (sweepDeg: number): string => {
  if (sweepDeg <= 0) return '';
  const [x0, y0] = pointAt(START_DEG, RADIUS);
  const [x1, y1] = pointAt(START_DEG + sweepDeg, RADIUS);
  return `M ${num(x0)} ${num(y0)} A ${RADIUS} ${RADIUS} 0 ${sweepDeg > 180 ? 1 : 0} 1 ${num(x1)} ${num(y1)}`;
};

// 56x56 viewBox: a 270 degree track opening at the bottom, the value arc over it and a pointer line.
export function knobGeometry(position: number): { track: string; arc: string; pointer: [number, number, number, number] } {
  const p = clamp01(position);
  const [x1, y1] = pointAt(START_DEG + SWEEP_DEG * p, POINTER_INNER);
  const [x2, y2] = pointAt(START_DEG + SWEEP_DEG * p, POINTER_OUTER);
  return { track: arcPath(SWEEP_DEG), arc: arcPath(SWEEP_DEG * p), pointer: [x1, y1, x2, y2] };
}
