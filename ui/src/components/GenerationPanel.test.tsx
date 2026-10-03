// @vitest-environment jsdom
import { describe, it, expect, afterEach } from 'vitest';
import { act } from 'react';
import { GenerationPanel } from './GenerationPanel';
import { StatusLine } from './StatusLine';
import { blur, button, change, click, field, makeHarness, pressEnter, render, settle } from '../testing/harness';

let unmount = () => {};
afterEach(() => unmount());
const mount = async (harness = makeHarness()) => {
  const mounted = await render(
    <>
      <GenerationPanel />
      <StatusLine />
    </>,
    harness,
  );
  unmount = mounted.unmount;
  return mounted;
};
const status = (host: HTMLElement) => host.querySelector('[role="status"], [role="alert"]')!;

describe('GenerationPanel: Generate, Randomize, Mutate', () => {
  it('Generate then Randomize dispatch regenerate with randomize false then true, with success messages', async () => {
    const { host, dispatch } = await mount();
    await click(button(host, 'Generate'));
    expect(dispatch).toHaveBeenLastCalledWith({ name: 'regenerate', args: { randomize: false } });
    expect(status(host).textContent).toBe('Generated.');

    await click(button(host, 'Randomize'));
    expect(dispatch).toHaveBeenLastCalledWith({ name: 'regenerate', args: { randomize: true } });
    expect(status(host).textContent).toBe('Randomized.');
  });

  it('Mutate dispatches mutate and reports Mutated.', async () => {
    const { host, dispatch } = await mount();
    await click(button(host, 'Mutate'));
    expect(dispatch).toHaveBeenLastCalledWith({ name: 'mutate', args: {} });
    expect(status(host).textContent).toBe('Mutated.');
  });

  it('reports Busy, try again when the host is busy', async () => {
    const { host } = await mount(makeHarness({}, { busy: true }));
    await click(button(host, 'Mutate'));
    expect(status(host).textContent).toBe('Busy, try again');
  });
});

describe('GenerationPanel: Lock Seed', () => {
  it('sends setGenerationParams {lockSeed} and disables Randomize while locked', async () => {
    const { host, dispatch } = await mount();
    expect(button(host, 'Randomize').disabled).toBe(false);

    await click(field(host, 'Lock Seed'));
    expect(dispatch).toHaveBeenLastCalledWith({ name: 'setGenerationParams', args: { lockSeed: true } });
    expect(button(host, 'Randomize').disabled).toBe(true);
    expect(button(host, 'Generate').disabled).toBe(false);

    await click(field(host, 'Lock Seed'));
    expect(dispatch).toHaveBeenLastCalledWith({ name: 'setGenerationParams', args: { lockSeed: false } });
    await settle();
    expect(button(host, 'Randomize').disabled).toBe(false);
  });
});

describe('GenerationPanel: seed field', () => {
  it('shows the snapshot seed', async () => {
    const { host } = await mount();
    expect(field(host, 'Seed').value).toBe('42');
  });

  it('a valid seed sends setSeed with the raw string, on Enter and on blur', async () => {
    const { host, dispatch } = await mount();
    await change(field(host, 'Seed'), '-123');
    expect(dispatch).not.toHaveBeenCalled();
    await pressEnter(field(host, 'Seed'));
    expect(dispatch).toHaveBeenLastCalledWith({ name: 'setSeed', args: { seed: '-123' } });

    await change(field(host, 'Seed'), '7');
    await blur(field(host, 'Seed'));
    expect(dispatch).toHaveBeenLastCalledWith({ name: 'setSeed', args: { seed: '7' } });
  });

  it('a large seed is preserved as a string end to end', async () => {
    const { host, dispatch } = await mount();
    await change(field(host, 'Seed'), '9223372036854770000');
    await pressEnter(field(host, 'Seed'));
    expect(dispatch).toHaveBeenLastCalledWith({ name: 'setSeed', args: { seed: '9223372036854770000' } });
    await settle();
    expect(field(host, 'Seed').value).toBe('9223372036854770000');
  });

  it('an invalid seed dispatches nothing and shows the error message; a valid one clears it', async () => {
    const { host, dispatch } = await mount();
    await change(field(host, 'Seed'), '12a');
    await pressEnter(field(host, 'Seed'));
    expect(dispatch).not.toHaveBeenCalled();
    expect(status(host).textContent).toBe('Seed must be a whole number.');
    expect(status(host).getAttribute('role')).toBe('alert');
    expect(field(host, 'Seed').value).toBe('12a'); // kept for correction

    await change(field(host, 'Seed'), '12');
    await pressEnter(field(host, 'Seed'));
    expect(dispatch).toHaveBeenLastCalledWith({ name: 'setSeed', args: { seed: '12' } });
    expect(status(host).textContent).toBe('');
  });

  it('surrounding whitespace is trimmed first and the TRIMMED string is sent (legacy)', async () => {
    const { host, dispatch } = await mount();
    await change(field(host, 'Seed'), ' 5');
    await pressEnter(field(host, 'Seed'));
    expect(dispatch).toHaveBeenLastCalledWith({ name: 'setSeed', args: { seed: '5' } });
    expect(field(host, 'Seed').value).toBe('5');
    await change(field(host, 'Seed'), ' -9223372036854770000  ');
    await blur(field(host, 'Seed'));
    expect(dispatch).toHaveBeenLastCalledWith({ name: 'setSeed', args: { seed: '-9223372036854770000' } });
  });

  it('a blank seed is invalid after trimming', async () => {
    const { host, dispatch } = await mount();
    dispatch.mockClear();
    await change(field(host, 'Seed'), '   ');
    await pressEnter(field(host, 'Seed'));
    expect(dispatch).not.toHaveBeenCalled();
    expect(status(host).textContent).toBe('Seed must be a whole number.');
  });

  it('a valid seed does not clear an unrelated status message', async () => {
    const { host, store } = await mount();
    await act(async () => store.setStatus({ text: 'Mutated.', error: false }));
    await change(field(host, 'Seed'), '5');
    await pressEnter(field(host, 'Seed'));
    expect(store.getState().status).toEqual({ text: 'Mutated.', error: false });
  });
});
