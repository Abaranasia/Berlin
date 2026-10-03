import { useStoreApi } from '../store/context';
import { useShown } from '../store/hooks';
import { Toggle } from './controls/Toggle';

export function EngineToggles() {
  const store = useStoreApi();
  const synth = useShown('synthEnabled', (s) => s.synthEnabled, true);
  const effects = useShown('effectsEnabled', (s) => s.effectsEnabled, true);
  return (
    <section>
      <Toggle label="Synth" checked={synth} onChange={(enabled) => store.send('synthEnabled', { name: 'setSynthEnabled', args: { enabled } }, enabled)} />{' '}
      <Toggle label="FX" checked={effects} onChange={(enabled) => store.send('effectsEnabled', { name: 'setEffectsEnabled', args: { enabled } }, enabled)} />
    </section>
  );
}
