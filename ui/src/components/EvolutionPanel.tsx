import { AUTO_EVOLVE_RATE } from '../bridge/limits';
import { useStore, useStoreApi } from '../store/context';
import { useShown } from '../store/hooks';
import { Slider } from './controls/Slider';
import { Toggle } from './controls/Toggle';

export function EvolutionPanel() {
  const store = useStoreApi();
  const enabled = useShown('autoEvolveEnabled', (s) => s.autoEvolveEnabled, false);
  const rate = useShown('autoEvolveRate', (s) => s.autoEvolveRate, 4);
  const mutations = useStore((s) => s.engine?.mutationCount ?? 0); // follows `snapshot` events from host-side evolution

  return (
    <section>
      <Toggle label="Auto-Evolve" checked={enabled} onChange={(next) => store.send('autoEvolveEnabled', { name: 'setAutoEvolveEnabled', args: { enabled: next } }, next)} />{' '}
      <Slider
        label="Evolve rate"
        value={rate}
        min={AUTO_EVOLVE_RATE.min}
        max={AUTO_EVOLVE_RATE.max}
        step={1}
        onChange={(next) => store.send('autoEvolveRate', { name: 'setAutoEvolveRate', args: { rate: next } }, next)}
      />{' '}
      <small>Mutations: {mutations}</small>
    </section>
  );
}
