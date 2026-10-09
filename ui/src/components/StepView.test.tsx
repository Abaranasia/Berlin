// @vitest-environment jsdom
import { describe, it, expect, afterEach } from 'vitest';
import { act } from 'react';
import { StepView } from './StepView';
import { makeHarness, render, settle, type Harness } from '../testing/harness';

// The mock engine: C minor scale notes 36..72; step i has note 48 + i and is gated on even steps.
// So step 2 is "D3, on", step 1 is "C#3, off" (a muted step, also out of scale), D#3 is 51, F3 is 53.
let unmount = () => {};
afterEach(() => unmount());
const mount = async (harness: Harness = makeHarness()) => {
  const mounted = await render(<StepView />, harness);
  unmount = mounted.unmount;
  return mounted;
};

const cell = (host: HTMLElement, index: number) => host.querySelector<HTMLElement>(`[data-step="${index}"]`)!;
const fire = (el: Element, type: string, init: MouseEventInit = {}) =>
  act(async () => void el.dispatchEvent(new MouseEvent(type, { bubbles: true, cancelable: true, ...init })));
const key = (el: Element, k: string) =>
  act(async () => void el.dispatchEvent(new KeyboardEvent('keydown', { key: k, bubbles: true, cancelable: true })));
const wheel = async (el: Element, deltaY: number, deltaMode = 0) => {
  const event = new WheelEvent('wheel', { deltaY, deltaMode, bubbles: true, cancelable: true });
  await act(async () => void el.dispatchEvent(event));
  return event;
};
const setSteps = (harness: Harness) =>
  harness.dispatch.mock.calls.map(([c]) => c).filter((c) => c.name === 'setStep').map((c) => c.args);
const tabbable = (host: HTMLElement) =>
  [...host.querySelectorAll<HTMLElement>('[data-step]')].filter((e) => e.tabIndex === 0).map((e) => Number(e.dataset.step));

describe('StepView: mute and note gestures', () => {
  it('a click without movement toggles mute and keeps the note', async () => {
    const harness = makeHarness();
    const { host } = await mount(harness);
    await fire(cell(host, 2), 'pointerdown', { clientY: 100 });
    await fire(cell(host, 2), 'pointerup', { clientY: 102 });
    await settle();
    expect(setSteps(harness)).toEqual([{ index: 2, note: 50, active: false }]);
    expect(cell(host, 2).getAttribute('aria-pressed')).toBe('false');
  });

  it('a click on a muted step unmutes it', async () => {
    const harness = makeHarness();
    const { host } = await mount(harness);
    await fire(cell(host, 1), 'pointerdown', { clientY: 100 });
    await fire(cell(host, 1), 'pointerup', { clientY: 100 });
    expect(setSteps(harness)).toEqual([{ index: 1, note: 49, active: true }]);
  });

  it('a vertical drag steps through the scale at 12 px per note and does not toggle mute', async () => {
    const harness = makeHarness();
    const { host } = await mount(harness);
    await fire(cell(host, 2), 'pointerdown', { clientY: 100 });
    await fire(cell(host, 2), 'pointermove', { clientY: 88 }); // one note up: D3 (50) to D#3 (51)
    await settle();
    await fire(cell(host, 2), 'pointermove', { clientY: 64 }); // three notes up from the start: G3 (55)
    await fire(cell(host, 2), 'pointerup', { clientY: 64 });
    await settle();
    const sent = setSteps(harness);
    expect(sent[0]).toEqual({ index: 2, note: 51, active: true });
    expect(sent.at(-1)).toEqual({ index: 2, note: 55, active: true });
    expect(sent.every((s) => s.active)).toBe(true);
  });

  it('dragging down steps the note down', async () => {
    const harness = makeHarness();
    const { host } = await mount(harness);
    await fire(cell(host, 2), 'pointerdown', { clientY: 100 });
    await fire(cell(host, 2), 'pointermove', { clientY: 112 });
    expect(setSteps(harness)).toEqual([{ index: 2, note: 48, active: true }]);
  });

  it('a wheel notch up moves one scale note up and prevents the page scroll', async () => {
    const harness = makeHarness();
    const { host } = await mount(harness);
    const event = await wheel(cell(host, 2), -100);
    expect(event.defaultPrevented).toBe(true);
    expect(setSteps(harness)).toEqual([{ index: 2, note: 51, active: true }]);
  });

  it('a wheel notch down moves one scale note down', async () => {
    const harness = makeHarness();
    const { host } = await mount(harness);
    await wheel(cell(host, 2), 100);
    expect(setSteps(harness)).toEqual([{ index: 2, note: 48, active: true }]);
  });

  it('small wheel deltas accumulate on one step, and the remainder is dropped when the step changes', async () => {
    const harness = makeHarness();
    const { host } = await mount(harness);
    await wheel(cell(host, 2), -60);
    await wheel(cell(host, 3), -60);
    expect(setSteps(harness)).toEqual([]);
    await wheel(cell(host, 3), -60);
    expect(setSteps(harness)).toEqual([{ index: 3, note: 53, active: false }]); // step 3 is D#3 (51), muted
  });
});

describe('StepView: keyboard', () => {
  it('uses a roving tabindex: only one cell is tabbable and it follows focus', async () => {
    const { host } = await mount();
    expect(tabbable(host)).toEqual([0]);
    await key(cell(host, 0), 'ArrowRight');
    expect(tabbable(host)).toEqual([1]);
  });

  it('Space toggles mute, ArrowUp and ArrowDown change the note', async () => {
    const harness = makeHarness();
    const { host } = await mount(harness);
    await key(cell(host, 2), ' ');
    await settle();
    await key(cell(host, 2), 'ArrowUp');
    await settle();
    await key(cell(host, 2), 'ArrowDown');
    await settle();
    expect(setSteps(harness)).toEqual([
      { index: 2, note: 50, active: false },
      { index: 2, note: 51, active: false },
      { index: 2, note: 50, active: false },
    ]);
  });

  it('ArrowRight moves focus right, End to the last cell, Home to the first, with one tabbable cell', async () => {
    const { host } = await mount();
    await key(cell(host, 2), 'ArrowRight');
    expect(document.activeElement).toBe(cell(host, 3));
    expect(tabbable(host)).toEqual([3]);
    await key(cell(host, 3), 'End');
    expect(document.activeElement).toBe(cell(host, 15));
    expect(tabbable(host)).toEqual([15]);
    await key(cell(host, 15), 'Home');
    expect(document.activeElement).toBe(cell(host, 0));
  });

  it('chained presses accumulate on the shown (optimistic) value while a request is in flight', async () => {
    const harness = makeHarness({ dispatch: () => new Promise(() => {}) });
    const { host } = await mount(harness);
    await key(cell(host, 2), 'ArrowUp');
    await key(cell(host, 2), 'ArrowUp');
    expect(cell(host, 2).getAttribute('aria-label')).toBe('Step 3, F3, on'); // 50 -> 51 -> 53
    expect(setSteps(harness)).toEqual([{ index: 2, note: 51, active: true }]); // the second one waits as the pending value
  });
});

describe('StepView: display', () => {
  it('labels each cell and exposes the gate as aria-pressed', async () => {
    const { host } = await mount();
    expect(cell(host, 2).getAttribute('role')).toBe('button');
    expect(cell(host, 2).getAttribute('aria-label')).toBe('Step 3, D3, on');
    expect(cell(host, 2).getAttribute('aria-pressed')).toBe('true');
    expect(cell(host, 1).getAttribute('aria-label')).toBe('Step 2, C#3, off');
    expect(cell(host, 1).getAttribute('aria-pressed')).toBe('false');
  });

  it('shows a muted step note dimmed (data-gate false) and a gated one normally', async () => {
    const { host } = await mount();
    expect(cell(host, 1).querySelector('.step-note')!.textContent).toBe('C#3');
    expect(cell(host, 1).getAttribute('data-gate')).toBe('false');
    expect(cell(host, 2).getAttribute('data-gate')).toBe('true');
  });

  it('shows the Edited badge after an edit and removes it when the engine regenerates', async () => {
    const harness = makeHarness();
    const { host } = await mount(harness);
    expect(host.querySelector('.steps-edited')).toBeNull();
    await fire(cell(host, 2), 'pointerdown', { clientY: 100 });
    await fire(cell(host, 2), 'pointerup', { clientY: 100 });
    await settle();
    expect(host.querySelector('.steps-edited')!.textContent).toBe('Edited');
    await act(async () => void (await harness.bridge.dispatch({ name: 'regenerate', args: { randomize: false } })));
    expect(host.querySelector('.steps-edited')).toBeNull();
  });

  it('still highlights only the playhead step', async () => {
    let emit: (e: { step: number; playing: boolean }) => void = () => {};
    const { host } = await mount(makeHarness({ onPlayhead: (l) => ((emit = l), () => {}) }));
    expect(host.querySelectorAll('[data-step][data-active="true"]')).toHaveLength(0);
    await act(async () => emit({ step: 4, playing: true }));
    expect([...host.querySelectorAll<HTMLElement>('[data-step][data-active="true"]')].map((e) => e.dataset.step)).toEqual(['4']);
  });
});
