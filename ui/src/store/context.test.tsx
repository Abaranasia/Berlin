// @vitest-environment jsdom
import { describe, it, expect } from 'vitest';
import { act } from 'react';
import { createRoot } from 'react-dom/client';
import { createStore } from './store';
import { StoreProvider, useStore } from './context';
import type { Bridge } from '../bridge/bridge';
import type { PlayheadEvent, Snapshot } from '../bridge/protocol';

(globalThis as any).IS_REACT_ACT_ENVIRONMENT = true;

describe('selector isolation', () => {
  it('a component selecting only bpm does not re-render on a playhead event, but does on a bpm change', async () => {
    let playhead: (e: PlayheadEvent) => void = () => {};
    let snapshot: (s: Snapshot) => void = () => {};
    const bridge: Bridge = {
      dispatch: async () => ({ ok: false, error: 'x' }),
      getSnapshot: async () => ({ bpm: 120, playing: false, playheadStep: 0 }) as Snapshot,
      onPlayhead: (l) => ((playhead = l), () => {}),
      onSnapshot: (l) => ((snapshot = l), () => {}),
      chooseExportFile: async () => ({ cancelled: true, path: '' }),
      confirm: async () => false,
    };
    const store = createStore(bridge);
    let renders = 0;
    const Bpm = () => {
      renders++;
      return <span>{useStore((s) => s.engine?.bpm)}</span>;
    };
    const host = document.createElement('div');
    const root = createRoot(host);
    await act(async () => {
      store.start();
      root.render(<StoreProvider store={store}><Bpm /></StoreProvider>);
    });
    expect(host.textContent).toBe('120');

    const before = renders;
    await act(async () => playhead({ step: 4, playing: true }));
    expect(renders).toBe(before);

    await act(async () => snapshot({ bpm: 90 } as Snapshot));
    expect(host.textContent).toBe('90');
    expect(renders).toBe(before + 1);
    act(() => root.unmount());
  });
});
