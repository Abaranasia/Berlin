import { useState } from 'react';
import { isValidSeed, normalizeSeed, SEED_ERROR } from '../lib/seed';
import { sendGen } from '../store/actions';
import { useStoreApi } from '../store/context';
import { useShown } from '../store/hooks';
import { Button } from './controls/Button';
import { Toggle } from './controls/Toggle';

export function GenerationPanel() {
  const store = useStoreApi();
  const seed = useShown('seed', (s) => s.seed, '0');
  const lockSeed = useShown('gen.lockSeed', (s) => s.generationParams.lockSeed, false);
  const [draft, setDraft] = useState<string | null>(null); // seed text being edited, committed on Enter or blur

  const commitSeed = () => {
    if (draft === null) return;
    const trimmed = normalizeSeed(draft);
    if (!isValidSeed(trimmed)) {
      store.setStatus({ text: SEED_ERROR, error: true }); // keep the draft so it can be corrected
      return;
    }
    if (store.getState().status?.text === SEED_ERROR) store.setStatus(null);
    setDraft(null);
    store.send('seed', { name: 'setSeed', args: { seed: trimmed } }, trimmed); // always the trimmed string, never a Number
  };
  const regenerate = (randomize: boolean) => store.send('regenerate', { name: 'regenerate', args: { randomize } }, randomize);

  return (
    <section>
      <label>
        Seed{' '}
        <input
          type="text"
          aria-label="Seed"
          value={draft ?? seed}
          onChange={(e) => setDraft(e.target.value)}
          onBlur={commitSeed}
          onKeyDown={(e) => e.key === 'Enter' && commitSeed()}
        />
      </label>{' '}
      <Toggle label="Lock Seed" checked={lockSeed} onChange={(locked) => sendGen(store, 'lockSeed', locked)} />{' '}
      <Button onClick={() => regenerate(false)}>Generate</Button>{' '}
      <Button disabled={lockSeed} onClick={() => regenerate(true)}>
        Randomize
      </Button>{' '}
      <Button onClick={() => store.send('mutate', { name: 'mutate', args: {} }, true)}>Mutate</Button>
    </section>
  );
}
