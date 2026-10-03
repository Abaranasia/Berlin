// @vitest-environment jsdom
import { describe, it, expect, afterEach } from 'vitest';
import { act } from 'react';
import { button, change, click, field, makeHarness, renderApp, settle } from './testing/harness';
import { createMockBridge } from './bridge/mock';
import type { Bridge } from './bridge/bridge';
import type { Command, DispatchResult, PlayheadEvent, Snapshot } from './bridge/protocol';

const bpmOf = (c: Command) => (c.args as { bpm: number }).bpm;

let unmount = () => {};
afterEach(() => unmount());

// A full snapshot (the panels read every section); BPM starts at 120.
const initialSnapshot = async (): Promise<Snapshot> => ({ ...(await createMockBridge().getSnapshot())!, bpm: 120 });

// By default `dispatch` answers setBpm like the host and everything else with an unchanged snapshot;
// pass `dispatch: undefined` to use the dev mock engine instead.
async function mount(over: Partial<Bridge> = {}) {
  const initial = await initialSnapshot();
  let emitPlayhead: (e: PlayheadEvent) => void = () => {};
  let emitSnapshot: (s: Snapshot) => void = () => {};
  const harness = makeHarness({
    getSnapshot: async () => initial,
    onPlayhead: (l) => ((emitPlayhead = l), () => {}),
    onSnapshot: (l) => ((emitSnapshot = l), () => {}),
    dispatch: async (c): Promise<DispatchResult> => ({ ok: true, error: '', snapshot: { ...initial, bpm: c.name === 'setBpm' ? bpmOf(c) : initial.bpm } }),
    ...over,
  });
  const mounted = await renderApp(harness);
  unmount = mounted.unmount;
  return { ...mounted, initial, emitPlayhead: (e: PlayheadEvent) => emitPlayhead(e), emitSnapshot: (s: Snapshot) => emitSnapshot(s) };
}
const bpmField = (host: HTMLElement) => field(host, 'BPM');
const activeSteps = (host: HTMLElement) =>
  [...host.querySelectorAll('[data-step]')].filter((e) => e.getAttribute('data-active') === 'true').map((e) => Number(e.getAttribute('data-step')));

describe('App: tempo and playhead', () => {
  it('shows the BPM from the initial snapshot and 16 steps', async () => {
    const { host } = await mount();
    expect(bpmField(host).value).toBe('120');
    expect(host.querySelectorAll('[data-step]')).toHaveLength(16);
    expect(activeSteps(host)).toEqual([]);
  });

  it('+ and - send setBpm through the store with bpm+1 and bpm-1', async () => {
    const { host, dispatch } = await mount();
    await click(button(host, '+'));
    expect(dispatch).toHaveBeenLastCalledWith({ name: 'setBpm', args: { bpm: 121 } });
    expect(bpmField(host).value).toBe('121');
    await click(button(host, '-'));
    expect(dispatch).toHaveBeenLastCalledWith({ name: 'setBpm', args: { bpm: 120 } });
    expect(bpmField(host).value).toBe('120');
  });

  it('rapid + presses coalesce and end at the last value (no stale closure)', async () => {
    const resolvers: (() => void)[] = [];
    const initial = await initialSnapshot();
    const { host, dispatch } = await mount({
      dispatch: (c) => new Promise((resolve) => resolvers.push(() => resolve({ ok: true, error: '', snapshot: { ...initial, bpm: bpmOf(c) } }))),
    });

    await act(async () => { button(host, '+').click(); button(host, '+').click(); button(host, '+').click(); });
    expect(bpmField(host).value).toBe('123'); // optimistic, before any response
    expect(dispatch).toHaveBeenCalledTimes(1);

    await act(async () => resolvers[0]());
    expect(dispatch).toHaveBeenCalledTimes(2);
    expect(dispatch).toHaveBeenLastCalledWith({ name: 'setBpm', args: { bpm: 123 } });
    await act(async () => resolvers[1]());
    expect(bpmField(host).value).toBe('123');
  });

  it('a playhead event moves the highlighted step', async () => {
    const { host, emitPlayhead } = await mount();
    await act(async () => { emitPlayhead({ step: 5, playing: true }); });
    expect(activeSteps(host)).toEqual([5]);
    await act(async () => { emitPlayhead({ step: 6, playing: true }); });
    expect(activeSteps(host)).toEqual([6]);
    await act(async () => { emitPlayhead({ step: 6, playing: false }); });
    expect(activeSteps(host)).toEqual([]);
  });

  it('a snapshot event updates the step display (evolved pattern)', async () => {
    const { host, initial, emitSnapshot } = await mount();
    expect(host.querySelector('[data-step="0"]')!.getAttribute('data-note')).toBe(String(initial.steps[0].note));
    const steps = initial.steps.map((s, i) => (i === 0 ? { note: 99, active: false } : s));
    await act(async () => { emitSnapshot({ ...initial, steps }); });
    expect(host.querySelector('[data-step="0"]')!.getAttribute('data-note')).toBe('99');
    expect(host.querySelector('[data-step="0"]')!.getAttribute('data-gate')).toBe('false');
  });
});

describe('App: Controls A are all composed', () => {
  it.each([
    'Synth', 'FX', 'BPM', 'Master level', 'Seed', 'Lock Seed', 'Rhythm mode', 'Pulses', 'Rotation', 'Chance %',
    'Scale', 'Root', 'Range low', 'Range high', 'Auto-Evolve', 'Evolve rate',
  ])('has the "%s" control', async (label) => {
    const { host } = await mount();
    expect(field(host, label)).toBeTruthy();
  });

  it.each(['Play', '-', '+', 'Generate', 'Randomize', 'Mutate', 'Export MIDI'])('has the "%s" button', async (label) => {
    const { host } = await mount();
    expect(button(host, label)).toBeTruthy();
  });

  it('has a status line', async () => {
    const { host } = await mount();
    expect(host.querySelector('[role="status"]')).toBeTruthy();
  });
});

describe('App: Controls B are all composed', () => {
  it.each([
    'Waveform', 'Pulse width', 'Cutoff', 'Resonance', 'Attack', 'Decay', 'Sustain', 'Release', 'LFO destination', 'LFO rate', 'LFO depth',
    'Delay sync', 'Delay division', 'Delay time', 'Delay feedback', 'Delay mix', 'Room', 'Damping', 'Wet', 'Dry', 'Preset name', 'Presets',
  ])('has the "%s" control', async (label) => {
    const { host } = await mount();
    expect(field(host, label)).toBeTruthy();
  });

  it.each(['Save', 'Load'])('has the "%s" button', async (label) => {
    const { host } = await mount();
    expect(button(host, label)).toBeTruthy();
  });

  it('shows the delay recommendations for the snapshot BPM', async () => {
    const { host } = await mount();
    expect(host.querySelector('[data-role="delay-recommendations"]')!.textContent).toContain('1/4 = 500 ms');
  });

  it('turning FX off disables every delay and reverb control, and on enables them again', async () => {
    const { host } = await mount({ dispatch: undefined }); // the mock engine confirms setEffectsEnabled
    const labels = ['Delay sync', 'Delay division', 'Delay time', 'Delay feedback', 'Delay mix', 'Room', 'Damping', 'Wet', 'Dry'];
    for (const label of labels) expect(field(host, label).disabled).toBe(false);
    await click(field(host, 'FX'));
    await settle();
    for (const label of labels) expect(field(host, label).disabled).toBe(true);
    expect(field(host, 'Waveform').disabled).toBe(false); // the synth is not an effect
    await click(field(host, 'FX'));
    await settle();
    for (const label of labels) expect(field(host, label).disabled).toBe(false);
  });
});

describe('App: disabled states and the status line', () => {
  it('Randomize is disabled while Lock Seed is on', async () => {
    const { host } = await mount({ dispatch: undefined }); // the mock engine confirms lockSeed
    expect(button(host, 'Randomize').disabled).toBe(false);
    await click(field(host, 'Lock Seed'));
    await settle();
    expect(button(host, 'Randomize').disabled).toBe(true);
  });

  it('a failing command shows its message in the status line from any panel', async () => {
    const { host } = await mount({ dispatch: async () => ({ ok: false, error: 'busy' }) });
    await change(field(host, 'Pulses'), '9');
    expect(host.querySelector('[role="alert"]')!.textContent).toBe('Busy, try again');
  });
});
