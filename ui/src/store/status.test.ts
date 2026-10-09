import { describe as suite, it, expect } from 'vitest';
import { describe, describeThrown, describeSuccess } from './status';

const PRESET_CMDS = ['savePreset', 'loadPreset'];

suite('describe(command, token): every row of the design Status Messages table', () => {
  it.each([
    ['regenerate', 'busy', 'Busy, try again'],
    ['loadPreset', 'busy', 'Busy, try again'],
    ['savePreset', 'exists', 'A preset with that name already exists.'],
    ['exportMidi', 'invalidTimeline', 'Export failed: the timeline was invalid.'],
    ['exportMidi', 'pathUnavailable', 'Export failed: destination folder unavailable.'],
    ['exportMidi', 'writeFailed', 'Export failed: could not write the file.'],
    ['exportMidi', 'path not absolute', 'Export failed: the path is not absolute.'],
    ['setBpm', 'missing arg: bpm', 'missing arg: bpm'],
    ['setPatch', 'weird', 'weird'],
  ])('%s + %s', (command, token, text) => {
    expect(describe(command, token)).toEqual({ text, error: true });
  });

  it.each(PRESET_CMDS)('%s preset tokens', (command) => {
    const text = (token: string) => describe(command, token).text;
    expect(text('nameInvalid')).toBe('Preset name is invalid.');
    expect(text('directoryUnavailable')).toBe('Preset folder unavailable.');
    expect(text('writeFailed')).toBe('Could not write the preset file.');
    expect(text('fileNotFound')).toBe('Preset not found.');
    expect(text('parseFailed')).toBe('Preset file is invalid or corrupted.');
    expect(text('unsupportedVersion')).toBe('Preset was saved by a newer version of Berlin.');
  });

  it('writeFailed depends on the command', () => {
    expect(describe('exportMidi', 'writeFailed').text).not.toBe(describe('savePreset', 'writeFailed').text);
  });
});

suite('describeThrown', () => {
  it('wraps an Error message', () => expect(describeThrown(new Error('boom'))).toEqual({ text: 'Error: boom', error: true }));
  it('wraps a non-Error rejection', () => expect(describeThrown('nope')).toEqual({ text: 'Error: nope', error: true }));
});

suite('describeSuccess', () => {
  it.each([
    [{ name: 'regenerate', args: { randomize: false } }, 'Generated.'],
    [{ name: 'regenerate', args: { randomize: true } }, 'Randomized.'],
    [{ name: 'mutate', args: {} }, 'Mutated.'],
    [{ name: 'savePreset', args: { name: 'Lead A', overwrite: true } }, 'Saved "Lead A".'],
    [{ name: 'loadPreset', args: { name: 'Lead A' } }, 'Loaded "Lead A".'],
    [{ name: 'exportMidi', args: { path: 'C:\\Users\\me\\berlin-export.mid' } }, 'Exported to berlin-export.mid'],
    [{ name: 'exportMidi', args: { path: '/home/me/out.mid' } }, 'Exported to out.mid'],
  ] as const)('%j', (command, text) => {
    expect(describeSuccess(command)).toEqual({ text, error: false });
  });

  it('is empty for commands with no success message', () => {
    expect(describeSuccess({ name: 'setBpm', args: { bpm: 121 } })).toBeNull();
  });
});
