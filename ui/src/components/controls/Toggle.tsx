interface ToggleProps {
  label: string;
  checked: boolean;
  disabled?: boolean;
  onChange(checked: boolean): void;
}

export const Toggle = ({ label, checked, disabled, onChange }: ToggleProps) => (
  <label>
    <input type="checkbox" aria-label={label} checked={checked} disabled={disabled} onChange={(e) => onChange(e.target.checked)} /> {label}
  </label>
);
