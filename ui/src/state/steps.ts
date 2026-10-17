// The grid wraps into rows of this many steps (the CSS columns); the sequence itself is 1..64 steps long.
export const ROW_SIZE = 16;

// One flag per step; only the playhead step is active, and only while playing. A playhead outside 0..count-1
// (the sequence just shrank, or no step yet) highlights nothing.
export function stepRow(playheadStep: number, playing: boolean, count: number): boolean[] {
  const valid = playing && Number.isInteger(playheadStep) && playheadStep >= 0 && playheadStep < count;
  return Array.from({ length: count }, (_, i) => valid && i === playheadStep);
}
