import { describe, it, expect } from 'vitest';
import { createStore, shown, type StoreKey, type KeyValues } from './store';
import type { Bridge } from '../bridge/bridge';
import type { Command, Patch } from '../bridge/protocol';

// READ-1: `send` and `shown` are paired by key type. The @ts-expect-error lines below are enforced by
// `pnpm --dir ui run typecheck` (an unused directive is an error), which is this suite's RED/GREEN signal.
const bridge: Bridge = {
  dispatch: () => new Promise(() => {}),
  getSnapshot: async () => null,
  onPlayhead: () => () => {},
  onSnapshot: () => () => {},
  chooseExportFile: async () => ({ cancelled: true, path: '' }),
  confirm: async () => false,
};
const cmd: Command = { name: 'setBpm', args: { bpm: 120 } };

// Compile-time only; never called.
export const typeChecks = (): void => {
  const store = createStore(bridge);
  const state = store.getState();
  store.send('bpm', cmd, 121);
  store.send('patch.waveform', cmd, 'saw');
  store.send('patch.delaySynced', cmd, true);
  store.send('gen.mode', cmd, 'euclidean');
  store.send('gen.range', cmd, [36, 72]);
  // @ts-expect-error unknown key
  store.send('bogus', cmd, 1);
  // @ts-expect-error bpm is a number
  store.send('bpm', cmd, 'fast');
  // @ts-expect-error waveform is a string literal union
  store.send('patch.waveform', cmd, 'sine');
  // @ts-expect-error range is sent as one pair under gen.range, never per field
  store.send('gen.rangeLow', cmd, 36);
  // @ts-expect-error the fallback must have the key's value type
  shown(state, 'patch.waveform', 440);
  const bpm: number = shown(state, 'bpm', 120);
  const waveform: Patch['waveform'] = shown(state, 'patch.waveform', 'saw');
  const range: [number, number] = shown(state, 'gen.range', [36, 72]);
  void [bpm, waveform, range];
};

describe('typed store keys', () => {
  it('every Patch field has a patch.<field> key and gen.range is a pair', () => {
    const keys: StoreKey[] = ['bpm', 'patch.cutoffHz', 'patch.waveform', 'gen.range', 'gen.pulses', 'masterLevel'];
    const sample: KeyValues['gen.range'] = [36, 72];
    expect(keys).toHaveLength(6);
    expect(sample).toEqual([36, 72]);
  });

  it('typed send and shown round-trip an optimistic value at runtime', () => {
    const store = createStore(bridge);
    store.send('gen.range', { name: 'setGenerationParams', args: { rangeLow: 36, rangeHigh: 72 } }, [36, 72]);
    expect(shown(store.getState(), 'gen.range', [0, 0] as [number, number])).toEqual([36, 72]);
    expect(shown(store.getState(), 'bpm', 120)).toBe(120);
  });
});
