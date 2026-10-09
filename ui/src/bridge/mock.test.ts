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

describe('mock bridge synced delay (host D7)', () => {
  const delayAfter = async (mock: ReturnType<typeof createMockBridge>, command: unknown) =>
    snapshotOf(await run(mock, command)).patch.delayTimeSeconds;

  it('setPatch derives delayTimeSeconds from bpm and division when synced', async () => {
    const mock = createMockBridge();
    expect(await delayAfter(mock, { name: 'setPatch', args: { delaySynced: true, delayDivision: 'eighth' } })).toBeCloseTo(0.25, 6);
    expect(await delayAfter(mock, { name: 'setPatch', args: { delayDivision: 'dottedEighth' } })).toBeCloseTo(0.375, 6);
  });

  it('a synced setPatch overrides a supplied delayTimeSeconds', async () => {
    const mock = createMockBridge();
    const seconds = await delayAfter(mock, { name: 'setPatch', args: { delaySynced: true, delayDivision: 'half', delayTimeSeconds: 0.1 } });
    expect(seconds).toBeCloseTo(1, 6);
  });

  it('setBpm recomputes the delay while synced', async () => {
    const mock = createMockBridge();
    await run(mock, { name: 'setPatch', args: { delaySynced: true, delayDivision: 'quarter' } });
    expect(snapshotOf(await run(mock, { name: 'setBpm', args: { bpm: 60 } })).patch.delayTimeSeconds).toBeCloseTo(1, 6);
  });

  it('free mode keeps the supplied delayTimeSeconds and setBpm leaves it alone', async () => {
    const mock = createMockBridge();
    expect(await delayAfter(mock, { name: 'setPatch', args: { delayTimeSeconds: 0.7 } })).toBeCloseTo(0.7, 6);
    expect(snapshotOf(await run(mock, { name: 'setBpm', args: { bpm: 60 } })).patch.delayTimeSeconds).toBeCloseTo(0.7, 6);
  });
});

describe('mock bridge setGenerationParams mirrors the host', () => {
  const params = async (args: Record<string, unknown>) =>
    snapshotOf(await run(createMockBridge(), { name: 'setGenerationParams', args })).generationParams;

  it('rounds int fields after the clamp, half away from zero', async () => {
    expect(await params({ pulses: 4.5, rotation: 2.4, rootPitchClass: 7.6 })).toMatchObject({ pulses: 5, rotation: 2, rootPitchClass: 8 });
    expect(await params({ pulses: 99.4 })).toMatchObject({ pulses: 16 });
  });

  it('rounds both range fields', async () => {
    expect(await params({ rangeLow: 40.4, rangeHigh: 80.5 })).toMatchObject({ rangeLow: 40, rangeHigh: 81 });
  });

  it('normalizes when both range fields are supplied (swap, then widen upward)', async () => {
    expect(await params({ rangeLow: 80, rangeHigh: 60 })).toMatchObject({ rangeLow: 60, rangeHigh: 80 });
    expect(await params({ rangeLow: 50, rangeHigh: 53 })).toMatchObject({ rangeLow: 50, rangeHigh: 62 });
    expect(await params({ rangeLow: 120, rangeHigh: 127 })).toMatchObject({ rangeLow: 115, rangeHigh: 127 });
  });

  it('span-clamps a lone rangeLow against the stored rangeHigh', async () => {
    expect(await params({ rangeLow: 70 })).toMatchObject({ rangeLow: 60, rangeHigh: 72 });
    expect(await params({ rangeLow: 50 })).toMatchObject({ rangeLow: 50, rangeHigh: 72 });
  });

  it('span-clamps a lone rangeHigh against the stored rangeLow', async () => {
    expect(await params({ rangeHigh: 40 })).toMatchObject({ rangeLow: 36, rangeHigh: 48 });
    expect(await params({ rangeHigh: 100 })).toMatchObject({ rangeLow: 36, rangeHigh: 100 });
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

describe('mock bridge settings', () => {
  it('is absent by default and present with {standalone: true}', () => {
    expect(createMockBridge().settings).toBeUndefined();
    expect(createMockBridge({ standalone: true }).settings).toBeDefined();
  });

  it('lists None first and no volume field', async () => {
    const audio = await createMockBridge({ standalone: true }).settings!.get();
    expect(audio.midiPorts[0]).toEqual({ id: '', name: 'None' });
    expect(audio).not.toHaveProperty('volume');
  });

  it('apply succeeds by default and can be configured to fail', async () => {
    const request = { deviceType: 'a', device: 'b', sampleRate: 48000, bufferSize: 512, midiPortId: '', midiChannel: 1 };
    await expect(createMockBridge({ standalone: true }).settings!.apply(request)).resolves.toEqual({ ok: true });
    await expect(createMockBridge({ standalone: true, applyResult: { ok: false, error: 'busy' } }).settings!.apply(request)).resolves.toEqual({ ok: false, error: 'busy' });
  });
});

describe('mock bridge setStep mirrors the host', () => {
  const C_MINOR_36_72 = [36, 38, 39, 41, 43, 44, 46, 48, 50, 51, 53, 55, 56, 58, 60, 62, 63, 65, 67, 68, 70, 72];
  const setStep = (mock: ReturnType<typeof createMockBridge>, index: number, note: number, active = true) =>
    run(mock, { name: 'setStep', args: { index, note, active } });

  it('a valid edit updates the step and sets edited; the fixture scaleNotes is C minor 36..72', async () => {
    const mock = createMockBridge();
    const before = (await mock.getSnapshot())!;
    expect(before.edited).toBe(false);
    expect(before.scaleNotes).toEqual(C_MINOR_36_72);
    const after = snapshotOf(await setStep(mock, 2, 50, false));
    expect(after.steps[2]).toEqual({ note: 50, active: false });
    expect(after.steps[3]).toEqual(before.steps[3]);
    expect(after.edited).toBe(true);
  });

  it('validates the index first, then the note', async () => {
    const mock = createMockBridge();
    expect(await setStep(mock, 16, 50)).toEqual({ ok: false, error: 'step-out-of-range' });
    expect(await setStep(mock, -1, 50)).toEqual({ ok: false, error: 'step-out-of-range' });
    expect(await setStep(mock, 2, 49)).toEqual({ ok: false, error: 'note-out-of-scale' });
    expect(await setStep(mock, 16, 49)).toEqual({ ok: false, error: 'step-out-of-range' });
    expect((await mock.getSnapshot())!.edited).toBe(false);
  });

  it('rejects malformed args like the bridge', async () => {
    const mock = createMockBridge();
    expect(await run(mock, { name: 'setStep', args: { index: 1, note: 50 } })).toEqual({ ok: false, error: 'missing arg: active' });
    expect(await run(mock, { name: 'setStep', args: { index: 2.5, note: 50, active: true } })).toEqual({ ok: false, error: 'invalid type: index' });
  });

  it('busy rejects a valid edit but validation still precedes busy', async () => {
    const mock = createMockBridge({ busy: true });
    expect(await setStep(mock, 2, 50)).toEqual({ ok: false, error: 'busy' });
    expect(await setStep(mock, 99, 50)).toEqual({ ok: false, error: 'step-out-of-range' });
  });

  it('regenerate and loadPreset clear edited; mutate keeps it', async () => {
    const mock = createMockBridge();
    await run(mock, { name: 'savePreset', args: { name: 'A', overwrite: false } });
    await setStep(mock, 0, 50);
    expect(snapshotOf(await run(mock, { name: 'mutate', args: {} })).edited).toBe(true);
    expect(snapshotOf(await run(mock, { name: 'regenerate', args: { randomize: false } })).edited).toBe(false);
    await setStep(mock, 0, 50);
    expect(snapshotOf(await run(mock, { name: 'loadPreset', args: { name: 'A' } })).edited).toBe(false);
  });
});
