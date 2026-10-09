import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
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

// Collects unhandled rejections for the duration of one test (RES-1: nothing may escape the store).
function trackUnhandled() {
  const seen: unknown[] = [];
  const onRejection = (reason: unknown) => seen.push(reason);
  process.on('unhandledRejection', onRejection);
  return { seen, stop: () => process.off('unhandledRejection', onRejection) };
}

describe('RES-1: malformed results and handler throws still settle the key', () => {
  const malformed: [string, unknown][] = [
    ['null', null],
    ['undefined', undefined],
    ['an object without ok', {}],
    ['ok:true without a snapshot', { ok: true, error: '' }],
    ['ok:false without a string error', { ok: false }],
  ];

  it.each(malformed)('%s settles the key, drops the overlay and sets an error status', async (_label, bad) => {
    const guard = trackUnhandled();
    const { store, calls } = await started();
    store.send('bpm', bpm(130), 130);
    calls[0].resolve(bad as DispatchResult);
    await tick();
    guard.stop();

    expect(guard.seen).toEqual([]);
    expect(store.getState().overlay).toEqual({});
    expect(store.getState().status).toEqual({ text: 'Error: malformed response', error: true });
    expect(store.getState().engine?.bpm).toBe(120);
    store.send('bpm', bpm(131), 131); // the key is free again, so this dispatches at once
    expect(calls).toHaveLength(2);
  });

  it('a malformed result still lets a pending value through', async () => {
    const { store, calls } = await started();
    store.send('bpm', bpm(130), 130);
    store.send('bpm', bpm(140), 140);
    calls[0].resolve(null as unknown as DispatchResult);
    await tick();
    expect(calls.map((c) => c.cmd)).toEqual([bpm(130), bpm(140)]);
    expect(shown(store.getState(), 'bpm', 120)).toBe(140);
  });

  it('a throw inside the success handler (bad command args) settles the key and sets a status', async () => {
    const guard = trackUnhandled();
    const { store, calls } = await started();
    store.send('exportMidi', { name: 'exportMidi', args: undefined } as unknown as Command, 'x');
    calls[0].resolve(ok(snap()));
    await tick();
    guard.stop();

    expect(guard.seen).toEqual([]);
    expect(store.getState().overlay).toEqual({});
    expect(store.getState().status?.error).toBe(true);
    expect(store.getState().status?.text).toMatch(/^Error: /);
  });

  it('a listener that throws does not leak a rejection or leave the key stuck', async () => {
    const guard = trackUnhandled();
    const { store, calls } = await started();
    let armed = false;
    store.subscribe(() => {
      if (armed) throw new Error('listener boom');
    });
    store.send('bpm', bpm(130), 130);
    armed = true;
    calls[0].resolve(ok(snap({ bpm: 130 })));
    await tick();
    armed = false;
    guard.stop();

    expect(guard.seen).toEqual([]);
    expect(store.getState().overlay).toEqual({});
    store.send('bpm', bpm(131), 131);
    expect(calls).toHaveLength(2);
  });
});

describe('RES-2: settle timeout per in-flight dispatch', () => {
  const flush = () => vi.advanceTimersByTimeAsync(0);
  const timedStore = async (options?: { settleTimeoutMs?: number }) => {
    const fake = fakeBridge();
    const store = createStore(fake.bridge, options);
    store.start();
    await flush();
    return { ...fake, store };
  };

  beforeEach(() => vi.useFakeTimers());
  afterEach(() => vi.useRealTimers());

  it('defaults to 5 s: the key settles with a status and the overlay is dropped', async () => {
    const { store, calls } = await timedStore();
    store.send('bpm', bpm(130), 130);
    await vi.advanceTimersByTimeAsync(4999);
    expect(shown(store.getState(), 'bpm', 120)).toBe(130);
    expect(store.getState().status).toBeNull();

    await vi.advanceTimersByTimeAsync(1);
    expect(store.getState().overlay).toEqual({});
    expect(store.getState().status).toEqual({ text: 'Error: request timed out', error: true });
    store.send('bpm', bpm(131), 131); // free again
    expect(calls).toHaveLength(2);
  });

  it('is injectable', async () => {
    const { store } = await timedStore({ settleTimeoutMs: 50 });
    store.send('bpm', bpm(130), 130);
    await vi.advanceTimersByTimeAsync(49);
    expect(store.getState().status).toBeNull();
    await vi.advanceTimersByTimeAsync(1);
    expect(store.getState().status?.text).toBe('Error: request timed out');
  });

  it('sends the pending value on timeout and keeps its overlay', async () => {
    const { store, calls } = await timedStore({ settleTimeoutMs: 50 });
    store.send('bpm', bpm(130), 130);
    store.send('bpm', bpm(140), 140);
    await vi.advanceTimersByTimeAsync(50);
    expect(calls.map((c) => c.cmd)).toEqual([bpm(130), bpm(140)]);
    expect(shown(store.getState(), 'bpm', 120)).toBe(140);
    expect(store.getState().status?.text).toBe('Error: request timed out');
  });

  it('a response before the timeout clears the timer (no later timeout status)', async () => {
    const { store, calls } = await timedStore({ settleTimeoutMs: 50 });
    store.send('bpm', bpm(130), 130);
    calls[0].resolve(ok(snap({ bpm: 130 })));
    await vi.advanceTimersByTimeAsync(10);
    await vi.advanceTimersByTimeAsync(1000);
    expect(store.getState().status).toBeNull();
    expect(store.getState().engine?.bpm).toBe(130);
  });

  it('a late failure after the timeout does not disturb the key newer run', async () => {
    const { store, calls } = await timedStore({ settleTimeoutMs: 50 });
    store.send('bpm', bpm(130), 130);
    store.send('bpm', bpm(140), 140);
    await vi.advanceTimersByTimeAsync(50); // run 1 timed out, run 2 (140) is now in flight
    calls[0].resolve({ ok: false, error: 'busy' }); // late
    await flush();

    expect(store.getState().status?.text).toBe('Error: request timed out'); // not replaced by the late "busy"
    expect(shown(store.getState(), 'bpm', 120)).toBe(140); // newer overlay intact
    store.send('bpm', bpm(150), 150);
    expect(calls).toHaveLength(2); // still in flight: 150 is pending, not dispatched

    calls[1].resolve(ok(snap({ bpm: 140 })));
    await flush();
    expect(calls.map((c) => c.cmd)).toEqual([bpm(130), bpm(140), bpm(150)]);
  });

  it('a late snapshot older than an applied one is dropped (seq rules)', async () => {
    const { store, calls } = await timedStore({ settleTimeoutMs: 50 });
    store.send('bpm', bpm(130), 130); // seq 1
    store.send('bpm', bpm(140), 140);
    await vi.advanceTimersByTimeAsync(50); // seq 2 in flight
    calls[1].resolve(ok(snap({ bpm: 140 })));
    await flush();
    calls[0].resolve(ok(snap({ bpm: 130 }))); // late, seq 1 <= 2
    await flush();
    expect(store.getState().engine?.bpm).toBe(140);
  });

  it('a late snapshot with a fresh seq still applies, without touching status or overlay', async () => {
    const { store, calls } = await timedStore({ settleTimeoutMs: 50 });
    store.send('bpm', bpm(130), 130);
    await vi.advanceTimersByTimeAsync(50);
    calls[0].resolve(ok(snap({ bpm: 130 })));
    await flush();
    expect(store.getState().engine?.bpm).toBe(130);
    expect(store.getState().status?.text).toBe('Error: request timed out');
    expect(store.getState().overlay).toEqual({});
  });
});

// Flows such as the preset save need the outcome of one request (the `exists` token) without bypassing the store.
describe('onSettled: the outcome of one request', () => {
  it('receives the DispatchResult on success, after the state is updated', async () => {
    const { store, calls } = await started();
    const seen: unknown[] = [];
    store.send('bpm', bpm(130), 130, (outcome) => seen.push(outcome, store.getState().engine?.bpm, store.getState().overlay));
    const result = ok(snap({ bpm: 130 }));
    calls[0].resolve(result);
    await tick();
    expect(seen).toEqual([result, 130, {}]);
  });

  it('receives the failing DispatchResult on ok:false, which is still reported in the status', async () => {
    const { store, calls } = await started();
    const seen: unknown[] = [];
    store.send('bpm', bpm(130), 130, (outcome) => seen.push(outcome));
    calls[0].resolve({ ok: false, error: 'exists' });
    await tick();
    expect(seen).toEqual([{ ok: false, error: 'exists' }]);
    expect(store.getState().status?.error).toBe(true);
  });

  it.each([
    ['a rejected dispatch', (c: { reject(e: unknown): void; resolve(r: DispatchResult): void }) => c.reject(new Error('boom'))],
    ['a malformed result', (c: { reject(e: unknown): void; resolve(r: DispatchResult): void }) => c.resolve(null as unknown as DispatchResult)],
  ])('receives null for %s', async (_label, fail) => {
    const { store, calls } = await started();
    const seen: unknown[] = [];
    store.send('bpm', bpm(130), 130, (outcome) => seen.push(outcome));
    fail(calls[0]);
    await tick();
    expect(seen).toEqual([null]);
  });

  it('receives null when the request times out', async () => {
    vi.useFakeTimers();
    try {
      const fake = fakeBridge();
      const store = createStore(fake.bridge, { settleTimeoutMs: 50 });
      store.start();
      await vi.advanceTimersByTimeAsync(0);
      const seen: unknown[] = [];
      store.send('bpm', bpm(130), 130, (outcome) => seen.push(outcome));
      await vi.advanceTimersByTimeAsync(50);
      expect(seen).toEqual([null]);
    } finally {
      vi.useRealTimers();
    }
  });

  it('a value superseded while pending reports null; only the latest one gets the result', async () => {
    const { store, calls } = await started();
    const first: unknown[] = [];
    const second: unknown[] = [];
    const third: unknown[] = [];
    store.send('bpm', bpm(130), 130, (o) => first.push(o));
    store.send('bpm', bpm(140), 140, (o) => second.push(o));
    store.send('bpm', bpm(150), 150, (o) => third.push(o));
    expect(second).toEqual([null]);
    const r1 = ok(snap({ bpm: 130 }));
    calls[0].resolve(r1);
    await tick();
    const r3 = ok(snap({ bpm: 150 }));
    calls[1].resolve(r3);
    await tick();
    expect(first).toEqual([r1]);
    expect(third).toEqual([r3]);
    expect(second).toEqual([null]);
  });

  it('a throwing callback neither leaks a rejection nor leaves the key stuck', async () => {
    const guard = trackUnhandled();
    const { store, calls } = await started();
    store.send('bpm', bpm(130), 130, () => {
      throw new Error('callback boom');
    });
    calls[0].resolve(ok(snap({ bpm: 130 })));
    await tick();
    guard.stop();
    expect(guard.seen).toEqual([]);
    expect(store.getState().status).toEqual({ text: 'Error: callback boom', error: true });
    store.send('bpm', bpm(131), 131);
    expect(calls).toHaveLength(2);
  });
});
