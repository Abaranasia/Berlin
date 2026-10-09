// JUCE NormalisableRange skew from a midpoint (setSkewForCentre): a slider position in 0..1 maps to
// start + (end - start) * position^(1 / skew), with skew = log(0.5) / log((centre - start) / (end - start)).
import { LIMITS } from '../bridge/limits';
import type { Skew } from '../components/controls/Slider';

const clamp01 = (v: number): number => Math.min(1, Math.max(0, v));

export function makeSkew(min: number, max: number, midpoint: number): Skew {
  const exponent = Math.log(0.5) / Math.log((midpoint - min) / (max - min));
  return {
    toPosition: (value) => Math.pow(clamp01((value - min) / (max - min)), exponent),
    toValue: (position) => min + (max - min) * Math.pow(clamp01(position), 1 / exponent),
  };
}

// Legacy slider midpoints (spec "Skewed Sliders"); endpoints are the protocol limits.
export const SKEWS = {
  cutoffHz: makeSkew(LIMITS.cutoffHz.min, LIMITS.cutoffHz.max, 1000),
  resonance: makeSkew(LIMITS.resonance.min, LIMITS.resonance.max, 2),
  attack: makeSkew(LIMITS.attackSeconds.min, LIMITS.attackSeconds.max, 0.2),
  decay: makeSkew(LIMITS.decaySeconds.min, LIMITS.decaySeconds.max, 0.3),
  release: makeSkew(LIMITS.releaseSeconds.min, LIMITS.releaseSeconds.max, 0.5),
  lfoRateHz: makeSkew(LIMITS.lfoRateHz.min, LIMITS.lfoRateHz.max, 2),
} as const;
