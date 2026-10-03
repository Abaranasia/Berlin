// @vitest-environment jsdom
import { describe, it, expect, vi, beforeEach } from 'vitest';

type Win = Window & { __JUCE__?: any };

const FUNCTIONS = ['dispatch', 'getSnapshot', 'chooseExportFile', 'confirm'];

// The real JUCE interop script is loaded; only the host side (postMessage + init data) is mocked.
const installHost = (functions: string[] = FUNCTIONS) => {
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

const callOf = (posted: any[], name: string) => posted.find((m) => m.payload?.name === name).payload;

const load = async () => {
  vi.resetModules();
  return import('./native');
};

beforeEach(() => {
  delete (window as Win).__JUCE__;
});

describe('nativeBridge with a host', () => {
  it('dispatch forwards the command name and args and returns the result', async () => {
    const posted = installHost();
    const { nativeBridge } = await load();

    const pending = nativeBridge.dispatch({ name: 'setBpm', args: { bpm: 121 } });
    const call = callOf(posted, 'dispatch');
    expect(call.params).toEqual(['setBpm', { bpm: 121 }]);

    reply(call.resultId, { ok: true, error: '', snapshot: { bpm: 121 } });
    await expect(pending).resolves.toEqual({ ok: true, error: '', snapshot: { bpm: 121 } });
  });

  it('getSnapshot returns the snapshot', async () => {
    const posted = installHost();
    const { nativeBridge } = await load();

    const pending = nativeBridge.getSnapshot();
    reply(callOf(posted, 'getSnapshot').resultId, { bpm: 100 });
    await expect(pending).resolves.toEqual({ bpm: 100 });
  });

  it('chooseExportFile returns the chooser result', async () => {
    const posted = installHost();
    const { nativeBridge } = await load();

    const pending = nativeBridge.chooseExportFile();
    reply(callOf(posted, 'chooseExportFile').resultId, { cancelled: false, path: 'C:/x/a.mid' });
    await expect(pending).resolves.toEqual({ cancelled: false, path: 'C:/x/a.mid' });
  });

  it('confirm forwards title and message and returns the boolean', async () => {
    const posted = installHost();
    const { nativeBridge } = await load();

    const pending = nativeBridge.confirm('Overwrite?', 'Lead A exists');
    const call = callOf(posted, 'confirm');
    expect(call.params).toEqual(['Overwrite?', 'Lead A exists']);
    reply(call.resultId, true);
    await expect(pending).resolves.toBe(true);
  });

  it.each([
    ['playhead', 'onPlayhead', { step: 3, playing: true }, { step: 4, playing: true }],
    ['snapshot', 'onSnapshot', { bpm: 130 }, { bpm: 131 }],
  ] as const)('%s listener receives events until unsubscribed', async (eventId, method, first, second) => {
    installHost();
    const { nativeBridge } = await load();
    const seen: unknown[] = [];

    const off = (nativeBridge[method] as (l: (e: any) => void) => () => void)((e) => seen.push(e));
    const backend = (window as Win).__JUCE__.backend;
    backend.emitByBackend(eventId, JSON.stringify(first));
    off();
    backend.emitByBackend(eventId, JSON.stringify(second));

    expect(seen).toEqual([first]);
  });
});

describe('nativeBridge without a host', () => {
  it('degrades without throwing', async () => {
    const { nativeBridge, hasNativeBridge } = await load();

    expect(hasNativeBridge()).toBe(false);
    await expect(nativeBridge.dispatch({ name: 'setBpm', args: { bpm: 1 } })).resolves.toMatchObject({ ok: false });
    await expect(nativeBridge.getSnapshot()).resolves.toBeNull();
    await expect(nativeBridge.chooseExportFile()).resolves.toEqual({ cancelled: true, path: '' });
    await expect(nativeBridge.confirm('t', 'm')).resolves.toBe(false);
    expect(() => nativeBridge.onPlayhead(() => {})()).not.toThrow();
    expect(() => nativeBridge.onSnapshot(() => {})()).not.toThrow();
  });
});
