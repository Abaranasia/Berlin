import { describe, it, expect } from 'vitest';
import {
  COMMAND_NAMES, WAVEFORMS, LFO_DESTINATIONS, DELAY_DIVISIONS, RHYTHM_MODES, SCALE_TYPES,
  type Command, type DispatchResult, type ErrorToken, type Snapshot,
} from './protocol';

// Literal lists copied from the ui-bridge "Encoding Conventions" requirement.
describe('enum lists match the bridge encoding', () => {
  it('waveform', () => expect([...WAVEFORMS]).toEqual(['saw', 'square', 'pulse', 'triangle']));
  it('lfoDestination', () => expect([...LFO_DESTINATIONS]).toEqual(['pitch', 'cutoff', 'amplitude', 'pulseWidth']));
  it('scaleType', () => expect([...SCALE_TYPES]).toEqual(['minor', 'major', 'dorian', 'phrygian', 'mixolydian', 'harmonicMinor']));
  it('delayDivision', () =>
    expect([...DELAY_DIVISIONS]).toEqual(['half', 'quarter', 'dottedEighth', 'eighth', 'eighthTriplet', 'sixteenth']));
  it('rhythm mode', () => expect([...RHYTHM_MODES]).toEqual(['random', 'euclidean', 'probability']));
});

describe('command names', () => {
  it('are the 16 bridge commands', () => {
    expect(COMMAND_NAMES).toHaveLength(16);
    expect(new Set(COMMAND_NAMES).size).toBe(16);
    expect(COMMAND_NAMES).toContain('setStep');
    expect(COMMAND_NAMES).toContain('exportMidi');
    expect(COMMAND_NAMES).toContain('setGenerationParams');
  });
});

describe('typed fixtures', () => {
  it('a Command, a failed and a successful DispatchResult typecheck', () => {
    const cmd: Command = { name: 'setPatch', args: { cutoffHz: 1200, waveform: 'saw' } };
    const snapshot = { bpm: 140, seed: '9223372036854770000' } as Snapshot;
    const ok: DispatchResult = { ok: true, error: '', snapshot };
    const bad: DispatchResult = { ok: false, error: 'busy' };
    expect(cmd.name).toBe('setPatch');
    expect(ok.ok && ok.snapshot.seed).toBe('9223372036854770000');
    expect(bad.ok).toBe(false);
  });

  it('rejects an unknown command name at compile time', () => {
    // @ts-expect-error not one of the 16 commands
    const cmd: Command = { name: 'doesNotExist', args: {} };
    expect(cmd.name).toBe('doesNotExist');
  });
});

describe('step editing protocol', () => {
  it('setStep args are typed {index, note, active}; omitting active fails to compile', () => {
    const cmd: Command = { name: 'setStep', args: { index: 2, note: 50, active: true } };
    // @ts-expect-error active is required
    const missing: Command = { name: 'setStep', args: { index: 2, note: 50 } };
    expect(cmd.args).toEqual({ index: 2, note: 50, active: true });
    expect(missing.name).toBe('setStep');
  });

  it('the snapshot type carries edited and scaleNotes', () => {
    const snapshot = { edited: true, scaleNotes: [48, 50, 51] } as Snapshot;
    const edited: boolean = snapshot.edited;
    const notes: number[] = snapshot.scaleNotes;
    expect(edited).toBe(true);
    expect(notes).toEqual([48, 50, 51]);
  });

  it('the step error tokens are accepted as DispatchResult errors', () => {
    const tokens: ErrorToken[] = ['step-out-of-range', 'note-out-of-scale'];
    const results: DispatchResult[] = tokens.map((error) => ({ ok: false, error }));
    expect(results.map((r) => r.ok === false && r.error)).toEqual(['step-out-of-range', 'note-out-of-scale']);
  });
});
