// Dev-only stand-in for the host: a minimal engine with the same envelopes and error tokens as UiBridge.
// Only reachable through the import.meta.env.DEV-guarded dynamic import in main.tsx.
import type { Bridge } from './bridge';
import { AUTO_EVOLVE_RATE, GENERATION_LIMITS, LIMITS, type Range } from './limits';
import {
  DELAY_DIVISIONS, LFO_DESTINATIONS, RHYTHM_MODES, SCALE_TYPES, WAVEFORMS,
  type Command, type DispatchResult, type PlayheadEvent, type Snapshot,
} from './protocol';

const initial = (): Snapshot => ({
  patch: {
    waveform: 'saw', cutoffHz: 1000, resonance: 1, pulseWidth: 0.5, attack: 0.01, decay: 0.2, sustain: 0.7, release: 0.3,
    lfoRateHz: 2, lfoDepth: 0, lfoDestination: 'cutoff', delayTimeSeconds: 0.5, delayFeedback: 0.3, delayMix: 0.2,
    reverbRoomSize: 0.4, reverbDamping: 0.5, reverbWetLevel: 0.2, reverbDryLevel: 0.8, outputLevel: 0.8,
    delaySynced: false, delayDivision: 'quarter',
  },
  generationParams: {
    mode: 'random', pulses: 5, rotation: 0, stepProbability: 0.5, lockSeed: false,
    scaleType: 'minor', rootPitchClass: 0, rangeLow: 36, rangeHigh: 72,
  },
  seed: '42', bpm: 120, playing: false, playheadStep: 0, loopCount: 0, synthEnabled: true, effectsEnabled: true,
  masterLevel: 0.8, autoEvolveEnabled: false, autoEvolveRate: 4, mutationCount: 0,
  steps: Array.from({ length: 16 }, (_, i) => ({ note: 48 + i, active: i % 2 === 0 })),
  presetNames: [],
});

const PATCH_RANGES: Record<string, Range> = {
  ...LIMITS, attack: LIMITS.attackSeconds, decay: LIMITS.decaySeconds, release: LIMITS.releaseSeconds,
};
const ENUMS: Record<string, readonly string[]> = {
  waveform: WAVEFORMS, lfoDestination: LFO_DESTINATIONS, delayDivision: DELAY_DIVISIONS, mode: RHYTHM_MODES, scaleType: SCALE_TYPES,
};
const BOOLEANS = new Set(['delaySynced', 'lockSeed']);
const INT_FIELDS = new Set(['pulses', 'rotation', 'rootPitchClass', 'rangeLow', 'rangeHigh']);
// Source/core/TempoSync.h factorFor: delay seconds = (60 / bpm) * beats per division.
const DIVISION_BEATS: Record<string, number> = {
  half: 2, quarter: 1, dottedEighth: 0.75, eighth: 0.5, eighthTriplet: 1 / 3, sixteenth: 0.25,
};
const MIN_PITCH_RANGE_SPAN = 12;
const [MIN_PITCH, MAX_PITCH] = [GENERATION_LIMITS.rangeLow.min, GENERATION_LIMITS.rangeLow.max];
// std::clamp as in UiBridge: below lo gives lo, otherwise above hi gives hi, otherwise v (so lo wins when lo > hi).
const stdClamp = (v: number, lo: number, hi: number): number => (v < lo ? lo : hi < v ? hi : v);
// std::lround: half away from zero.
const lround = (v: number): number => Math.sign(v) * Math.round(Math.abs(v));

// Mirrors normalizePitchRange (Source/generation/SequenceBuilder.cpp): order, clamp, widen up then down.
function normalizePitchRange(low: number, high: number): [number, number] {
  [low, high] = low > high ? [high, low] : [low, high];
  low = stdClamp(low, MIN_PITCH, MAX_PITCH);
  high = stdClamp(high, MIN_PITCH, MAX_PITCH);
  [low, high] = low > high ? [high, low] : [low, high];
  while (high - low < MIN_PITCH_RANGE_SPAN) {
    if (high < MAX_PITCH) high += 1;
    else low -= 1;
  }
  return [low, high];
}

// Throws the error token, mirroring UiBridge's argument validation.
class Rejected extends Error {}
const reject = (token: string): never => {
  throw new Rejected(token);
};

function number(args: Record<string, unknown>, name: string, range: Range): number {
  const value = args[name];
  if (value === undefined) return reject(`missing arg: ${name}`);
  if (typeof value !== 'number') return reject(`invalid type: ${name}`);
  if (!Number.isFinite(value)) return reject(`non-finite: ${name}`);
  return Math.min(range.max, Math.max(range.min, value));
}

const text = (args: Record<string, unknown>, name: string): string =>
  typeof args[name] === 'string' ? (args[name] as string) : reject(args[name] === undefined ? `missing arg: ${name}` : `invalid type: ${name}`);

const flag = (args: Record<string, unknown>, name: string): boolean =>
  typeof args[name] === 'boolean' ? (args[name] as boolean) : reject(args[name] === undefined ? `missing arg: ${name}` : `invalid type: ${name}`);

// Merges the supplied fields of a partial patch / generation-params object into `target`.
function merge(target: object, args: Record<string, unknown>, ranges: Record<string, Range>): void {
  const known = new Set(Object.keys(target));
  const next: Record<string, unknown> = {};
  for (const name of Object.keys(args)) {
    if (!known.has(name)) reject(`unknown field: ${name}`);
    if (ENUMS[name]) {
      if (typeof args[name] !== 'string') reject(`invalid type: ${name}`);
      if (!ENUMS[name].includes(args[name] as string)) reject(`invalid enum: ${name}`);
      next[name] = args[name];
    } else if (BOOLEANS.has(name)) {
      next[name] = flag(args, name);
    } else {
      const value = number(args, name, ranges[name] ?? { min: -Infinity, max: Infinity });
      next[name] = INT_FIELDS.has(name) ? lround(value) : value;
    }
  }
  Object.assign(target, next);
}

export function createMockBridge({ busy = false }: { busy?: boolean } = {}): Bridge {
  const state = initial();
  const snapshotListeners = new Set<(s: Snapshot) => void>();
  const playheadListeners = new Set<(e: PlayheadEvent) => void>();
  const presets = new Set<string>();

  // Host D7: a synced delay time is derived from bpm and division, then clamped (BerlinAudioProcessor::applySyncedDelayTime).
  const applySyncedDelayTime = (): void => {
    if (!state.patch.delaySynced) return;
    const seconds = (60 / state.bpm) * DIVISION_BEATS[state.patch.delayDivision];
    state.patch.delayTimeSeconds = Math.min(LIMITS.delayTimeSeconds.max, Math.max(LIMITS.delayTimeSeconds.min, seconds));
  };

  // Host D12: both range fields -> normalizePitchRange; a lone one is span-clamped against the stored other.
  const applyRangeConstraint = (args: Record<string, unknown>): void => {
    const params = state.generationParams;
    const hasLow = 'rangeLow' in args;
    const hasHigh = 'rangeHigh' in args;
    if (hasLow && hasHigh) [params.rangeLow, params.rangeHigh] = normalizePitchRange(params.rangeLow, params.rangeHigh);
    else if (hasLow) params.rangeLow = stdClamp(params.rangeLow, MIN_PITCH, params.rangeHigh - MIN_PITCH_RANGE_SPAN);
    else if (hasHigh) params.rangeHigh = stdClamp(params.rangeHigh, params.rangeLow + MIN_PITCH_RANGE_SPAN, MAX_PITCH);
  };

  const apply = (command: Command): void => {
    const args = (command.args ?? {}) as Record<string, unknown>;
    switch (command.name) {
      case 'setPlaying': state.playing = flag(args, 'playing'); break;
      case 'setBpm': state.bpm = number(args, 'bpm', LIMITS.bpm); applySyncedDelayTime(); break;
      case 'setSeed': state.seed = /^-?\d+$/.test(text(args, 'seed')) ? (args.seed as string) : reject('invalid type: seed'); break;
      case 'setPatch': merge(state.patch, args, PATCH_RANGES); applySyncedDelayTime(); break;
      case 'setGenerationParams': merge(state.generationParams, args, GENERATION_LIMITS); applyRangeConstraint(args); break;
      case 'regenerate':
        if (busy) reject('busy');
        if (flag(args, 'randomize') && !state.generationParams.lockSeed) state.seed = String(Math.floor(Math.random() * 1e9));
        break;
      case 'mutate':
        if (busy) reject('busy');
        state.mutationCount += 1;
        break;
      case 'setSynthEnabled': state.synthEnabled = flag(args, 'enabled'); break;
      case 'setEffectsEnabled': state.effectsEnabled = flag(args, 'enabled'); break;
      case 'setMasterLevel': state.masterLevel = number(args, 'level', LIMITS.outputLevel); break;
      case 'setAutoEvolveEnabled': state.autoEvolveEnabled = flag(args, 'enabled'); break;
      case 'setAutoEvolveRate': state.autoEvolveRate = Math.round(number(args, 'rate', AUTO_EVOLVE_RATE)); break;
      case 'savePreset': {
        const name = text(args, 'name');
        if (presets.has(name) && !flag(args, 'overwrite')) reject('exists');
        presets.add(name);
        state.presetNames = [...presets].sort();
        break;
      }
      case 'loadPreset':
        if (busy) reject('busy');
        if (!presets.has(text(args, 'name'))) reject('fileNotFound');
        break;
      case 'exportMidi':
        if (!/^([A-Za-z]:[\\/]|\/)/.test(text(args, 'path'))) reject('path not absolute');
        break;
      default: reject(`unknown command: ${(command as { name: string }).name}`);
    }
  };

  return {
    async dispatch(command) {
      try {
        apply(command);
      } catch (e) {
        if (e instanceof Rejected) return { ok: false, error: e.message } satisfies DispatchResult;
        throw e;
      }
      const snapshot = structuredClone(state);
      snapshotListeners.forEach((l) => l(snapshot));
      return { ok: true, error: '', snapshot };
    },
    getSnapshot: async () => structuredClone(state),
    onPlayhead(listener) {
      playheadListeners.add(listener);
      return () => playheadListeners.delete(listener);
    },
    onSnapshot(listener) {
      snapshotListeners.add(listener);
      return () => snapshotListeners.delete(listener);
    },
    chooseExportFile: async () => ({ cancelled: false, path: '/mock/berlin-export.mid' }),
    confirm: async (title, message) => (typeof confirm === 'function' ? confirm(`${title}\n${message}`) : true),
  };
}

export const mockBridge = createMockBridge();
