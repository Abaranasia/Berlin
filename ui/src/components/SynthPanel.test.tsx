// @vitest-environment jsdom
import { describe, it, expect, afterEach } from 'vitest';
import { act } from 'react';
import { SynthPanel } from './SynthPanel';
import { change, field, makeHarness, render } from '../testing/harness';
import { LFO_DESTINATIONS, WAVEFORMS, type Patch } from '../bridge/protocol';
import { LIMITS } from '../bridge/limits';
import { SKEWS } from '../lib/skew';

let unmount = () => {};
afterEach(() => unmount());
const mount = async () => {
  const mounted = await render(<SynthPanel />);
  unmount = mounted.unmount;
  return mounted;
};
const options = (select: HTMLSelectElement) => [...select.options].map((o) => o.value);
const lastPatch = (dispatch: { mock: { lastCall?: unknown[] } }) => (dispatch.mock.lastCall![0] as { name: string; args: Partial<Patch> });

describe('SynthPanel', () => {
  it('fields reflect the snapshot (skewed ones as slider positions)', async () => {
    const { host } = await mount();
    expect(field<HTMLSelectElement>(host, 'Waveform').value).toBe('saw');
    expect(field<HTMLSelectElement>(host, 'LFO destination').value).toBe('cutoff');
    expect(Number(field(host, 'Cutoff').value)).toBeCloseTo(0.5, 6); // cutoff 1000 is the skew midpoint
    expect(Number(field(host, 'Resonance').value)).toBeCloseTo(SKEWS.resonance.toPosition(1), 6);
    expect(field(host, 'Pulse width').value).toBe('0.5');
    expect(field(host, 'Sustain').value).toBe('0.7');
  });

  it('offers exactly the waveforms and LFO destinations', async () => {
    const { host } = await mount();
    expect(options(field<HTMLSelectElement>(host, 'Waveform'))).toEqual([...WAVEFORMS]);
    expect(options(field<HTMLSelectElement>(host, 'LFO destination'))).toEqual([...LFO_DESTINATIONS]);
  });

  it('selecting a waveform dispatches setPatch {waveform}', async () => {
    const { host, dispatch } = await mount();
    await change(field<HTMLSelectElement>(host, 'Waveform'), 'pulse');
    expect(dispatch).toHaveBeenCalledTimes(1);
    expect(dispatch).toHaveBeenLastCalledWith({ name: 'setPatch', args: { waveform: 'pulse' } });
    expect(field<HTMLSelectElement>(host, 'Waveform').value).toBe('pulse');
  });

  it('cutoff uses the skew: position 0.5 is 1000, the ends are the limits', async () => {
    const { host, dispatch } = await mount();
    await change(field(host, 'Cutoff'), '0.25');
    const quarter = lastPatch(dispatch);
    expect(quarter.name).toBe('setPatch');
    expect(Object.keys(quarter.args)).toEqual(['cutoffHz']);
    expect(quarter.args.cutoffHz).toBeCloseTo(SKEWS.cutoffHz.toValue(0.25), 6);
    expect(quarter.args.cutoffHz).toBeGreaterThan(LIMITS.cutoffHz.min);
    expect(quarter.args.cutoffHz).toBeLessThan(1000);
    await change(field(host, 'Cutoff'), '1');
    expect(lastPatch(dispatch).args).toEqual({ cutoffHz: LIMITS.cutoffHz.max });
    await change(field(host, 'Cutoff'), '0');
    expect(lastPatch(dispatch).args).toEqual({ cutoffHz: LIMITS.cutoffHz.min });
  });

  it.each([
    ['Resonance', 'resonance', SKEWS.resonance, 2],
    ['Attack', 'attack', SKEWS.attack, 0.2],
    ['Decay', 'decay', SKEWS.decay, 0.3],
    ['Release', 'release', SKEWS.release, 0.5],
    ['LFO rate', 'lfoRateHz', SKEWS.lfoRateHz, 2],
  ] as const)('%s slider sends its field through the skew (midpoint %d)', async (label, key, skew, midpoint) => {
    const { host, dispatch } = await mount();
    await change(field(host, label), '0.75');
    const sent = lastPatch(dispatch);
    expect(Object.keys(sent.args)).toEqual([key]);
    expect(sent.args[key]).toBeCloseTo(skew.toValue(0.75), 9);
    expect(sent.args[key]).toBeGreaterThan(midpoint); // above the midpoint, so not a linear or unmapped value
    expect(Number(field(host, label).value)).toBeCloseTo(0.75, 6);
  });

  it('pulse width, sustain and LFO depth send their own field, unskewed', async () => {
    const { host, dispatch } = await mount();
    await change(field(host, 'Pulse width'), '0.3');
    expect(dispatch).toHaveBeenLastCalledWith({ name: 'setPatch', args: { pulseWidth: 0.3 } });
    await change(field(host, 'Sustain'), '0.4');
    expect(dispatch).toHaveBeenLastCalledWith({ name: 'setPatch', args: { sustain: 0.4 } });
    await change(field(host, 'LFO depth'), '0.6');
    expect(dispatch).toHaveBeenLastCalledWith({ name: 'setPatch', args: { lfoDepth: 0.6 } });
    expect(field(host, 'Pulse width').value).toBe('0.3');
  });

  it('LFO destination sends setPatch {lfoDestination}', async () => {
    const { host, dispatch } = await mount();
    await change(field<HTMLSelectElement>(host, 'LFO destination'), 'pitch');
    expect(dispatch).toHaveBeenLastCalledWith({ name: 'setPatch', args: { lfoDestination: 'pitch' } });
  });

  it('follows a changed snapshot', async () => {
    const { host, bridge } = await mount();
    await act(async () => void (await bridge.dispatch({ name: 'setPatch', args: { waveform: 'triangle', cutoffHz: 20000 } })));
    expect(field<HTMLSelectElement>(host, 'Waveform').value).toBe('triangle');
    expect(Number(field(host, 'Cutoff').value)).toBeCloseTo(1, 9);
  });

  it('a failed edit reverts the slider to the engine position', async () => {
    const failing = makeHarness({ dispatch: async () => ({ ok: false, error: 'busy' }) });
    const mounted = await render(<SynthPanel />, failing);
    unmount = mounted.unmount;
    await change(field(mounted.host, 'Cutoff'), '1');
    expect(Number(field(mounted.host, 'Cutoff').value)).toBeCloseTo(0.5, 6);
  });
});
