import { useRef, type PointerEvent } from 'react';
import { dragToPosition, knobGeometry, positionOf, rangeAttrs, valueAt } from '../../lib/knob';
import type { SliderProps } from './Slider';

interface Drag {
  startY: number;
  startPos: number;
  last: number;
}

// Same props and native range input as Slider; the SVG face adds a vertical pointer drag (200 px = full range).
// No wheel and no double-click handling.
export function Knob({ label, value, min, max, step, disabled, skew, onChange }: SliderProps) {
  const drag = useRef<Drag | null>(null);
  const input = useRef<HTMLInputElement>(null);
  const { track, arc, pointer } = knobGeometry(positionOf(value, min, max, skew));

  const start = (e: PointerEvent<SVGSVGElement>) => {
    if (disabled || e.button !== 0) return;
    drag.current = { startY: e.clientY, startPos: positionOf(value, min, max, skew), last: value };
    e.currentTarget.setPointerCapture?.(e.pointerId); // optional: jsdom has no pointer capture
    input.current?.focus();
  };
  const move = (e: PointerEvent<SVGSVGElement>) => {
    const d = drag.current;
    if (!d) return;
    const next = valueAt(dragToPosition(d.startPos, d.startY - e.clientY), min, max, step, skew);
    if (next === d.last) return;
    d.last = next;
    onChange(next);
  };
  const stop = () => {
    drag.current = null;
  };

  return (
    <label className={disabled ? 'knob disabled' : 'knob'} data-knob="">
      <svg data-knob-face="" viewBox="0 0 56 56" width="56" height="56" aria-hidden="true" onPointerDown={start} onPointerMove={move} onPointerUp={stop} onPointerCancel={stop}>
        <circle className="knob-face" cx="28" cy="28" r="16" />
        <path className="knob-track" d={track} />
        {arc && <path className="knob-arc" d={arc} />}
        <line className="knob-pointer" x1={pointer[0]} y1={pointer[1]} x2={pointer[2]} y2={pointer[3]} />
      </svg>
      <input
        ref={input}
        className="knob-input"
        type="range"
        aria-label={label}
        {...rangeAttrs(value, min, max, step, skew)}
        disabled={disabled}
        onChange={(e) => onChange(skew ? skew.toValue(e.target.valueAsNumber) : e.target.valueAsNumber)}
      />
      <output>{Number(value.toFixed(3))}</output>
      <span className="knob-label">{label}</span>
    </label>
  );
}
