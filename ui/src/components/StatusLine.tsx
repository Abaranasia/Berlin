import { useStore } from '../store/context';

// Errors use role="alert" (the error style hook for Slice 3); everything else is a plain status.
export function StatusLine() {
  const status = useStore((s) => s.status);
  return (
    <p role={status?.error ? 'alert' : 'status'} className={status?.error ? 'status error' : 'status'}>
      {status?.text ?? ''}
    </p>
  );
}
