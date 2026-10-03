import { describe, it, expect, vi } from 'vitest';
import { createStore, shown } from './store';
import type { Bridge } from '../bridge/bridge';
import type { Command, DispatchResult, PlayheadEvent, Snapshot } from '../bridge/protocol';

const snap = (over: Partial<Snapshot> = {}) => ({ bpm: 120, playing: false, playheadStep: 2, patch: { cutoffHz: 1000 }, ...over }) as Snapshot;
const ok = (snapshot: Snapshot): DispatchResult => ({ ok: true, error: '', snapshot });
const bpm = (value: number): Command => ({ name: 'setBpm', args: { bpm: value } });
const cutoff = (value: number): Command => ({ name: 'setPatch', args: { cutoffHz: value } });
const tick = () => new Promise((r) => setTimeout(r, 0));

// A bridge whose dispatches stay pending until the test settles them.
function fakeBridge(initial: Snapshot | null = snap()) {
  const calls: { cmd: Command; resolve(r: DispatchResult): void; reject(e: unknown): void }[] = [];
  const playhead = new Set<(e: PlayheadEvent) => void>();
  const snapshots = new Set<(s: Snapshot) => void>();
  const bridge: Bridge = {
    dispatch: (cmd) => new Promise((resolve, reject) => calls.push({ cmd, resolve, reject })),
    getSnapshot: async () => initial,
    onPlayhead: (l) => (playhead.add(l), () => playhead.delete(l)),
    onSnapshot: (l) => (snapshots.add(l), () => snapshots.delete(l)),
    chooseExportFile: async () => ({ cancelled: true, path: '' }),
    confirm: async () => false,
  };
  return { bridge, calls, emitPlayhead: (e: PlayheadEvent) => playhead.forEach((l) => l(e)), emitSnapshot: (s: Snapshot) => snapshots.forEach((l) => l(s)) };
}

const started = async (initial?: Snapshot | null) => {
  const fake = fakeBridge(initial);
  const store = createStore(fake.bridge);
  const stop = store.start();
  await tick();
  return { ...fake, store, stop };
};

describe('slices', () => {
  it('start loads engine and playhead from the snapshot; status, overlay and draft begin empty', async () => {
    const { store } = await started();
    expect(store.getState()).toMatchObject({
      engine: { bpm: 120 }, playhead: { step: 2, playing: false }, status: null, overlay: {}, draft: { lastManualDelay: null },
    });
  });

  it('a playhead event moves only the playhead slice; the initial snapshot does not overwrite a newer event', async () => {
    const fake = fakeBridge();
    const store = createStore(fake.bridge);
    store.start();
    fake.emitPlayhead({ step: 7, playing: true });
    await tick();
    expect(store.getState().playhead).toEqual({ step: 7, playing: true });
    expect(store.getState().engine?.bpm).toBe(120);
  });

  it('a snapshot event updates engine', async () => {
    const { store, emitSnapshot } = await started();
    emitSnapshot(snap({ bpm: 133 }));
    expect(store.getState().engine?.bpm).toBe(133);
  });

  it('setStatus and setDraft write their own slices', async () => {
    const { store } = await started();
    store.setStatus({ text: 'Seed must be a whole number.', error: true });
    store.setDraft({ lastManualDelay: 0.25 });
    expect(store.getState().status).toEqual({ text: 'Seed must be a whole number.', error: true });
    expect(store.getState().draft.lastManualDelay).toBe(0.25);
  });

  it('stop unsubscribes from bridge events', async () => {
    const { store, stop, emitSnapshot } = await started();
    stop();
    emitSnapshot(snap({ bpm: 200 }));
    expect(store.getState().engine?.bpm).toBe(120);
  });
});

describe('coalescing', () => {
  it('3 rapid sends on one key make 2 dispatches and the last value wins', async () => {
    const { store, calls } = await started();
    store.send('bpm', bpm(121), 121);
    store.send('bpm', bpm(122), 122);
    store.send('bpm', bpm(123), 123);
    expect(calls.map((c) => c.cmd)).toEqual([bpm(121)]);

    calls[0].resolve(ok(snap({ bpm: 121 })));
    await tick();
    expect(calls.map((c) => c.cmd)).toEqual([bpm(121), bpm(123)]);
  });

  it('different keys do not block each other', async () => {
    const { store, calls } = await started();
    store.send('bpm', bpm(121), 121);
    store.send('patch.cutoffHz', cutoff(500), 500);
    expect(calls.map((c) => c.cmd)).toEqual([bpm(121), cutoff(500)]);
  });
});

describe('optimistic overlay and ordering', () => {
  it('shows the new value before any response, then drops it once the key settles', async () => {
    const { store, calls } = await started();
    store.send('bpm', bpm(130), 130);
    expect(shown(store.getState(), 'bpm', 120)).toBe(130);

    calls[0].resolve(ok(snap({ bpm: 130 })));
    await tick();
    expect(store.getState().overlay).toEqual({});
    expect(store.getState().engine?.bpm).toBe(130);
  });

  it('drops an out-of-order response snapshot but still settles its key', async () => {
    const { store, calls } = await started();
    store.send('bpm', bpm(130), 130); // seq 1
    store.send('patch.cutoffHz', cutoff(500), 500); // seq 2

    calls[1].resolve(ok(snap({ bpm: 111, patch: { cutoffHz: 500 } as Snapshot['patch'] })));
    await tick();
    calls[0].resolve(ok(snap({ bpm: 999 })));
    await tick();

    expect(store.getState().engine?.bpm).toBe(111);
    expect(store.getState().overlay).toEqual({});
  });

  it('a snapshot event does not clobber an in-flight overlay until the key settles', async () => {
    const { store, calls, emitSnapshot } = await started();
    store.send('patch.cutoffHz', cutoff(500), 500);
    emitSnapshot(snap({ patch: { cutoffHz: 1000 } as Snapshot['patch'] }));
    expect(store.getState().engine?.patch.cutoffHz).toBe(1000);
    expect(shown(store.getState(), 'patch.cutoffHz', 1000)).toBe(500);

    calls[0].resolve(ok(snap({ patch: { cutoffHz: 500 } as Snapshot['patch'] })));
    await tick();
    expect(shown(store.getState(), 'patch.cutoffHz', store.getState().engine!.patch.cutoffHz)).toBe(500);
  });
});

describe('failures', () => {
  it('ok:false with nothing pending drops the overlay and sets the status from the token', async () => {
    const { store, calls } = await started();
    store.send('bpm', bpm(130), 130);
    calls[0].resolve({ ok: false, error: 'busy' });
    await tick();
    expect(store.getState().overlay).toEqual({});
    expect(shown(store.getState(), 'bpm', store.getState().engine!.bpm)).toBe(120);
    expect(store.getState().status).toEqual({ text: 'Busy, try again', error: true });
  });

  it('with a newer pending value the overlay is kept and the value is still sent', async () => {
    const { store, calls } = await started();
    store.send('bpm', bpm(130), 130);
    store.send('bpm', bpm(140), 140);
    calls[0].resolve({ ok: false, error: 'busy' });
    await tick();
    expect(shown(store.getState(), 'bpm', 120)).toBe(140);
    expect(calls.map((c) => c.cmd)).toEqual([bpm(130), bpm(140)]);
  });

  it('a successful flow command reports its success message', async () => {
    const { store, calls } = await started();
    store.send('regenerate', { name: 'regenerate', args: { randomize: true } }, true);
    calls[0].resolve(ok(snap()));
    await tick();
    expect(store.getState().status).toEqual({ text: 'Randomized.', error: false });
  });
});

describe('rejection never escapes', () => {
  it('a rejected dispatch becomes "Error: <message>" and settles the key', async () => {
    const { store, calls } = await started();
    store.send('bpm', bpm(130), 130);
    calls[0].reject(new Error('native exploded'));
    await tick();
    expect(store.getState().status).toEqual({ text: 'Error: native exploded', error: true });
    expect(store.getState().overlay).toEqual({});
  });

  it('a dispatch that throws synchronously is handled the same way', async () => {
    const fake = fakeBridge();
    fake.bridge.dispatch = () => {
      throw new Error('sync boom');
    };
    const store = createStore(fake.bridge);
    expect(() => store.send('bpm', bpm(130), 130)).not.toThrow();
    await tick();
    expect(store.getState().status?.text).toBe('Error: sync boom');
  });

  it('getSnapshot failing at start is reported, not thrown', async () => {
    const fake = fakeBridge();
    fake.bridge.getSnapshot = vi.fn().mockRejectedValue(new Error('no host'));
    const store = createStore(fake.bridge);
    store.start();
    await tick();
    expect(store.getState().status?.text).toBe('Error: no host');
  });
});
