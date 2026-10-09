import { describe, it, expect, vi } from 'vitest';
import { SYNTH_PANEL_KEY, readSynthPanelOpen, writeSynthPanelOpen } from './panelState';

const storageWith = (over: Partial<Storage>): (() => Storage) => () => ({ getItem: () => null, setItem: () => {}, ...over }) as Storage;

describe('readSynthPanelOpen', () => {
  it('uses the documented key', () => {
    expect(SYNTH_PANEL_KEY).toBe('berlin.ui.synthPanelOpen');
  });

  it('reads "1" as open', () => {
    expect(readSynthPanelOpen(storageWith({ getItem: () => '1' }))).toBe(true);
  });

  it.each(['0', null, 'true', ''])('reads %j as closed', (value) => {
    expect(readSynthPanelOpen(storageWith({ getItem: () => value }))).toBe(false);
  });

  it('is closed when getItem throws', () => {
    expect(readSynthPanelOpen(storageWith({ getItem: () => { throw new Error('denied'); } }))).toBe(false);
  });

  it('is closed when reaching the storage throws', () => {
    expect(readSynthPanelOpen(() => { throw new Error('SecurityError'); })).toBe(false);
  });
});

describe('writeSynthPanelOpen', () => {
  it('stores "1" and "0" under the key', () => {
    const setItem = vi.fn();
    writeSynthPanelOpen(true, storageWith({ setItem }));
    writeSynthPanelOpen(false, storageWith({ setItem }));
    expect(setItem).toHaveBeenNthCalledWith(1, SYNTH_PANEL_KEY, '1');
    expect(setItem).toHaveBeenNthCalledWith(2, SYNTH_PANEL_KEY, '0');
  });

  it('swallows a throwing setItem', () => {
    expect(() => writeSynthPanelOpen(true, storageWith({ setItem: () => { throw new Error('quota'); } }))).not.toThrow();
  });

  it('swallows a throwing storage getter', () => {
    expect(() => writeSynthPanelOpen(true, () => { throw new Error('SecurityError'); })).not.toThrow();
  });
});
