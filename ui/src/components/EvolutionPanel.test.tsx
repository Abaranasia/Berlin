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

  it('Rate offers exactly the legacy choices 1, 2, 4, 8 and 16 loops', async () => {
    const { host } = await mount();
    const select = field<HTMLSelectElement>(host, 'Evolve rate');
    expect([...select.options].map((o) => o.value)).toEqual(['1', '2', '4', '8', '16']);
    expect([...select.options].map((o) => o.text)).toEqual(['Every 1 loops', 'Every 2 loops', 'Every 4 loops', 'Every 8 loops', 'Every 16 loops']);
  });

  it('Rate sends setAutoEvolveRate with the chosen whole number', async () => {
    const { host, dispatch } = await mount();
    await change(field<HTMLSelectElement>(host, 'Evolve rate'), '8');
    expect(dispatch).toHaveBeenLastCalledWith({ name: 'setAutoEvolveRate', args: { rate: 8 } });
    expect(field(host, 'Evolve rate').value).toBe('8');
    await change(field<HTMLSelectElement>(host, 'Evolve rate'), '16');
    expect(dispatch).toHaveBeenLastCalledWith({ name: 'setAutoEvolveRate', args: { rate: 16 } });
  });

  it('Evolve updates the UI via a snapshot event (mutationCount)', async () => {
    const { host, bridge } = await mount();
    // The mock engine emits a snapshot event on every dispatch; this one bypasses the store, like host-side evolution.
    await act(async () => void (await bridge.dispatch({ name: 'mutate', args: {} })));
    await act(async () => void (await bridge.dispatch({ name: 'mutate', args: {} })));
    expect(host.textContent).toContain('Mutations: 2');
  });
});
