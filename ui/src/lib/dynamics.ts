// The step dynamics (Franke-style accents) and gate lengths the UI offers. Each level maps to the integer the host stores per step:
// a MIDI velocity, or a gate length as a percent of the step.
export interface Level<N extends string> {
  name: N;
  value: number;
}

export type VelocityLevel = Level<'ghost' | 'soft' | 'normal' | 'accent'>;
export type GateLevel = Level<'staccato' | 'short' | 'long' | 'full'>;

export const VELOCITY_LEVELS: readonly VelocityLevel[] = [
  { name: 'ghost', value: 40 },
  { name: 'soft', value: 70 },
  { name: 'normal', value: 100 },
  { name: 'accent', value: 127 },
];

export const GATE_LEVELS: readonly GateLevel[] = [
  { name: 'staccato', value: 25 },
  { name: 'short', value: 50 },
  { name: 'long', value: 75 },
  { name: 'full', value: 100 },
];

// The level closest to a value; a tie goes to the higher level.
export function nearestLevel<N extends string>(levels: readonly Level<N>[], value: number): Level<N> {
  return levels.reduce((best, level) => (Math.abs(level.value - value) <= Math.abs(best.value - value) ? level : best));
}

// The value of the level after the nearest one; the last level wraps to the first.
export function nextLevel<N extends string>(levels: readonly Level<N>[], value: number): number {
  const index = levels.indexOf(nearestLevel(levels, value));
  return levels[(index + 1) % levels.length].value;
}
