export type Option = string | { value: string; label: string };

interface SelectProps {
  label: string;
  value: string;
  options: readonly Option[];
  disabled?: boolean;
  onChange(value: string): void;
}

export function Select({ label, value, options, disabled, onChange }: SelectProps) {
  return (
    <label>
      {label}{' '}
      <select aria-label={label} value={value} disabled={disabled} onChange={(e) => onChange(e.target.value)}>
        {options.map((o) => {
          const { value: v, label: text } = typeof o === 'string' ? { value: o, label: o } : o;
          return (
            <option key={v} value={v}>
              {text}
            </option>
          );
        })}
      </select>
    </label>
  );
}
