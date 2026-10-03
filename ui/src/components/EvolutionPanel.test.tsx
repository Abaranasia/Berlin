// @vitest-environment jsdom
import { describe, it, expect, afterEach } from 'vitest';
import { act } from 'react';
import { EvolutionPanel } from './EvolutionPanel';
import { change, click, field, render } from '../testing/harness';

let unmount = () => {};
afterEach(() => unmount());
const mount = async () => {
  const mounted = await render(<EvolutionPanel />);
  unmount = mounted.unmount;
  return mounted;
};

describe('EvolutionPanel', () => {
  it('reflects the snapshot (off, rate 4, 0 mutations)', async () => {
    const { host } = await mount();
    expect(field(host, 'Auto-Evolve').checked).toBe(false);
    expect(field(host, 'Evolve rate').value).toBe('4');
    expect(host.textContent).toContain('Mutations: 0');
  });

  it('Enable evolve sends setAutoEvolveEnabled {enabled:true}', async () => {
    const { host, dispatch } = await mount();
    await click(field(host, 'Auto-Evolve'));
    expect(dispatch).toHaveBeenLastCalledWith({ name: 'setAutoEvolveEnabled', args: { enabled: true } });
    expect(field(host, 'Auto-Evolve').checked).toBe(true);
  });

  it('Rate sends setAutoEvolveRate with the whole-number value', async () => {
    const { host, dispatch } = await mount();
    await change(field(host, 'Evolve rate'), '9');
    expect(dispatch).toHaveBeenLastCalledWith({ name: 'setAutoEvolveRate', args: { rate: 9 } });
    expect(field(host, 'Evolve rate').value).toBe('9');
  });

  it('Evolve updates the UI via a snapshot event (mutationCount)', async () => {
    const { host, bridge } = await mount();
    // The mock engine emits a snapshot event on every dispatch; this one bypasses the store, like host-side evolution.
    await act(async () => void (await bridge.dispatch({ name: 'mutate', args: {} })));
    await act(async () => void (await bridge.dispatch({ name: 'mutate', args: {} })));
    expect(host.textContent).toContain('Mutations: 2');
  });
});
