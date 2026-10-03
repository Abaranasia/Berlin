// @vitest-environment jsdom
import { describe, it, expect, vi, beforeEach } from 'vitest';

type Win = Window & { __JUCE__?: any };

// The real JUCE interop script is loaded; only the host side (postMessage + init data) is mocked.
const installHost = (functions: string[]) => {
  const posted: any[] = [];
  (window as Win).__JUCE__ = {
    postMessage: (m: string) => posted.push(JSON.parse(m)),
    initialisationData: {
      __juce__platform: [],
      __juce__functions: functions,
      __juce__registeredGlobalEventIds: [],
      __juce__sliders: [],
      __juce__toggles: [],
      __juce__comboBoxes: [],
    },
  };
  return posted;
};

const reply = (resultId: number, result: unknown) =>
  (window as Win).__JUCE__.backend.emitByBackend('__juce__complete', JSON.stringify({ promiseId: resultId, result }));

const load = async () => {
  vi.resetModules();
  return import('./native');
};

beforeEach(() => {
  delete (window as Win).__JUCE__;
});

describe('native bridge with a host', () => {
  it('dispatch calls the native function and returns its result', async () => {
    const posted = installHost(['dispatch', 'getSnapshot']);
    const native = await load();

    const pending = native.dispatch('setBpm', { bpm: 121 });
    const call = posted.find((m) => m.eventId === '__juce__invoke');
    expect(call.payload.name).toBe('dispatch');
    expect(call.payload.params).toEqual(['setBpm', { bpm: 121 }]);

    reply(call.payload.resultId, { ok: true, error: '', snapshot: { bpm: 121 } });
    await expect(pending).resolves.toEqual({ ok: true, error: '', snapshot: { bpm: 121 } });
  });

  it('getSnapshot returns the snapshot', async () => {
    const posted = installHost(['dispatch', 'getSnapshot']);
    const native = await load();

    const pending = native.getSnapshot();
    const call = posted.find((m) => m.payload?.name === 'getSnapshot');
    reply(call.payload.resultId, { bpm: 100, playing: false, playheadStep: 0 });
    await expect(pending).resolves.toEqual({ bpm: 100, playing: false, playheadStep: 0 });
  });

  it('playhead listener receives events until unsubscribed', async () => {
    installHost(['dispatch', 'getSnapshot']);
    const native = await load();
    const seen: unknown[] = [];

    const off = native.onPlayhead((e) => seen.push(e));
    const backend = (window as Win).__JUCE__.backend;
    backend.emitByBackend('playhead', JSON.stringify({ step: 3, playing: true }));
    off();
    backend.emitByBackend('playhead', JSON.stringify({ step: 4, playing: true }));

    expect(seen).toEqual([{ step: 3, playing: true }]);
  });
});

describe('native bridge without a host', () => {
  it('degrades without throwing', async () => {
    vi.spyOn(console, 'warn').mockImplementation(() => {});
    const native = await load();

    expect(native.hasNativeBridge()).toBe(false);
    await expect(native.dispatch('setBpm', { bpm: 1 })).resolves.toMatchObject({ ok: false });
    await expect(native.getSnapshot()).resolves.toBeNull();
    expect(() => native.onPlayhead(() => {})()).not.toThrow();
  });
});
