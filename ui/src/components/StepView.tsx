import { memo } from 'react';
import { noteName } from '../lib/noteName';
import { STEP_COUNT, stepRow } from '../state/steps';
import { useStore } from '../store/context';

interface StepCellProps {
  index: number;
  active: boolean;
  note: number | undefined;
  gate: boolean | undefined;
}

// Styled through the data-* attributes (see components.css); a gated step shows its note, a rest shows a dot.
const StepCell = memo(function StepCell({ index, active, note, gate }: StepCellProps) {
  return (
    <div className="step" data-step={index} data-active={active} data-note={note} data-gate={gate}>
      <span className="step-num">{index + 1}</span>
      <span className="led" data-on={gate === true} />
      <span className="step-note">{gate && note !== undefined ? noteName(note) : '·'}</span>
    </div>
  );
});

// The pattern row: the playhead highlight plus each step's note and gate from the snapshot.
export function StepView() {
  const playhead = useStore((s) => s.playhead);
  const steps = useStore((s) => s.engine?.steps);
  const row = stepRow(playhead.step, playhead.playing);
  return (
    <div className="steps">
      {Array.from({ length: STEP_COUNT }, (_, i) => (
        <StepCell key={i} index={i} active={row[i]} note={steps?.[i]?.note} gate={steps?.[i]?.active} />
      ))}
    </div>
  );
}
