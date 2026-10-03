// @vitest-environment jsdom
import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { act } from 'react';
import { createRoot, type Root } from 'react-dom/client';
import { App } from './App';
import { createStore } from './store/store';
import type { Bridge } from './bridge/bridge';
import type { Command, DispatchResult, PlayheadEvent, Snapshot } from './bridge/protocol';

(globalThis as any).IS_REACT_ACT_ENVIRONMENT = true;

const snapshotWith = (bpm: number) => ({ bpm, playing: false, playheadStep: 0 }) as Snapshot;
const bpmOf = (c: Command) => (c.args as { bpm: number }).bpm;

let host: HTMLDivElement;
let root: Root;
let emitPlayhead: (e: PlayheadEvent) => void;
let dispatch: ReturnType<typeof vi.fn<(c: Command) => Promise<DispatchResult>>>;

const mount = async () => {
  const bridge: Bridge = {
    dispatch,
    getSnapshot: async () => snapshotWith(120),
    onPlayhead: (l) => ((emitPlayhead = l), () => {}),
    onSnapshot: () => () => {},
    chooseExportFile: async () => ({ cancelled: true, path: '' }),
    confirm: async () => false,
  };
  await act(async () => { root.render(<App store={createStore(bridge)} />); });
};
const button = (label: string) => [...host.querySelectorAll('button')].find((b) => b.textContent === label)!;
const activeSteps = () =>
  [...host.querySelectorAll('[data-step]')].filter((e) => e.getAttribute('data-active') === 'true').map((e) => Number(e.getAttribute('data-step')));

beforeEach(() => {
  host = document.createElement('div');
  document.body.appendChild(host);
  root = createRoot(host);
  dispatch = vi.fn(async (c) => ({ ok: true, error: '', snapshot: snapshotWith(bpmOf(c)) }));
});

afterEach(() => {
  act(() => root.unmount());
  host.remove();
});

describe('App', () => {
  it('shows the BPM from the initial snapshot and 16 steps', async () => {
    await mount();
    expect(host.textContent).toContain('120');
    expect(host.querySelectorAll('[data-step]')).toHaveLength(16);
    expect(activeSteps()).toEqual([]);
  });

  it('+ and - send setBpm through the store with bpm+1 and bpm-1', async () => {
    await mount();
    await act(async () => { button('+').click(); });
    expect(dispatch).toHaveBeenLastCalledWith({ name: 'setBpm', args: { bpm: 121 } });
    expect(host.textContent).toContain('121');
    await act(async () => { button('-').click(); });
    expect(dispatch).toHaveBeenLastCalledWith({ name: 'setBpm', args: { bpm: 120 } });
    expect(host.textContent).toContain('120');
  });

  it('rapid + presses coalesce and end at the last value (no stale closure)', async () => {
    const resolvers: (() => void)[] = [];
    dispatch.mockImplementation((c) => new Promise((resolve) => resolvers.push(() => resolve({ ok: true, error: '', snapshot: snapshotWith(bpmOf(c)) }))));
    await mount();

    await act(async () => { button('+').click(); button('+').click(); button('+').click(); });
    expect(host.textContent).toContain('123'); // optimistic, before any response
    expect(dispatch).toHaveBeenCalledTimes(1);

    await act(async () => resolvers[0]());
    expect(dispatch).toHaveBeenCalledTimes(2);
    expect(dispatch).toHaveBeenLastCalledWith({ name: 'setBpm', args: { bpm: 123 } });
    await act(async () => resolvers[1]());
    expect(host.textContent).toContain('123');
  });

  it('a playhead event moves the highlighted step', async () => {
    await mount();
    await act(async () => { emitPlayhead({ step: 5, playing: true }); });
    expect(activeSteps()).toEqual([5]);
    await act(async () => { emitPlayhead({ step: 6, playing: true }); });
    expect(activeSteps()).toEqual([6]);
    await act(async () => { emitPlayhead({ step: 6, playing: false }); });
    expect(activeSteps()).toEqual([]);
  });
});
