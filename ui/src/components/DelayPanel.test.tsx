// @vitest-environment jsdom
import { describe, it, expect, afterEach } from 'vitest';
import { act } from 'react';
import { DelayPanel } from './DelayPanel';
import { createMockBridge } from '../bridge/mock';
import { change, click, field, makeHarness, render, settle } from '../testing/harness';
import { DELAY_DIVISIONS } from '../bridge/protocol';

let unmount = () => {};
afterEach(() => unmount());
const mount = async () => {
  const mounted = await render(<DelayPanel />);
  unmount = mounted.unmount;
  return mounted;
};
const options = (select: HTMLSelectElement) => [...select.options].map((o) => o.value);
const recommendations = (host: HTMLElement) => host.querySelector('[data-role="delay-recommendations"]')!.textContent;

describe('DelayPanel', () => {
  it('fields reflect the snapshot', async () => {
    const { host } = await mount();
    expect(field<HTMLSelectElement>(host, 'Delay division').value).toBe('quarter');
    expect(field(host, 'Delay time').value).toBe('0.5');
    expect(field(host, 'Delay feedback').value).toBe('0.3');
    expect(field(host, 'Delay mix').value).toBe('0.2');
    expect(field(host, 'Delay sync').checked).toBe(false);
  });

  it('offers exactly the delay divisions', async () => {
    const { host } = await mount();
    expect(options(field<HTMLSelectElement>(host, 'Delay division'))).toEqual([...DELAY_DIVISIONS]);
  });

  it('division, feedback and mix each send their own setPatch field', async () => {
    const { host, dispatch } = await mount();
    await change(field<HTMLSelectElement>(host, 'Delay division'), 'eighthTriplet');
    expect(dispatch).toHaveBeenLastCalledWith({ name: 'setPatch', args: { delayDivision: 'eighthTriplet' } });
    await change(field(host, 'Delay feedback'), '0.6');
    expect(dispatch).toHaveBeenLastCalledWith({ name: 'setPatch', args: { delayFeedback: 0.6 } });
    await change(field(host, 'Delay mix'), '0.5');
    expect(dispatch).toHaveBeenLastCalledWith({ name: 'setPatch', args: { delayMix: 0.5 } });
    await change(field(host, 'Delay time'), '0.3');
    expect(dispatch).toHaveBeenLastCalledWith({ name: 'setPatch', args: { delayTimeSeconds: 0.3 } });
  });

  it('Sync on sends delaySynced and keeps the manual time in draft.lastManualDelay', async () => {
    const { host, dispatch, store } = await mount();
    await change(field(host, 'Delay time'), '0.3');
    await click(field(host, 'Delay sync'));
    await settle();
    expect(dispatch).toHaveBeenLastCalledWith({ name: 'setPatch', args: { delaySynced: true } });
    expect(store.getState().draft.lastManualDelay).toBe(0.3);
  });

  it('Time is disabled while Sync is on and enabled again after Sync off', async () => {
    const { host } = await mount();
    expect(field(host, 'Delay time').disabled).toBe(false);
    await click(field(host, 'Delay sync'));
    await settle();
    expect(field(host, 'Delay sync').checked).toBe(true);
    expect(field(host, 'Delay time').disabled).toBe(true);
    await click(field(host, 'Delay sync'));
    await settle();
    expect(field(host, 'Delay time').disabled).toBe(false);
  });

  it('Sync off restores the last manual delay time in the same setPatch (spec "Sync memory")', async () => {
    const { host, dispatch, bridge } = await mount();
    await change(field(host, 'Delay time'), '0.3');
    await click(field(host, 'Delay sync'));
    await settle();
    // the host recomputes the synced time when the tempo changes
    await act(async () => void (await bridge.dispatch({ name: 'setBpm', args: { bpm: 90 } })));
    expect(Number(field(host, 'Delay time').value)).toBeCloseTo(60 / 90, 2);
    await click(field(host, 'Delay sync'));
    await settle();
    expect(dispatch).toHaveBeenLastCalledWith({ name: 'setPatch', args: { delaySynced: false, delayTimeSeconds: 0.3 } });
    expect(Number(field(host, 'Delay time').value)).toBeCloseTo(0.3, 6);
  });

  it('Sync off after the HOST synced restores the last unsynced time seen in snapshots (legacy seeds Free memory from loads)', async () => {
    const { host, dispatch } = await mount();
    await change(field(host, 'Delay time'), '0.25');
    await act(async () => void (await dispatch({ name: 'setPatch', args: { delaySynced: true } })));
    dispatch.mockClear();
    await click(field(host, 'Delay sync'));
    await settle();
    expect(dispatch).toHaveBeenCalledTimes(1);
    expect(dispatch).toHaveBeenLastCalledWith({ name: 'setPatch', args: { delaySynced: false, delayTimeSeconds: 0.25 } });
  });

  it('Sync off when the panel only ever saw a synced patch falls back to the default patch time 0.3 (kDefaultPatch)', async () => {
    const base = createMockBridge();
    const synced = { ...(await base.getSnapshot())! };
    synced.patch = { ...synced.patch, delaySynced: true, delayTimeSeconds: 0.8 };
    const harness = makeHarness({ getSnapshot: async () => synced });
    const mounted = await render(<DelayPanel />, harness);
    unmount = mounted.unmount;
    expect(field(mounted.host, 'Delay sync').checked).toBe(true);
    await click(field(mounted.host, 'Delay sync'));
    await settle();
    expect(harness.dispatch).toHaveBeenLastCalledWith({ name: 'setPatch', args: { delaySynced: false, delayTimeSeconds: 0.3 } });
  });

  it('shows the recommendations and updates them with the BPM', async () => {
    const { host, bridge } = await mount();
    expect(recommendations(host)).toContain('1/4 500 ms');
    expect(recommendations(host)).toContain('1/8T 167 ms | 1/16 125 ms');
    await act(async () => void (await bridge.dispatch({ name: 'setBpm', args: { bpm: 150 } })));
    expect(recommendations(host)).toContain('1/4 400 ms');
  });

  it('every delay control is disabled while FX is off, and enabled with FX on', async () => {
    const { host, bridge } = await mount();
    const labels = ['Delay sync', 'Delay division', 'Delay time', 'Delay feedback', 'Delay mix'];
    for (const label of labels) expect(field(host, label).disabled).toBe(false);
    await act(async () => void (await bridge.dispatch({ name: 'setEffectsEnabled', args: { enabled: false } })));
    for (const label of labels) expect(field(host, label).disabled).toBe(true);
    await act(async () => void (await bridge.dispatch({ name: 'setEffectsEnabled', args: { enabled: true } })));
    for (const label of labels) expect(field(host, label).disabled).toBe(false);
  });
});
