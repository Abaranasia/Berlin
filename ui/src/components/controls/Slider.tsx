import { rangeAttrs } from '../../lib/knob';

// A skewed slider moves over a 0..1 position; `skew` converts between that position and the real value.
export interface Skew {
  toPosition(value: number): number;
  toValue(position: number): number;
}

export interface SliderProps {
  label: string;
  value: number;
  min: number;
  max: number;
  step?: number;
  disabled?: boolean;
  skew?: Skew;
  format?(value: number): string; // readout text; defaults to the plain number
  onChange(value: number): void;
}

export function Slider({ label, value, min, max, step, disabled, skew, format, onChange }: SliderProps) {
  const range = rangeAttrs(value, min, max, step, skew);
  return (
    <label>
      {label}{' '}
      <input
        type="range"
        aria-label={label}
        {...range}
        disabled={disabled}
        onChange={(e) => onChange(skew ? skew.toValue(e.target.valueAsNumber) : e.target.valueAsNumber)}
      />{' '}
      <output>{format ? format(value) : Number(value.toFixed(3))}</output>
    </label>
  );
}
