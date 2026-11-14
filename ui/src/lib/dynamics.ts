// The four step dynamics (Franke-style accents) the UI offers. Each maps to a MIDI velocity the host stores per step.
export interface VelocityLevel {
  name: 'ghost' | 'soft' | 'normal' | 'accent';
  value: number;
}

export const VELOCITY_LEVELS: readonly VelocityLevel[] = [
  { name: 'ghost', value: 40 },
  { name: 'soft', value: 70 },
  { name: 'normal', value: 100 },
  { name: 'accent', value: 127 },
];

// The level closest to a velocity; a tie goes to the higher level.
export function nearestLevel(velocity: number): VelocityLevel {
  return VELOCITY_LEVELS.reduce((best, level) => (Math.abs(level.value - velocity) <= Math.abs(best.value - velocity) ? level : best));
}

// The velocity of the level after the nearest one; accent wraps to ghost.
export function nextLevel(velocity: number): number {
  const index = VELOCITY_LEVELS.indexOf(nearestLevel(velocity));
  return VELOCITY_LEVELS[(index + 1) % VELOCITY_LEVELS.length].value;
}
