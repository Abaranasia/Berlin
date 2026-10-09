// Pure helpers for editing steps: scale-aware note stepping, gesture maths and keyboard focus movement.

export const DRAG_SLOP_PX = 4; // movement up to this far still counts as a click
export const DRAG_PX_PER_NOTE = 12;
const WHEEL_PX_PER_NOTE = 100;
const WHEEL_LINES_PER_NOTE = 3;
const WHEEL_PAGES_PER_NOTE = 1;

// The adjacent scale note above (dir 1) or below (dir -1); an out-of-scale note snaps by direction.
// At the ends it stays where it is (no wrap). `scaleNotes` is ascending.
export function nextNote(scaleNotes: readonly number[], current: number, dir: 1 | -1): number {
  const target = dir === 1 ? scaleNotes.find((n) => n > current) : scaleNotes.filter((n) => n < current).at(-1);
  return target ?? current;
}

// `steps` scale notes from `start` (positive = up), stopping at the ends.
export function noteAfter(scaleNotes: readonly number[], start: number, steps: number): number {
  const dir = steps < 0 ? -1 : 1;
  let note = start;
  for (let i = 0; i < Math.abs(steps); i += 1) note = nextNote(scaleNotes, note, dir);
  return note;
}

export const classifyGesture = (dy: number, slopPx: number = DRAG_SLOP_PX): 'click' | 'drag' =>
  Math.abs(dy) > slopPx ? 'drag' : 'click';

// Whole notes covered by a vertical drag from `startY` to `y` (screen y grows downward, so up is positive).
export const dragSteps = (startY: number, y: number, pxPerNote: number = DRAG_PX_PER_NOTE): number =>
  Math.trunc((startY - y) / pxPerNote) || 0;

// Wheel deltas become notes: 100 px, 3 lines or 1 page per note. The fractional remainder carries in `acc`;
// scrolling up (negative deltaY) means a higher note.
export function accumulateWheel(acc: number, deltaY: number, deltaMode: number): { acc: number; steps: number } {
  const unit = deltaMode === 1 ? WHEEL_LINES_PER_NOTE : deltaMode === 2 ? WHEEL_PAGES_PER_NOTE : WHEEL_PX_PER_NOTE;
  const total = acc - deltaY / unit;
  const steps = Math.trunc(total) || 0;
  return { acc: total - steps || 0, steps };
}

// The cell that takes focus for a navigation key, or null when the key is not one.
export function focusTarget(index: number, key: string, count: number): number | null {
  switch (key) {
    case 'ArrowLeft': return Math.max(0, index - 1);
    case 'ArrowRight': return Math.min(count - 1, index + 1);
    case 'Home': return 0;
    case 'End': return count - 1;
    default: return null;
  }
}
