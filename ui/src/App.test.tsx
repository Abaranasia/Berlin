// @vitest-environment jsdom
import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { act } from 'react';
import { createRoot, type Root } from 'react-dom/client';

(globalThis as any).IS_REACT_ACT_ENVIRONMENT = true;

const native = vi.hoisted(() => ({
  dispatch: vi.fn(),
  getSnapshot: vi.fn(),
  onPlayhead: vi.fn(),
}));
vi.mock('./bridge/native', () => native);

import { App } from './App';

let host: HTMLDivElement;
let root: Root;
let emit: (e: { step: number; playing: boolean }) => void;

const flush = () => act(async () => { await Promise.resolve(); });
const mount = async () => {
  await act(async () => { root.render(<App />); });
  await flush();
};
const button = (label: string) => [...host.querySelectorAll('button')].find((b) => b.textContent === label)!;
const activeSteps = () => [...host.querySelectorAll('[data-step]')].filter((e) => e.getAttribute('data-active') === 'true').map((e) => Number(e.getAttribute('data-step')));

beforeEach(() => {
  host = document.createElement('div');
  document.body.appendChild(host);
  root = createRoot(host);
  native.getSnapshot.mockResolvedValue({ bpm: 120, playing: false, playheadStep: 0 });
  native.dispatch.mockImplementation(async (_c: string, a: { bpm: number }) => ({ ok: true, error: '', snapshot: { bpm: a.bpm, playing: false, playheadStep: 0 } }));
  native.onPlayhead.mockImplementation((cb: typeof emit) => { emit = cb; return () => {}; });
});

afterEach(() => {
  act(() => root.unmount());
  host.remove();
  vi.clearAllMocks();
});

describe('App', () => {
  it('shows the BPM from the initial snapshot and 16 steps', async () => {
    await mount();
    expect(host.textContent).toContain('120');
    expect(host.querySelectorAll('[data-step]')).toHaveLength(16);
    expect(activeSteps()).toEqual([]);
  });

  it('+ and - dispatch setBpm with bpm+1 and bpm-1', async () => {
    await mount();
    await act(async () => { button('+').click(); });
    expect(native.dispatch).toHaveBeenLastCalledWith('setBpm', { bpm: 121 });
    expect(host.textContent).toContain('121');
    await act(async () => { button('-').click(); });
    expect(native.dispatch).toHaveBeenLastCalledWith('setBpm', { bpm: 120 });
  });

  it('a playhead event moves the highlighted step', async () => {
    await mount();
    await act(async () => { emit({ step: 5, playing: true }); });
    expect(activeSteps()).toEqual([5]);
    await act(async () => { emit({ step: 6, playing: true }); });
    expect(activeSteps()).toEqual([6]);
    await act(async () => { emit({ step: 6, playing: false }); });
    expect(activeSteps()).toEqual([]);
  });
});
