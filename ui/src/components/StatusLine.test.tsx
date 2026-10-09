// @vitest-environment jsdom
import { describe, it, expect, afterEach } from 'vitest';
import { act } from 'react';
import { StatusLine } from './StatusLine';
import { SEED_ERROR } from '../lib/seed';
import { makeHarness, render, settle } from '../testing/harness';

let unmount = () => {};
afterEach(() => unmount());
const mount = async (harness = makeHarness()) => {
  const mounted = await render(<StatusLine />, harness);
  unmount = mounted.unmount;
  return mounted;
};
const line = (host: HTMLElement) => host.querySelector('[role="status"], [role="alert"]')!;

describe('StatusLine', () => {
  it('is empty until something happens', async () => {
    const { host } = await mount();
    expect(line(host).textContent).toBe('');
    expect(line(host).getAttribute('role')).toBe('status');
  });

  it('shows a success message as a plain status', async () => {
    const { host, store } = await mount();
    await act(async () => store.setStatus({ text: 'Generated.', error: false }));
    expect(line(host).textContent).toBe('Generated.');
    expect(line(host).getAttribute('role')).toBe('status');
  });

  it('shows an error message in the error style (role alert), including the seed message', async () => {
    const { host, store } = await mount();
    await act(async () => store.setStatus({ text: SEED_ERROR, error: true }));
    expect(line(host).textContent).toBe('Seed must be a whole number.');
    expect(line(host).getAttribute('role')).toBe('alert');

    await act(async () => store.setStatus(null));
    expect(line(host).textContent).toBe('');
  });

  it('shows the mapped message of a failed command', async () => {
    const harness = makeHarness({}, { busy: true });
    const { host, store } = await mount(harness);
    store.send('regenerate', { name: 'regenerate', args: { randomize: false } }, false);
    await settle();
    expect(line(host).textContent).toBe('Busy, try again');
    expect(line(host).getAttribute('role')).toBe('alert');
  });
});
