import { memo, useEffect, useRef, useState, type KeyboardEvent, type PointerEvent } from 'react';
import { accumulateWheel, classifyGesture, dragSteps, focusTarget, noteAfter } from '../lib/stepEdit';
import { noteName } from '../lib/noteName';
import { STEP_COUNT, stepRow } from '../state/steps';
import { sendStep } from '../store/actions';
import { useStore, useStoreApi } from '../store/context';
import { shown, type Store } from '../store/store';

interface StepValue {
  note: number;
  active: boolean;
}

// What the step shows right now: the optimistic value while an edit is unsettled, else the engine's step.
// Read at event time (not from render props) so chained presses and drags accumulate on the shown value.
const shownStep = (store: Store, index: number): StepValue | undefined => {
  const state = store.getState();
  const engineStep = state.engine?.steps[index];
  return engineStep && shown(state, `step.${index}`, engineStep);
};

const scaleNotes = (store: Store): readonly number[] => store.getState().engine?.scaleNotes ?? [];

function stepNote(store: Store, index: number, steps: number): void {
  const current = shownStep(store, index);
  if (!current) return;
  const note = noteAfter(scaleNotes(store), current.note, steps);
  if (note !== current.note) sendStep(store, index, note, current.active);
}

function toggleMute(store: Store, index: number): void {
  const current = shownStep(store, index);
  if (current) sendStep(store, index, current.note, !current.active);
}

interface StepCellProps {
  index: number;
  active: boolean; // the playhead is on this step
  note: number | undefined;
  gate: boolean | undefined;
  focusable: boolean; // the roving tabindex stop
  onFocusCell(index: number): void;
}

interface Drag {
  startY: number;
  startNote: number;
  dragged: boolean;
}

// Styled through the data-* attributes (see components.css); a muted step keeps its note, shown dimmed.
// Click toggles mute, a vertical drag changes the note; the keyboard follows the roving-tabindex button pattern.
const StepCell = memo(function StepCell({ index, active, note, gate, focusable, onFocusCell }: StepCellProps) {
  const store = useStoreApi();
  const drag = useRef<Drag | null>(null);

  const onPointerDown = (e: PointerEvent<HTMLDivElement>) => {
    const current = shownStep(store, index);
    if (e.button !== 0 || !current) return;
    drag.current = { startY: e.clientY, startNote: current.note, dragged: false };
    e.currentTarget.setPointerCapture?.(e.pointerId); // optional: jsdom has no pointer capture
  };
  const onPointerMove = (e: PointerEvent<HTMLDivElement>) => {
    const d = drag.current;
    if (!d || (!d.dragged && classifyGesture(e.clientY - d.startY) === 'click')) return;
    d.dragged = true;
    const current = shownStep(store, index);
    const target = noteAfter(scaleNotes(store), d.startNote, dragSteps(d.startY, e.clientY));
    if (current && target !== current.note) sendStep(store, index, target, current.active);
  };
  const onPointerUp = () => {
    const d = drag.current;
    drag.current = null;
    if (d && !d.dragged) toggleMute(store, index);
  };
  const onKeyDown = (e: KeyboardEvent<HTMLDivElement>) => {
    const to = focusTarget(index, e.key, STEP_COUNT);
    if (to !== null) {
      e.preventDefault();
      onFocusCell(to);
      e.currentTarget.parentElement?.querySelector<HTMLElement>(`[data-step="${to}"]`)?.focus();
    } else if (e.key === ' ') {
      e.preventDefault();
      toggleMute(store, index);
    } else if (e.key === 'ArrowUp' || e.key === 'ArrowDown') {
      e.preventDefault();
      stepNote(store, index, e.key === 'ArrowUp' ? 1 : -1);
    }
  };

  return (
    <div
      className="step"
      role="button"
      tabIndex={focusable ? 0 : -1}
      aria-pressed={gate === true}
      aria-label={note === undefined ? `Step ${index + 1}` : `Step ${index + 1}, ${noteName(note)}, ${gate ? 'on' : 'off'}`}
      data-step={index}
      data-active={active}
      data-note={note}
      data-gate={gate}
      onFocus={() => onFocusCell(index)}
      onPointerDown={onPointerDown}
      onPointerMove={onPointerMove}
      onPointerUp={onPointerUp}
      onPointerCancel={() => (drag.current = null)}
      onKeyDown={onKeyDown}
    >
      <span className="step-num">{index + 1}</span>
      <span className="led" data-on={gate === true} />
      <span className="step-note">{note !== undefined ? noteName(note) : '·'}</span>
    </div>
  );
});

// The pattern row: the playhead highlight plus each step's shown note and gate, editable in place.
export function StepView() {
  const store = useStoreApi();
  const playhead = useStore((s) => s.playhead);
  const steps = useStore((s) => s.engine?.steps);
  const overlay = useStore((s) => s.overlay);
  const edited = useStore((s) => s.engine?.edited === true);
  const [focused, setFocused] = useState(0);
  const grid = useRef<HTMLDivElement>(null);
  const wheel = useRef({ index: -1, acc: 0 });
  const row = stepRow(playhead.step, playhead.playing);

  // React's onWheel is passive and cannot preventDefault, so the wheel is a native non-passive listener.
  useEffect(() => {
    const el = grid.current;
    if (!el) return;
    const onWheel = (e: WheelEvent) => {
      e.preventDefault(); // the page must not scroll while the pointer is over the steps
      const index = Number((e.target as Element).closest<HTMLElement>('[data-step]')?.dataset.step);
      if (!Number.isInteger(index)) return;
      if (wheel.current.index !== index) wheel.current = { index, acc: 0 };
      const next = accumulateWheel(wheel.current.acc, e.deltaY, e.deltaMode);
      wheel.current.acc = next.acc;
      if (next.steps) stepNote(store, index, next.steps);
    };
    el.addEventListener('wheel', onWheel, { passive: false });
    return () => el.removeEventListener('wheel', onWheel);
  }, [store]);

  return (
    <div className="step-view">
      <div className="steps" ref={grid}>
        {Array.from({ length: STEP_COUNT }, (_, i) => {
          const value = (overlay[`step.${i}`] as StepValue | undefined) ?? steps?.[i];
          return <StepCell key={i} index={i} active={row[i]} note={value?.note} gate={value?.active} focusable={i === focused} onFocusCell={setFocused} />;
        })}
      </div>
      {edited && (
        <span className="steps-edited" title="Edited by hand. Generate, Randomize or loading a preset rebuilds the pattern from the seed.">
          Edited
        </span>
      )}
    </div>
  );
}
