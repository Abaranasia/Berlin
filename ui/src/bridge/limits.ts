// Hand-written parameter bounds. LIMITS mirrors the kMin*/kMax* constants in Source/synth/SynthPatch.h
// (key = constant name without the kMin/kMax prefix, first letter lowercased); scripts/limits-drift.test.mjs
// fails when they diverge. The generation and auto-evolve bounds below are not in that header.
export interface Range {
  min: number;
  max: number;
}

export const LIMITS: Record<string, Range> = {
  cutoffHz: { min: 20, max: 20000 },
  resonance: { min: 0.7071068, max: 8 },
  pulseWidth: { min: 0.05, max: 0.95 },
  attackSeconds: { min: 0.001, max: 4 },
  decaySeconds: { min: 0.001, max: 4 },
  sustain: { min: 0, max: 1 },
  releaseSeconds: { min: 0.005, max: 8 },
  lfoRateHz: { min: 0.05, max: 20 },
  lfoDepth: { min: 0, max: 1 },
  bpm: { min: 40, max: 240 },
  delayTimeSeconds: { min: 0, max: 3 },
  delayFeedback: { min: 0, max: 0.95 },
  delayMix: { min: 0, max: 1 },
  reverbRoomSize: { min: 0, max: 1 },
  reverbDamping: { min: 0, max: 1 },
  reverbWetLevel: { min: 0, max: 1 },
  reverbDryLevel: { min: 0, max: 1 },
  outputLevel: { min: 0, max: 1 },
};

// Source/generation/GenerationParams.h and the UiBridge.cpp auto-evolve clamp.
export const GENERATION_LIMITS: Record<string, Range> = {
  pulses: { min: 0, max: 16 },
  rotation: { min: 0, max: 15 },
  rootPitchClass: { min: 0, max: 11 },
  rangeLow: { min: 0, max: 127 },
  rangeHigh: { min: 0, max: 127 },
  stepProbability: { min: 0, max: 1 },
};
export const AUTO_EVOLVE_RATE: Range = { min: 1, max: 16 };
