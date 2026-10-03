// @vitest-environment jsdom
import { describe, it, expect, afterEach } from 'vitest';
import { act } from 'react';
import { RhythmPanel } from './RhythmPanel';
import { change, field, makeHarness, render } from '../testing/harness';
import { RHYTHM_MODES } from '../bridge/protocol';

let unmount = () => {};
afterEach(() => unmount());
const mount = async () => {
  const mounted = await render(<RhythmPanel />);
  unmount = mounted.unmount;
  return mounted;
};
const options = (select: HTMLSelectElement) => [...select.options].map((o) => o.value);

describe('RhythmPanel', () => {
  it('fields reflect the snapshot', async () => {
    const { host } = await mount();
    expect(field<HTMLSelectElement>(host, 'Rhythm mode').value).toBe('random');
    expect(field(host, 'Pulses').value).toBe('5');
    expect(field(host, 'Rotation').value).toBe('0');
    expect(field(host, 'Chance %').value).toBe('50'); // stepProbability 0.5 shown as a percentage
  });

  it('follows a changed snapshot (mode probability, chance 0.25)', async () => {
    const { host, bridge } = await mount();
    await act(async () => void (await bridge.dispatch({ name: 'setGenerationParams', args: { mode: 'probability', stepProbability: 0.25 } })));
    expect(field<HTMLSelectElement>(host, 'Rhythm mode').value).toBe('probability');
    expect(field(host, 'Chance %').value).toBe('25');
  });

  it('offers exactly the rhythm modes', async () => {
    const { host } = await mount();
    expect(options(field<HTMLSelectElement>(host, 'Rhythm mode'))).toEqual([...RHYTHM_MODES]);
  });

  it('Live send: mode sends setGenerationParams at once, without Generate', async () => {
    const { host, dispatch } = await mount();
    await change(field<HTMLSelectElement>(host, 'Rhythm mode'), 'euclidean');
    expect(dispatch).toHaveBeenCalledTimes(1);
    expect(dispatch).toHaveBeenLastCalledWith({ name: 'setGenerationParams', args: { mode: 'euclidean' } });
    expect(field<HTMLSelectElement>(host, 'Rhythm mode').value).toBe('euclidean');
  });

  it('Live send: pulses, rotation and chance send their own field only', async () => {
    const { host, dispatch } = await mount();
    await change(field(host, 'Pulses'), '9');
    expect(dispatch).toHaveBeenLastCalledWith({ name: 'setGenerationParams', args: { pulses: 9 } });
    await change(field(host, 'Rotation'), '3');
    expect(dispatch).toHaveBeenLastCalledWith({ name: 'setGenerationParams', args: { rotation: 3 } });
    await change(field(host, 'Chance %'), '25');
    expect(dispatch).toHaveBeenLastCalledWith({ name: 'setGenerationParams', args: { stepProbability: 0.25 } });
    expect(dispatch).toHaveBeenCalledTimes(3); // no regenerate
    expect(field(host, 'Chance %').value).toBe('25');
  });

  it('a failed edit reverts the field to the engine value', async () => {
    const failing = makeHarness({ dispatch: async () => ({ ok: false, error: 'busy' }) });
    const mounted = await render(<RhythmPanel />, failing);
    unmount = mounted.unmount;
    await change(field(mounted.host, 'Pulses'), '12');
    expect(field(mounted.host, 'Pulses').value).toBe('5');
  });
});
