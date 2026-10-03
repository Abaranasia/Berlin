// Hand-written wire protocol for the 15 UiBridge commands (see the ui-bridge spec, "Encoding Conventions").
export const WAVEFORMS = ['saw', 'square', 'pulse', 'triangle'] as const;
export const LFO_DESTINATIONS = ['pitch', 'cutoff', 'amplitude', 'pulseWidth'] as const;
export const DELAY_DIVISIONS = ['half', 'quarter', 'dottedEighth', 'eighth', 'eighthTriplet', 'sixteenth'] as const;
export const RHYTHM_MODES = ['random', 'euclidean', 'probability'] as const;
export const SCALE_TYPES = ['minor', 'major', 'dorian', 'phrygian', 'mixolydian', 'harmonicMinor'] as const;

export type Waveform = (typeof WAVEFORMS)[number];
export type LfoDestination = (typeof LFO_DESTINATIONS)[number];
export type DelayDivision = (typeof DELAY_DIVISIONS)[number];
export type RhythmMode = (typeof RHYTHM_MODES)[number];
export type ScaleType = (typeof SCALE_TYPES)[number];

export interface Patch {
  waveform: Waveform;
  cutoffHz: number;
  resonance: number;
  pulseWidth: number;
  attack: number;
  decay: number;
  sustain: number;
  release: number;
  lfoRateHz: number;
  lfoDepth: number;
  lfoDestination: LfoDestination;
  delayTimeSeconds: number;
  delayFeedback: number;
  delayMix: number;
  reverbRoomSize: number;
  reverbDamping: number;
  reverbWetLevel: number;
  reverbDryLevel: number;
  outputLevel: number;
  delaySynced: boolean;
  delayDivision: DelayDivision;
}

export interface GenerationParams {
  mode: RhythmMode;
  pulses: number;
  rotation: number;
  stepProbability: number;
  lockSeed: boolean;
  scaleType: ScaleType;
  rootPitchClass: number;
  rangeLow: number;
  rangeHigh: number;
}

export interface Snapshot {
  patch: Patch;
  generationParams: GenerationParams;
  seed: string; // decimal string: int64 exceeds the JS safe-integer range
  bpm: number;
  playing: boolean;
  playheadStep: number;
  loopCount: number;
  synthEnabled: boolean;
  effectsEnabled: boolean;
  masterLevel: number;
  autoEvolveEnabled: boolean;
  autoEvolveRate: number;
  mutationCount: number;
  steps: { note: number; active: boolean }[];
  presetNames: string[];
}

export type Command =
  | { name: 'setPlaying'; args: { playing: boolean } }
  | { name: 'setBpm'; args: { bpm: number } }
  | { name: 'setSeed'; args: { seed: string } }
  | { name: 'setPatch'; args: Partial<Patch> }
  | { name: 'setGenerationParams'; args: Partial<GenerationParams> }
  | { name: 'regenerate'; args: { randomize: boolean } }
  | { name: 'mutate'; args: Record<string, never> }
  | { name: 'setSynthEnabled'; args: { enabled: boolean } }
  | { name: 'setEffectsEnabled'; args: { enabled: boolean } }
  | { name: 'setMasterLevel'; args: { level: number } }
  | { name: 'setAutoEvolveEnabled'; args: { enabled: boolean } }
  | { name: 'setAutoEvolveRate'; args: { rate: number } }
  | { name: 'savePreset'; args: { name: string; overwrite: boolean } }
  | { name: 'loadPreset'; args: { name: string } }
  | { name: 'exportMidi'; args: { path: string } };

export type CommandName = Command['name'];

export const COMMAND_NAMES: readonly CommandName[] = [
  'setPlaying', 'setBpm', 'setSeed', 'setPatch', 'setGenerationParams', 'regenerate', 'mutate',
  'setSynthEnabled', 'setEffectsEnabled', 'setMasterLevel', 'setAutoEvolveEnabled', 'setAutoEvolveRate',
  'savePreset', 'loadPreset', 'exportMidi',
];

// Fixed tokens; the parameterised ones (`missing arg: x`, ...) and anything unknown fall under `string`.
export type ErrorToken =
  | 'busy' | 'exists' | 'path not absolute'
  | 'nameInvalid' | 'directoryUnavailable' | 'writeFailed' | 'fileNotFound' | 'parseFailed' | 'unsupportedVersion'
  | 'invalidTimeline' | 'pathUnavailable'
  | (string & {});

export type DispatchResult = { ok: true; error: ''; snapshot: Snapshot } | { ok: false; error: ErrorToken };

export interface PlayheadEvent {
  step: number;
  playing: boolean;
}

export interface ChooseExportResult {
  cancelled: boolean;
  path: string;
  error?: 'busy';
}
