// @vitest-environment jsdom
import { describe, it, expect, afterEach } from 'vitest';
import { Transport } from './Transport';
import { blur, button, change, click, field, makeHarness, pressEnter, render, settle, type Harness } from '../testing/harness';
import type { DispatchResult } from '../bridge/protocol';

let unmount = () => {};
afterEach(() => unmount());
const mount = async (harness: Harness = makeHarness()) => {
  const mounted = await render(<Transport />, harness);
  unmount = mounted.unmount;
  return mounted;
};

describe('Transport: Play/Stop', () => {
  it('sends setPlaying with the opposite state and shows the confirmed state', async () => {
    const { host, dispatch } = await mount();
    expect(button(host, 'Play')).toBeTruthy();

    await click(button(host, 'Play'));
    expect(dispatch).toHaveBeenLastCalledWith({ name: 'setPlaying', args: { playing: true } });
    expect(button(host, 'Stop')).toBeTruthy();

    await click(button(host, 'Stop'));
    expect(dispatch).toHaveBeenLastCalledWith({ name: 'setPlaying', args: { playing: false } });
    expect(button(host, 'Play')).toBeTruthy();
  });
});

describe('Transport: BPM', () => {
  it('shows the snapshot BPM; - and + send setBpm', async () => {
    const { host, dispatch } = await mount();
    expect(field(host, 'BPM').value).toBe('120');
    await click(button(host, '+'));
    expect(dispatch).toHaveBeenLastCalledWith({ name: 'setBpm', args: { bpm: 121 } });
    await click(button(host, '-'));
    await click(button(host, '-'));
    expect(dispatch).toHaveBeenLastCalledWith({ name: 'setBpm', args: { bpm: 119 } });
    expect(field(host, 'BPM').value).toBe('119');
  });

  it('typing then Enter sends the clamped, rounded value', async () => {
    const { host, dispatch } = await mount();
    await change(field(host, 'BPM'), '150.4');
    expect(dispatch).not.toHaveBeenCalled(); // editing alone never sends
    await pressEnter(field(host, 'BPM'));
    expect(dispatch).toHaveBeenLastCalledWith({ name: 'setBpm', args: { bpm: 150 } });

    await change(field(host, 'BPM'), '500');
    await pressEnter(field(host, 'BPM'));
    expect(dispatch).toHaveBeenLastCalledWith({ name: 'setBpm', args: { bpm: 240 } });
  });

  it('leaving the field commits it too', async () => {
    const { host, dispatch } = await mount();
    await change(field(host, 'BPM'), '90');
    await blur(field(host, 'BPM'));
    expect(dispatch).toHaveBeenLastCalledWith({ name: 'setBpm', args: { bpm: 90 } });
  });

  it('non-numeric input sends nothing and the field returns to the shown BPM', async () => {
    const { host, dispatch } = await mount();
    await change(field(host, 'BPM'), '');
    await pressEnter(field(host, 'BPM'));
    expect(dispatch).not.toHaveBeenCalled();
    expect(field(host, 'BPM').value).toBe('120');
  });
});

describe('Transport: master level', () => {
  it('sends setMasterLevel with the slider value', async () => {
    const { host, dispatch, store } = await mount();
    expect(field(host, 'Master level').value).toBe('0.8');
    await change(field(host, 'Master level'), '0.5');
    expect(dispatch).toHaveBeenLastCalledWith({ name: 'setMasterLevel', args: { level: 0.5 } });
    await settle();
    expect(store.getState().engine?.masterLevel).toBe(0.5);
  });

  it('reverts to the engine value and reports the error when the command fails', async () => {
    const failing = makeHarness({ dispatch: async (): Promise<DispatchResult> => ({ ok: false, error: 'busy' }) });
    const { host, store } = await mount(failing);
    await change(field(host, 'Master level'), '0.3');
    await settle();
    expect(field(host, 'Master level').value).toBe('0.8');
    expect(store.getState().status).toEqual({ text: 'Busy, try again', error: true });
  });
});
