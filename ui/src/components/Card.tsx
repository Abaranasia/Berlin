import { useId, type ReactNode } from 'react';

// A titled surface for one main-area panel; the panel body is rendered unchanged inside it.
export function Card({ title, children }: { title: string; children: ReactNode }) {
  const id = useId();
  return (
    <section className="card" aria-labelledby={id}>
      <h2 id={id}>{title}</h2>
      {children}
    </section>
  );
}
