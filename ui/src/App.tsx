import { useEffect } from 'react';
import { LIMITS } from './bridge/limits';
import { StoreProvider, useStore, useStoreApi } from './store/context';
import { shown, type Store } from './store/store';
import { stepRow } from './state/steps';

const DEFAULT_BPM = 120;
const bpmOf = (s: Parameters<typeof shown>[0]) => shown(s, 'bpm', s.engine?.bpm ?? DEFAULT_BPM);

function Main() {
  const store = useStoreApi();
  const bpm = useStore(bpmOf);
  const playhead = useStore((s) => s.playhead);

  // Read the displayed value at click time, not from a render closure, so rapid presses accumulate.
  const changeBpm = (delta: number) => {
    const next = Math.min(LIMITS.bpm.max, Math.max(LIMITS.bpm.min, bpmOf(store.getState()) + delta));
    store.send('bpm', { name: 'setBpm', args: { bpm: next } }, next);
  };

  const row = stepRow(playhead.step, playhead.playing);

  return (
    <main>
      <h1>Berlin</h1>
      <div className="bpm">
        <button type="button" onClick={() => changeBpm(-1)}>-</button>
        <span>{bpm}</span>
        <button type="button" onClick={() => changeBpm(1)}>+</button>
        <small>BPM</small>
      </div>
      <div className="steps">
        {row.map((active, i) => (
          <span key={i} className={active ? 'step active' : 'step'} data-step={i} data-active={active} />
        ))}
      </div>
    </main>
  );
}

export function App({ store }: { store: Store }) {
  useEffect(() => store.start(), [store]);
  return (
    <StoreProvider store={store}>
      <Main />
    </StoreProvider>
  );
}
