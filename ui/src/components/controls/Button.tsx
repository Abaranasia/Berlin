import type { ReactNode } from 'react';

interface ButtonProps {
  children: ReactNode;
  disabled?: boolean;
  onClick(): void;
}

export const Button = ({ children, disabled, onClick }: ButtonProps) => (
  <button type="button" disabled={disabled} onClick={onClick}>
    {children}
  </button>
);
