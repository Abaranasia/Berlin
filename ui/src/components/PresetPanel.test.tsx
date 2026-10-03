// @vitest-environment jsdom
import { describe, it, expect, afterEach, vi } from 'vitest';
import { act } from 'react';
import { PresetPanel } from './PresetPanel';
import { button, change, click, field, makeHarness, render, settle } from '../testing/harness';
import { createMockBridge } from '../bridge/mock';
import type { Snapshot } from '../bridge/protocol';

let unmount = () => {};
afterEach(() => unmount());

const mount = async (over: Parameters<typeof makeHarness>[0] = {}, options: Parameters<typeof makeHarness>[1] = {}) => {
  const mounted = await render(<PresetPanel />, makeHarness(over, options));
  unmount = mounted.unmount;
  return mounted;
};
const nameField = (host: HTMLElement) => field(host, 'Preset name');
const list = (host: HTMLElement) => field<HTMLSelectElement>(host, 'Presets');
const names = (host: HTMLElement) => [...list(host).options].map((o) => o.value).filter((v) => v !== '');
const status = (store: { getState(): { status: { text: string; error: boolean } | null } }) => store.getState().status;
const save = async (host: HTMLElement, name: string) => {
  await change(nameField(host), name);
  await click(button(host, 'Save'));
  await settle();
};
// A snapshot that already lists these presets (the mock engine knows them too once saved through it).
const withPresets = (presetNames: string[]) => async (): Promise<Snapshot> => ({ ...(await createMockBridge().getSnapshot())!, presetNames });

describe('PresetPanel: disabled states', () => {
  it('Save is disabled with an empty name and with a whitespace-only name; Load with no selection', async () => {
    const { host } = await mount();
    expect(button(host, 'Save').disabled).toBe(true);
    expect(button(host, 'Load').disabled).toBe(true);
    await change(nameField(host), '   ');
    expect(button(host, 'Save').disabled).toBe(true);
    await change(nameField(host), ' x ');
    expect(button(host, 'Save').disabled).toBe(false);
  });

  it('Load is enabled once a preset is selected', async () => {
    const { host } = await mount({ getSnapshot: withPresets(['Lead A', 'Pad']) });
    expect(button(host, 'Load').disabled).toBe(true);
    await change(list(host), 'Pad');
    expect(button(host, 'Load').disabled).toBe(false);
  });
});

describe('PresetPanel: save flow', () => {
  it('a new name sends the trimmed name with overwrite:false, no confirm, and reports Saved', async () => {
    const confirm = vi.fn(async () => true);
    const { host, dispatch, store } = await mount({ confirm });
    await save(host, '  Lead  ');
    expect(dispatch).toHaveBeenCalledTimes(1);
    expect(dispatch).toHaveBeenLastCalledWith({ name: 'savePreset', args: { name: 'Lead', overwrite: false } });
    expect(confirm).not.toHaveBeenCalled();
    expect(status(store)).toEqual({ text: 'Saved "Lead".', error: false });
    expect(names(host)).toEqual(['Lead']); // the list refreshes from the response snapshot
  });

  it('exists asks for confirmation and, if confirmed, sends overwrite:true', async () => {
    const confirm = vi.fn(async () => true);
    const { host, dispatch, store } = await mount({ confirm });
    await save(host, 'Lead');
    await save(host, 'Lead');
    expect(confirm).toHaveBeenCalledTimes(1);
    expect(String(confirm.mock.calls[0].join(' '))).toContain('Lead');
    expect(dispatch.mock.calls.map((c) => c[0])).toEqual([
      { name: 'savePreset', args: { name: 'Lead', overwrite: false } },
      { name: 'savePreset', args: { name: 'Lead', overwrite: false } },
      { name: 'savePreset', args: { name: 'Lead', overwrite: true } },
    ]);
    expect(status(store)).toEqual({ text: 'Saved "Lead".', error: false });
  });

  it('a declined overwrite sends no second dispatch and leaves no error', async () => {
    const confirm = vi.fn(async () => false);
    const { host, dispatch, store } = await mount({ confirm });
    await save(host, 'Lead');
    dispatch.mockClear();
    await save(host, 'Lead');
    expect(confirm).toHaveBeenCalledTimes(1);
    expect(dispatch).toHaveBeenCalledTimes(1); // only the overwrite:false probe
    expect(status(store)?.error ?? false).toBe(false);
  });

  it('a rejected confirm shows an error and sends no overwrite', async () => {
    const confirm = vi.fn(async () => {
      throw new Error('dialog failed');
    });
    const { host, dispatch, store } = await mount({ confirm });
    await save(host, 'Lead');
    dispatch.mockClear();
    await save(host, 'Lead');
    expect(dispatch).toHaveBeenCalledTimes(1);
    expect(status(store)).toEqual({ text: 'Error: dialog failed', error: true });
  });

  it('other failures keep their mapped message and do not ask to overwrite', async () => {
    const confirm = vi.fn(async () => true);
    const { host, store } = await mount({ confirm, dispatch: async () => ({ ok: false, error: 'nameInvalid' }) });
    await save(host, 'a/b');
    expect(confirm).not.toHaveBeenCalled();
    expect(status(store)).toEqual({ text: 'Preset name is invalid.', error: true });
  });
});

describe('PresetPanel: load', () => {
  it('Load sends loadPreset, fills the name field and reports Loaded', async () => {
    const { host, dispatch, store } = await mount();
    await save(host, 'Lead A');
    await change(nameField(host), 'something else');
    await change(list(host), 'Lead A');
    await click(button(host, 'Load'));
    await settle();
    expect(dispatch).toHaveBeenLastCalledWith({ name: 'loadPreset', args: { name: 'Lead A' } });
    expect(nameField(host).value).toBe('Lead A');
    expect(status(store)).toEqual({ text: 'Loaded "Lead A".', error: false });
  });

  it('Load while busy shows the busy message and leaves the name field unchanged', async () => {
    const { host, store } = await mount({ getSnapshot: withPresets(['Lead A']) }, { busy: true });
    await change(nameField(host), 'Draft');
    await change(list(host), 'Lead A');
    await click(button(host, 'Load'));
    await settle();
    expect(status(store)).toEqual({ text: 'Busy, try again', error: true });
    expect(nameField(host).value).toBe('Draft');
  });

  it('a missing file leaves the name field unchanged and maps the token', async () => {
    const { host, store } = await mount({ getSnapshot: withPresets(['Ghost']) }); // listed, but the engine has no such file
    await change(list(host), 'Ghost');
    await click(button(host, 'Load'));
    await settle();
    expect(status(store)).toEqual({ text: 'Preset not found.', error: true });
    expect(nameField(host).value).toBe('');
  });
});

describe('PresetPanel: list', () => {
  it('shows the snapshot preset names and refreshes on a snapshot event', async () => {
    let emit: (s: Snapshot) => void = () => {};
    const initial = await withPresets(['Lead A', 'Pad'])();
    const { host } = await mount({ getSnapshot: async () => initial, onSnapshot: (l) => ((emit = l), () => {}) });
    expect(names(host)).toEqual(['Lead A', 'Pad']);
    await act(async () => emit({ ...initial, presetNames: ['Pad', 'Zed'] }));
    expect(names(host)).toEqual(['Pad', 'Zed']);
  });

  it('drops a selection that disappeared from the list (Load is disabled again)', async () => {
    let emit: (s: Snapshot) => void = () => {};
    const initial = await withPresets(['Lead A', 'Pad'])();
    const { host } = await mount({ getSnapshot: async () => initial, onSnapshot: (l) => ((emit = l), () => {}) });
    await change(list(host), 'Pad');
    expect(button(host, 'Load').disabled).toBe(false);
    await act(async () => emit({ ...initial, presetNames: ['Lead A'] }));
    expect(button(host, 'Load').disabled).toBe(true);
  });
});
