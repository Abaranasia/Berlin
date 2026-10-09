// Status line wording (design "Status Messages"). Preset/export text is copied from the legacy editor.
import type { Command } from '../bridge/protocol';

export interface Status {
  text: string;
  error: boolean;
}

const BUSY = 'Busy, try again';
const PRESET: Record<string, string> = {
  nameInvalid: 'Preset name is invalid.',
  directoryUnavailable: 'Preset folder unavailable.',
  writeFailed: 'Could not write the preset file.',
  fileNotFound: 'Preset not found.',
  parseFailed: 'Preset file is invalid or corrupted.',
  unsupportedVersion: 'Preset was saved by a newer version of Berlin.',
};
const EXPORT: Record<string, string> = {
  invalidTimeline: 'Export failed: the timeline was invalid.',
  pathUnavailable: 'Export failed: destination folder unavailable.',
  writeFailed: 'Export failed: could not write the file.',
  'path not absolute': 'Export failed: the path is not absolute.',
};

const failure = (text: string): Status => ({ text, error: true });

// `writeFailed` is both a PresetResult and a MidiFileWriteResult name, hence the command parameter.
export function describe(command: string, token: string): Status {
  if (token === 'busy') return failure(BUSY);
  if (command === 'savePreset' && token === 'exists') return failure('A preset with that name already exists.');
  const table = command === 'exportMidi' ? EXPORT : command === 'savePreset' || command === 'loadPreset' ? PRESET : {};
  return failure(table[token] ?? token);
}

export const describeThrown = (e: unknown): Status => failure(`Error: ${e instanceof Error ? e.message : String(e)}`);

export function describeSuccess(command: Command): Status | null {
  const ok = (text: string): Status => ({ text, error: false });
  switch (command.name) {
    case 'regenerate': return ok(command.args.randomize ? 'Randomized.' : 'Generated.');
    case 'mutate': return ok('Mutated.');
    case 'savePreset': return ok(`Saved "${command.args.name}".`);
    case 'loadPreset': return ok(`Loaded "${command.args.name}".`);
    case 'exportMidi': return ok(`Exported to ${command.args.path.split(/[\\/]/).pop()}`);
    default: return null;
  }
}
