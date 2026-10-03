import { useStore, useStoreApi } from '../store/context';
import { useShown } from '../store/hooks';
import { Select } from './controls/Select';
import { Toggle } from './controls/Toggle';

// The legacy editor's ComboBox: every N loops, N in 1, 2, 4, 8, 16.
const RATE_OPTIONS = [1, 2, 4, 8, 16].map((n) => ({ value: String(n), label: `Every ${n} loops` }));

export function EvolutionPanel() {
  const store = useStoreApi();
  const enabled = useShown('autoEvolveEnabled', (s) => s.autoEvolveEnabled, false);
  const rate = useShown('autoEvolveRate', (s) => s.autoEvolveRate, 4);
  const mutations = useStore((s) => s.engine?.mutationCount ?? 0); // follows `snapshot` events from host-side evolution

  return (
    <section>
      <Toggle label="Auto-Evolve" checked={enabled} onChange={(next) => store.send('autoEvolveEnabled', { name: 'setAutoEvolveEnabled', args: { enabled: next } }, next)} />{' '}
      <Select
        label="Evolve rate"
        value={String(rate)}
        options={RATE_OPTIONS}
        onChange={(next) => store.send('autoEvolveRate', { name: 'setAutoEvolveRate', args: { rate: Number(next) } }, Number(next))}
      />{' '}
      <small>Mutations: {mutations}</small>
    </section>
  );
}
