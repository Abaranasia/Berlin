// A skewed slider moves over a 0..1 position; `skew` converts between that position and the real value.
export interface Skew {
  toPosition(value: number): number;
  toValue(position: number): number;
}

interface SliderProps {
  label: string;
  value: number;
  min: number;
  max: number;
  step?: number;
  disabled?: boolean;
  skew?: Skew;
  onChange(value: number): void;
}

export function Slider({ label, value, min, max, step, disabled, skew, onChange }: SliderProps) {
  const range = skew ? { min: 0, max: 1, step: 'any', value: skew.toPosition(value) } : { min, max, step, value };
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
      <output>{Number(value.toFixed(3))}</output>
    </label>
  );
}
