import { describe, it, expect, vi } from 'vitest';
import { createMockBridge } from './mock';
import { LIMITS } from './limits';
import type { Command, DispatchResult, Snapshot } from './protocol';

const run = (bridge: ReturnType<typeof createMockBridge>, command: unknown) => bridge.dispatch(command as Command);
const snapshotOf = (r: DispatchResult): Snapshot => {
  if (!r.ok) throw new Error(`dispatch failed: ${r.error}`);
  return r.snapshot;
};

describe('mock bridge envelopes', () => {
  it('a successful command returns {ok, error:"", snapshot} reflecting the change', async () => {
    const mock = createMockBridge();
    const result = await run(mock, { name: 'setBpm', args: { bpm: 140 } });
    expect(result).toMatchObject({ ok: true, error: '' });
    expect(snapshotOf(result).bpm).toBe(140);
    expect((await mock.getSnapshot())?.bpm).toBe(140);
  });

  it('setPatch merges only the supplied fields', async () => {
    const mock = createMockBridge();
    const before = (await mock.getSnapshot())!.patch;
    const patch = snapshotOf(await run(mock, { name: 'setPatch', args: { cutoffHz: 1200 } })).patch;
    expect(patch.cutoffHz).toBe(1200);
    expect(patch.resonance).toBe(before.resonance);
  });

  it('emits a snapshot event after a successful command', async () => {
    const mock = createMockBridge();
    const seen: number[] = [];
    const off = mock.onSnapshot((s) => seen.push(s.bpm));
    await run(mock, { name: 'setBpm', args: { bpm: 90 } });
    off();
    await run(mock, { name: 'setBpm', args: { bpm: 91 } });
    expect(seen).toEqual([90]);
  });
});

describe('mock bridge clamps to limits.ts', () => {
  it.each([
    [{ name: 'setBpm', args: { bpm: 9999 } }, (s: Snapshot) => s.bpm, LIMITS.bpm.max],
    [{ name: 'setBpm', args: { bpm: -5 } }, (s: Snapshot) => s.bpm, LIMITS.bpm.min],
    [{ name: 'setPatch', args: { cutoffHz: 1e9 } }, (s: Snapshot) => s.patch.cutoffHz, LIMITS.cutoffHz.max],
    [{ name: 'setPatch', args: { attack: 0 } }, (s: Snapshot) => s.patch.attack, LIMITS.attackSeconds.min],
    [{ name: 'setMasterLevel', args: { level: 2.5 } }, (s: Snapshot) => s.masterLevel, LIMITS.outputLevel.max],
  ])('%j', async (command, read, expected) => {
    expect(read(snapshotOf(await run(createMockBridge(), command)))).toBe(expected);
  });
});

describe('mock bridge reproduces error tokens', () => {
  it.each([
    [{ name: 'setBpm', args: {} }, 'missing arg: bpm'],
    [{ name: 'setBpm', args: { bpm: 'fast' } }, 'invalid type: bpm'],
    [{ name: 'setMasterLevel', args: { level: Infinity } }, 'non-finite: level'],
    [{ name: 'setPatch', args: { oscilatorWaveform: 'square' } }, 'unknown field: oscilatorWaveform'],
    [{ name: 'setPatch', args: { waveform: 'sawtooth' } }, 'invalid enum: waveform'],
    [{ name: 'doesNotExist', args: {} }, 'unknown command: doesNotExist'],
    [{ name: 'exportMidi', args: { path: 'output.mid' } }, 'path not absolute'],
    [{ name: 'loadPreset', args: { name: 'Ghost' } }, 'fileNotFound'],
  ])('%j -> %s', async (command, error) => {
    expect(await run(createMockBridge(), command)).toEqual({ ok: false, error });
  });

  it('exists without overwrite, then ok with overwrite', async () => {
    const mock = createMockBridge();
    expect((await run(mock, { name: 'savePreset', args: { name: 'A', overwrite: false } })).ok).toBe(true);
    expect(await run(mock, { name: 'savePreset', args: { name: 'A', overwrite: false } })).toEqual({ ok: false, error: 'exists' });
    expect((await run(mock, { name: 'savePreset', args: { name: 'A', overwrite: true } })).ok).toBe(true);
    expect(snapshotOf(await run(mock, { name: 'loadPreset', args: { name: 'A' } })).presetNames).toEqual(['A']);
  });

  it('busy rejects regenerate, mutate and loadPreset', async () => {
    const mock = createMockBridge({ busy: true });
    for (const command of [{ name: 'regenerate', args: { randomize: true } }, { name: 'mutate', args: {} }, { name: 'loadPreset', args: { name: 'A' } }]) {
      expect(await run(mock, command)).toEqual({ ok: false, error: 'busy' });
    }
  });
});

describe('mock bridge dialogs', () => {
  it('chooseExportFile yields an absolute path and confirm defers to window.confirm', async () => {
    const mock = createMockBridge();
    expect(await mock.chooseExportFile()).toEqual({ cancelled: false, path: '/mock/berlin-export.mid' });
    vi.stubGlobal('confirm', () => false);
    expect(await mock.confirm('t', 'm')).toBe(false);
    vi.unstubAllGlobals();
  });
});
