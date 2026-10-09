// @vitest-environment jsdom
import { describe, it, expect, vi, afterEach } from 'vitest';
import { ExportButton } from './ExportButton';
import { StatusLine } from './StatusLine';
import { button, click, makeHarness, render, settle } from '../testing/harness';
import type { ChooseExportResult, DispatchResult } from '../bridge/protocol';

let unmount = () => {};
afterEach(() => unmount());
const mount = async (chosen: () => Promise<ChooseExportResult>, dispatch?: () => Promise<DispatchResult>) => {
  const chooseExportFile = vi.fn(chosen);
  const harness = makeHarness({ chooseExportFile, ...(dispatch && { dispatch }) });
  const mounted = await render(
    <>
      <ExportButton />
      <StatusLine />
    </>,
    harness,
  );
  unmount = mounted.unmount;
  await click(button(mounted.host, 'Export MIDI'));
  await settle();
  return { ...mounted, chooseExportFile, status: mounted.host.querySelector('[role="status"], [role="alert"]')! };
};

describe('ExportButton', () => {
  it('Export chosen path: asks the chooser, then dispatches exportMidi with that path and reports the file name', async () => {
    const { chooseExportFile, dispatch, status } = await mount(async () => ({ cancelled: false, path: 'C:/music/a.mid' }));
    expect(chooseExportFile).toHaveBeenCalledTimes(1);
    expect(dispatch).toHaveBeenCalledTimes(1);
    expect(dispatch).toHaveBeenLastCalledWith({ name: 'exportMidi', args: { path: 'C:/music/a.mid' } });
    expect(status.textContent).toBe('Exported to a.mid');
  });

  it('Cancel is a silent no-op: nothing dispatched, no status', async () => {
    const { dispatch, status, store } = await mount(async () => ({ cancelled: true, path: '' }));
    expect(dispatch).not.toHaveBeenCalled();
    expect(status.textContent).toBe('');
    expect(store.getState().status).toBeNull();
  });

  it('Chooser busy: nothing dispatched and the status shows "Busy, try again" as an error', async () => {
    const { dispatch, status } = await mount(async () => ({ cancelled: true, path: '', error: 'busy' }));
    expect(dispatch).not.toHaveBeenCalled();
    expect(status.textContent).toBe('Busy, try again');
    expect(status.getAttribute('role')).toBe('alert');
  });

  it.each([
    ['pathUnavailable', 'Export failed: destination folder unavailable.'],
    ['writeFailed', 'Export failed: could not write the file.'],
    ['invalidTimeline', 'Export failed: the timeline was invalid.'],
  ])('Write failure %s shows the mapped message', async (token, message) => {
    const { dispatch, status } = await mount(
      async () => ({ cancelled: false, path: 'C:/a.mid' }),
      async () => ({ ok: false, error: token }),
    );
    expect(dispatch).toHaveBeenCalledTimes(1);
    expect(status.textContent).toBe(message);
    expect(status.getAttribute('role')).toBe('alert');
  });

  it('a rejected chooser shows an error status and dispatches nothing', async () => {
    const { dispatch, status } = await mount(async () => {
      throw new Error('dialog failed');
    });
    expect(dispatch).not.toHaveBeenCalled();
    expect(status.textContent).toBe('Error: dialog failed');
  });

  it('a malformed chooser result is reported, not thrown', async () => {
    const { dispatch, status } = await mount(async () => null as unknown as ChooseExportResult);
    expect(dispatch).not.toHaveBeenCalled();
    expect(status.getAttribute('role')).toBe('alert');
    expect(status.textContent).toMatch(/^Error: /);
  });
});
