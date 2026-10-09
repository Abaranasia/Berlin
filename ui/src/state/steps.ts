export const STEP_COUNT = 16;

// One flag per step; only the playhead step is active, and only while playing.
export function stepRow(playheadStep: number, playing: boolean): boolean[] {
  const valid = playing && Number.isInteger(playheadStep) && playheadStep >= 0 && playheadStep < STEP_COUNT;
  return Array.from({ length: STEP_COUNT }, (_, i) => valid && i === playheadStep);
}
