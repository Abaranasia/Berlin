// @vitest-environment jsdom
import { describe, it, expect, vi, afterEach } from 'vitest';
import { act } from 'react';
import { DelayTimesCard } from './DelayTimesCard';
import { StatusLine } from './StatusLine';
import { button, click, render, settle } from '../testing/harness';
import type { CopyText } from '../lib/clipboard';

let unmount = () => {};
afterEach(() => {
  unmount();
  vi.restoreAllMocks();
  Reflect.deleteProperty(navigator, 'clipboard');
});
const mount = async (copy?: CopyText) => {
  const mounted = await render(
    <>
      <DelayTimesCard copy={copy} />
      <StatusLine />
    </>,
  );
  unmount = mounted.unmount;
  return mounted;
};
const chips = (host: HTMLElement) => [...host.querySelectorAll('button')].map((b) => b.textContent);
const status = (host: HTMLElement) => host.querySelector('[role="status"], [role="alert"]')!;

describe('DelayTimesCard chips', () => {
  it('renders six buttons at 120 BPM with the label and the whole milliseconds', async () => {
    const { host } = await mount();
    expect(chips(host)).toEqual(['1/2 1000 ms', '1/4 500 ms', '1/8. 375 ms', '1/8 250 ms', '1/8T 167 ms', '1/16 125 ms']);
  });

  it('follows the BPM (150 BPM: 1/4 is 400 ms)', async () => {
    const { host, bridge } = await mount();
    await act(async () => void (await bridge.dispatch({ name: 'setBpm', args: { bpm: 150 } })));
    expect(chips(host)).toContain('1/4 400 ms');
    expect(chips(host)).not.toContain('1/4 500 ms');
  });
});

describe('DelayTimesCard copy', () => {
  it('copies the integer ms text of the clicked chip and reports success', async () => {
    const copy = vi.fn<CopyText>(() => Promise.resolve());
    const { host } = await mount(copy);
    await click(button(host, '1/4 500 ms'));
    await settle();
    expect(copy).toHaveBeenCalledTimes(1);
    expect(copy).toHaveBeenCalledWith('500');
    expect(status(host).textContent).toBe('Copied 500 ms');
    expect(status(host).getAttribute('role')).toBe('status');
    await click(button(host, '1/8T 167 ms'));
    await settle();
    expect(copy).toHaveBeenLastCalledWith('167');
    expect(status(host).textContent).toBe('Copied 167 ms');
  });

  it('shows "Copy failed" as an alert when the copy rejects', async () => {
    const { host } = await mount(() => Promise.reject(new Error('denied')));
    await click(button(host, '1/4 500 ms'));
    await settle();
    expect(status(host).textContent).toBe('Copy failed');
    expect(status(host).getAttribute('role')).toBe('alert');
  });

  it('shows "Copy failed" when the copy throws synchronously, without an unhandled rejection', async () => {
    const unhandled = vi.fn();
    process.on('unhandledRejection', unhandled);
    try {
      const { host } = await mount(() => {
        throw new Error('sync');
      });
      await click(button(host, '1/2 1000 ms'));
      await settle();
      await new Promise((resolve) => setTimeout(resolve, 0));
      expect(status(host).textContent).toBe('Copy failed');
      expect(unhandled).not.toHaveBeenCalled();
    } finally {
      process.off('unhandledRejection', unhandled);
    }
  });

  it('with the default copy and no navigator.clipboard shows "Copy failed"', async () => {
    expect(navigator.clipboard).toBeUndefined();
    const { host } = await mount();
    await click(button(host, '1/4 500 ms'));
    await settle();
    expect(status(host).textContent).toBe('Copy failed');
  });

  it('with the default copy writes the integer ms text to navigator.clipboard', async () => {
    const writeText = vi.fn(() => Promise.resolve());
    Object.defineProperty(navigator, 'clipboard', { value: { writeText }, configurable: true });
    const { host } = await mount();
    await click(button(host, '1/16 125 ms'));
    await settle();
    expect(writeText).toHaveBeenCalledTimes(1);
    expect(writeText).toHaveBeenCalledWith('125');
    expect(status(host).textContent).toBe('Copied 125 ms');
  });
});
