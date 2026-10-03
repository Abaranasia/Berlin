// @vitest-environment jsdom
import { describe, it, expect, afterEach } from 'vitest';
import { EngineToggles } from './EngineToggles';
import { click, field, render, settle } from '../testing/harness';

let unmount = () => {};
afterEach(() => unmount());
const mount = async () => {
  const mounted = await render(<EngineToggles />);
  unmount = mounted.unmount;
  return mounted;
};

describe('EngineToggles', () => {
  it('reflects the snapshot (synth on, FX on)', async () => {
    const { host } = await mount();
    expect(field(host, 'Synth').checked).toBe(true);
    expect(field(host, 'FX').checked).toBe(true);
  });

  it('Toggle FX sends setEffectsEnabled {enabled:false}, then true again', async () => {
    const { host, dispatch } = await mount();
    await click(field(host, 'FX'));
    expect(dispatch).toHaveBeenLastCalledWith({ name: 'setEffectsEnabled', args: { enabled: false } });
    expect(field(host, 'FX').checked).toBe(false);
    expect(field(host, 'Synth').checked).toBe(true); // the other toggle is untouched

    await click(field(host, 'FX'));
    expect(dispatch).toHaveBeenLastCalledWith({ name: 'setEffectsEnabled', args: { enabled: true } });
    await settle();
    expect(field(host, 'FX').checked).toBe(true);
  });

  it('Synth sends setSynthEnabled', async () => {
    const { host, dispatch } = await mount();
    await click(field(host, 'Synth'));
    expect(dispatch).toHaveBeenLastCalledWith({ name: 'setSynthEnabled', args: { enabled: false } });
    expect(field(host, 'Synth').checked).toBe(false);
  });
});
