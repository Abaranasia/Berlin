import { describe, it, expect } from 'vitest';
import {
  COMMAND_NAMES, WAVEFORMS, LFO_DESTINATIONS, DELAY_DIVISIONS, RHYTHM_MODES, SCALE_TYPES,
  type Command, type DispatchResult, type Snapshot,
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
  it('are the 15 bridge commands', () => {
    expect(COMMAND_NAMES).toHaveLength(15);
    expect(new Set(COMMAND_NAMES).size).toBe(15);
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
    // @ts-expect-error not one of the 15 commands
    const cmd: Command = { name: 'doesNotExist', args: {} };
    expect(cmd.name).toBe('doesNotExist');
  });
});
