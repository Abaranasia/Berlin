// @vitest-environment jsdom
import { describe, it, expect, afterEach, vi } from 'vitest';
import { act } from 'react';
import { button, change, click, field, makeHarness, renderApp, settle } from './testing/harness';
import { createMockBridge } from './bridge/mock';
import type { Bridge } from './bridge/bridge';
import type { Command, DispatchResult, PlayheadEvent, Snapshot } from './bridge/protocol';

const bpmOf = (c: Command) => (c.args as { bpm: number }).bpm;

let unmount = () => {};
afterEach(() => {
  unmount();
  window.localStorage.clear();
});

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

  it('shows the delay recommendations for the snapshot BPM as copy buttons in the main area', async () => {
    const { host } = await mount();
    const chip = button(host, '1/4 500 ms');
    expect(region(host, 'main').contains(chip)).toBe(true);
    expect(host.querySelector('[data-role="delay-recommendations"]')).toBeNull();
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

const region = (host: HTMLElement, selector: string) => host.querySelector<HTMLElement>(selector)!;
const synthToggle = (host: HTMLElement) => field<HTMLButtonElement>(host, 'Synth panel');
const withSteps = (initial: Snapshot, patch: Record<number, { note: number; active: boolean }>): Snapshot => ({
  ...initial,
  steps: initial.steps.map((s, i) => patch[i] ?? s),
});

describe('App: shell regions', () => {
  it('places each control in its region', async () => {
    const { host } = await mount();
    const top = region(host, 'header');
    const main = region(host, 'main');
    const drawer = region(host, '#synth-panel');
    expect(top.contains(field(host, 'Master level'))).toBe(true);
    expect(top.contains(field(host, 'BPM'))).toBe(true);
    expect(main.querySelectorAll('[data-step]')).toHaveLength(16);
    expect(main.contains(field(host, 'Pulses'))).toBe(true);
    for (const label of ['FX', 'Cutoff', 'Delay mix', 'Room']) expect(drawer.contains(field(host, label))).toBe(true);
  });

  it('uses Knobs for every synth-panel range and keeps Sliders elsewhere', async () => {
    const { host } = await mount();
    const synthRanges = ['Pulse width', 'Cutoff', 'Resonance', 'Attack', 'Decay', 'Sustain', 'Release', 'LFO rate', 'LFO depth', 'Delay time', 'Delay feedback', 'Delay mix', 'Room', 'Damping', 'Wet', 'Dry'];
    for (const label of synthRanges) {
      const input = field(host, label);
      expect(input.type).toBe('range');
      expect(input.closest('[data-knob]')).not.toBeNull();
      expect(region(host, '#synth-panel').contains(input)).toBe(true);
    }
    expect(field(host, 'Master level').type).toBe('range');
    expect(field(host, 'Master level').closest('[data-knob]')).toBeNull();
  });

  it('has no Settings button without the settings capability (plugin)', async () => {
    const { host } = await mount();
    expect(host.querySelector('[aria-label="Settings"]')).toBeNull();
  });
});

describe('App: Settings dialog (standalone)', () => {
  const standalone = () => ({ settings: createMockBridge({ standalone: true }).settings });

  it('shows an enabled Settings button that opens the dialog without dispatching', async () => {
    const { host, dispatch } = await mount(standalone());
    const settings = field<HTMLButtonElement>(host, 'Settings');
    expect(settings.disabled).toBe(false);
    expect(host.querySelector('[role="dialog"]')).toBeNull();
    await click(settings);
    expect(host.querySelector('[role="dialog"]')).not.toBeNull();
    expect(dispatch).not.toHaveBeenCalled();
  });

  it('closes on Escape and returns focus to the Settings button', async () => {
    const { host } = await mount(standalone());
    const settings = field<HTMLButtonElement>(host, 'Settings');
    await click(settings);
    await act(async () => void document.dispatchEvent(new KeyboardEvent('keydown', { key: 'Escape', bubbles: true })));
    expect(host.querySelector('[role="dialog"]')).toBeNull();
    expect(document.activeElement).toBe(settings);
  });

  it('shares the output level with the TopBar slider', async () => {
    const { host } = await mount({ ...standalone(), dispatch: undefined }); // the dev mock engine applies setMasterLevel
    await click(field<HTMLButtonElement>(host, 'Settings'));
    await change(field(host, 'Output level'), '0.25');
    await settle();
    expect(field(host, 'Master level').value).toBe('0.25');
  });
});

describe('App: collapsible synth panel', () => {
  it('is closed by default but its controls stay in the DOM', async () => {
    const { host } = await mount();
    expect(region(host, '#synth-panel').hidden).toBe(true);
    expect(synthToggle(host).getAttribute('aria-expanded')).toBe('false');
    expect(synthToggle(host).getAttribute('aria-controls')).toBe('synth-panel');
    expect(field(host, 'Cutoff')).toBeTruthy();
  });

  it('opens on toggle and persists the choice', async () => {
    const { host } = await mount();
    await click(synthToggle(host));
    expect(region(host, '#synth-panel').hidden).toBe(false);
    expect(synthToggle(host).getAttribute('aria-expanded')).toBe('true');
    expect(window.localStorage.getItem('berlin.ui.synthPanelOpen')).toBe('1');
    await click(synthToggle(host));
    expect(window.localStorage.getItem('berlin.ui.synthPanelOpen')).toBe('0');
  });

  it('starts open when the stored choice is open', async () => {
    window.localStorage.setItem('berlin.ui.synthPanelOpen', '1');
    const { host } = await mount();
    expect(region(host, '#synth-panel').hidden).toBe(false);
    expect(synthToggle(host).getAttribute('aria-expanded')).toBe('true');
  });

  it('survives localStorage throwing: starts closed and toggles without an error', async () => {
    const spy = vi.spyOn(window, 'localStorage', 'get').mockImplementation(() => {
      throw new DOMException('denied', 'SecurityError');
    });
    try {
      const { host } = await mount();
      expect(region(host, '#synth-panel').hidden).toBe(true);
      await click(synthToggle(host));
      expect(region(host, '#synth-panel').hidden).toBe(false);
      expect(host.querySelector('[role="alert"]')).toBeNull();
    } finally {
      spy.mockRestore();
    }
  });
});

describe('App: step view', () => {
  it('distinguishes gated steps (LED on) from rests (LED off, note kept and flagged for dimming)', async () => {
    const { host, initial, emitSnapshot } = await mount();
    await act(async () => { emitSnapshot(withSteps(initial, { 1: { note: 48, active: true }, 2: { note: 50, active: false } })); });
    const gated = region(host, '[data-step="1"]');
    const rest = region(host, '[data-step="2"]');
    expect(gated.querySelector('.led')!.getAttribute('data-on')).toBe('true');
    expect(gated.textContent).toContain('C3');
    expect(rest.querySelector('.led')!.getAttribute('data-on')).toBe('false');
    expect(rest.textContent).toContain('D3'); // a muted step keeps its note, dimmed by data-gate=false
    expect(rest.getAttribute('data-gate')).toBe('false');
    expect(gated.getAttribute('data-gate')).toBe('true');
  });

  it('highlights only the playhead step', async () => {
    const { host, emitPlayhead } = await mount();
    await act(async () => { emitPlayhead({ step: 4, playing: true }); });
    expect(activeSteps(host)).toEqual([4]);
  });
});
