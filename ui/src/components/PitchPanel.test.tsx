// @vitest-environment jsdom
import { describe, it, expect, afterEach } from 'vitest';
import { act } from 'react';
import { PitchPanel } from './PitchPanel';
import { change, field, makeHarness, render, settle } from '../testing/harness';
import { SCALE_TYPES, type DispatchResult } from '../bridge/protocol';

let unmount = () => {};
afterEach(() => unmount());
const mount = async (harness = makeHarness()) => {
  const mounted = await render(<PitchPanel />, harness);
  unmount = mounted.unmount;
  return mounted;
};
const options = (select: HTMLSelectElement) => [...select.options].map((o) => o.value);

describe('PitchPanel: scale and root', () => {
  it('scale choices equal the ScaleType list; the snapshot scale is selected', async () => {
    const { host } = await mount();
    const scale = field<HTMLSelectElement>(host, 'Scale');
    expect(options(scale)).toEqual([...SCALE_TYPES]);
    expect(scale.value).toBe('minor');
  });

  it('choosing a scale sends setGenerationParams {scaleType}', async () => {
    const { host, dispatch } = await mount();
    await change(field<HTMLSelectElement>(host, 'Scale'), 'dorian');
    expect(dispatch).toHaveBeenLastCalledWith({ name: 'setGenerationParams', args: { scaleType: 'dorian' } });
    expect(field<HTMLSelectElement>(host, 'Scale').value).toBe('dorian');
  });

  it('root offers the 12 pitch classes by note name and sends the numeric pitch class', async () => {
    const { host, dispatch } = await mount();
    const root = field<HTMLSelectElement>(host, 'Root');
    expect([...root.options].map((o) => o.textContent)).toEqual(['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']);
    await change(root, '7');
    expect(dispatch).toHaveBeenLastCalledWith({ name: 'setGenerationParams', args: { rootPitchClass: 7 } });
    expect(field<HTMLSelectElement>(host, 'Root').value).toBe('7');
  });
});

describe('PitchPanel: range', () => {
  it('shows the snapshot range 36..72', async () => {
    const { host } = await mount();
    expect([field(host, 'Range low').value, field(host, 'Range high').value]).toEqual(['36', '72']);
  });

  it('Range sent together: changing Low sends both rangeLow and rangeHigh in one command', async () => {
    const { host, dispatch } = await mount();
    await change(field(host, 'Range low'), '40');
    expect(dispatch).toHaveBeenCalledTimes(1);
    expect(dispatch).toHaveBeenLastCalledWith({ name: 'setGenerationParams', args: { rangeLow: 40, rangeHigh: 72 } });
  });

  it('changing High sends the pair with the current Low', async () => {
    const { host, dispatch } = await mount();
    await change(field(host, 'Range high'), '90');
    expect(dispatch).toHaveBeenLastCalledWith({ name: 'setGenerationParams', args: { rangeLow: 36, rangeHigh: 90 } });
  });

  it('uses the optimistic partner while a request is in flight (no stale half of the pair)', async () => {
    const resolvers: ((r: DispatchResult) => void)[] = [];
    const harness = makeHarness({ dispatch: () => new Promise<DispatchResult>((resolve) => resolvers.push(resolve)) });
    const { host, dispatch } = await mount(harness);
    await change(field(host, 'Range low'), '40'); // in flight
    await change(field(host, 'Range high'), '90'); // pending, must carry Low 40
    expect([field(host, 'Range low').value, field(host, 'Range high').value]).toEqual(['40', '90']);

    await act(async () => resolvers[0]({ ok: false, error: 'busy' }));
    expect(dispatch).toHaveBeenLastCalledWith({ name: 'setGenerationParams', args: { rangeLow: 40, rangeHigh: 90 } });
  });

  it('shows the host-normalized pair after the response (range clamps together)', async () => {
    const { host } = await mount();
    await change(field(host, 'Range low'), '70'); // 70..72 is narrower than the minimum span, so the host widens it
    await settle();
    expect(field(host, 'Range low').value).toBe('70');
    expect(field(host, 'Range high').value).toBe('82');
  });
});
