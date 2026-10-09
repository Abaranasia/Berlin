// @vitest-environment jsdom
import { describe, it, expect, afterEach } from 'vitest';
import { act } from 'react';
import { ReverbPanel } from './ReverbPanel';
import { change, field, render } from '../testing/harness';

let unmount = () => {};
afterEach(() => unmount());
const mount = async () => {
  const mounted = await render(<ReverbPanel />);
  unmount = mounted.unmount;
  return mounted;
};

describe('ReverbPanel', () => {
  it('fields reflect the snapshot', async () => {
    const { host } = await mount();
    expect(field(host, 'Room').value).toBe('0.4');
    expect(field(host, 'Damping').value).toBe('0.5');
    expect(field(host, 'Wet').value).toBe('0.2');
    expect(field(host, 'Dry').value).toBe('0.8');
  });

  it('Wet change sends setPatch {reverbWetLevel} only', async () => {
    const { host, dispatch } = await mount();
    await change(field(host, 'Wet'), '0.55');
    expect(dispatch).toHaveBeenCalledTimes(1);
    expect(dispatch).toHaveBeenLastCalledWith({ name: 'setPatch', args: { reverbWetLevel: 0.55 } });
    expect(field(host, 'Wet').value).toBe('0.55');
  });

  it('Room, Damping and Dry each send their own field', async () => {
    const { host, dispatch } = await mount();
    await change(field(host, 'Room'), '0.9');
    expect(dispatch).toHaveBeenLastCalledWith({ name: 'setPatch', args: { reverbRoomSize: 0.9 } });
    await change(field(host, 'Damping'), '0.1');
    expect(dispatch).toHaveBeenLastCalledWith({ name: 'setPatch', args: { reverbDamping: 0.1 } });
    await change(field(host, 'Dry'), '0.3');
    expect(dispatch).toHaveBeenLastCalledWith({ name: 'setPatch', args: { reverbDryLevel: 0.3 } });
  });

  it('every reverb control is disabled while FX is off, and enabled with FX on', async () => {
    const { host, bridge } = await mount();
    const labels = ['Room', 'Damping', 'Wet', 'Dry'];
    for (const label of labels) expect(field(host, label).disabled).toBe(false);
    await act(async () => void (await bridge.dispatch({ name: 'setEffectsEnabled', args: { enabled: false } })));
    for (const label of labels) expect(field(host, label).disabled).toBe(true);
    await act(async () => void (await bridge.dispatch({ name: 'setEffectsEnabled', args: { enabled: true } })));
    for (const label of labels) expect(field(host, label).disabled).toBe(false);
  });
});
