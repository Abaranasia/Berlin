// Dependency-free external store (design D8): engine snapshot, optimistic overlay, playhead, status, draft.
import type { Bridge } from '../bridge/bridge';
import type { Command, DispatchResult, GenerationParams, Patch, PlayheadEvent, Snapshot } from '../bridge/protocol';
import { describe, describeSuccess, describeThrown, type Status } from './status';

// Store key convention. A key names one control (or one in-flight command lane): at most one request is in
// flight per key and the latest pending value wins. Its value type is the optimistic value the control shows.
//   - `patch.<field>`  every Patch field, sent as setPatch {<field>}
//   - `gen.<field>`    every GenerationParams field except the range, sent as setGenerationParams {<field>}
//   - `gen.range`      rangeLow and rangeHigh, ALWAYS sent together as one pair [low, high]
//   - bare command-level names for the single-value commands (bpm, playing, masterLevel, synthEnabled,
//     effectsEnabled, autoEvolveEnabled, autoEvolveRate, seed) and the flow commands (regenerate, mutate,
//     exportMidi, savePreset, loadPreset), whose optimistic value is the command's argument.
// `send` and `shown` are both typed by KeyValues, so a control cannot read a value of another type than it writes.
type PatchKeyValues = { [F in keyof Patch as `patch.${F}`]: Patch[F] };
type GenKeyValues = { [F in Exclude<keyof GenerationParams, 'rangeLow' | 'rangeHigh'> as `gen.${F}`]: GenerationParams[F] };
export type KeyValues = PatchKeyValues &
  GenKeyValues & {
    'gen.range': [number, number];
    bpm: number;
    playing: boolean;
    masterLevel: number;
    synthEnabled: boolean;
    effectsEnabled: boolean;
    autoEvolveEnabled: boolean;
    autoEvolveRate: number;
    seed: string;
    regenerate: boolean;
    mutate: boolean;
    exportMidi: string;
    savePreset: string;
    loadPreset: string;
  };
export type StoreKey = keyof KeyValues;

export interface State {
  engine: Snapshot | null;
  overlay: Record<string, unknown>; // optimistic value per key until that key settles
  playhead: PlayheadEvent;
  status: Status | null;
  draft: { lastManualDelay: number | null };
}

// How one request ended, for flows that branch on the token (the preset overwrite prompt). Called once, after the
// state is updated: the DispatchResult for a well-formed response, null when there was no usable one (rejected,
// malformed, timed out, or replaced by a newer pending value). Failures are still reported in the status line.
export type Settled = (outcome: DispatchResult | null) => void;

export interface Store {
  readonly bridge: Bridge;
  getState(): State;
  subscribe(listener: () => void): () => void;
  start(): () => void;
  send<K extends StoreKey>(key: K, command: Command, optimistic: KeyValues[K], onSettled?: Settled): void;
  setStatus(status: Status | null): void;
  setDraft(patch: Partial<State['draft']>): void;
}

export interface StoreOptions {
  settleTimeoutMs?: number; // how long one dispatch may stay in flight before its key is force-settled
}

export const DEFAULT_SETTLE_TIMEOUT_MS = 5000;
const MALFORMED_RESPONSE = 'malformed response';
const TIMED_OUT = 'request timed out';

// What a control displays: its optimistic value while the key is unsettled, otherwise the engine value.
export const shown = <K extends StoreKey>(state: State, key: K, fromEngine: KeyValues[K]): KeyValues[K] =>
  key in state.overlay ? (state.overlay[key] as KeyValues[K]) : fromEngine;

// The host promises a DispatchResult; anything else is treated as a failed request, never trusted.
const isDispatchResult = (result: unknown): result is DispatchResult => {
  if (typeof result !== 'object' || result === null) return false;
  const { ok, error, snapshot } = result as Record<string, unknown>;
  if (ok === true) return typeof snapshot === 'object' && snapshot !== null;
  return ok === false && typeof error === 'string';
};

export function createStore(bridge: Bridge, { settleTimeoutMs = DEFAULT_SETTLE_TIMEOUT_MS }: StoreOptions = {}): Store {
  let state: State = { engine: null, overlay: {}, playhead: { step: 0, playing: false }, status: null, draft: { lastManualDelay: null } };
  const listeners = new Set<() => void>();
  const set = (patch: Partial<State>) => {
    state = { ...state, ...patch };
    listeners.forEach((l) => l());
  };
  // Last resort for the trailing catches: a throwing listener must not become an unhandled rejection.
  const report = (e: unknown) => {
    try {
      set({ status: describeThrown(e) });
    } catch {
      /* the state is already updated; only a listener threw */
    }
  };

  const inFlight = new Set<string>();
  const pending = new Map<string, { command: Command; onSettled?: Settled }>();
  let seq = 0;
  let lastAppliedSeq = 0;

  const run = (key: string, command: Command, onSettled?: Settled): void => {
    inFlight.add(key);
    const mine = ++seq;
    let settled = false;
    let timer: ReturnType<typeof setTimeout> | undefined;

    // The sequence rule: a snapshot applies only if it is newer than the last applied one.
    const fresh = (snapshot: Snapshot): Partial<State> => {
      if (mine <= lastAppliedSeq) return {};
      lastAppliedSeq = mine;
      return { engine: snapshot };
    };
    // Ends this run exactly once (response, rejection or timeout, whichever comes first).
    const settle = (patch: Partial<State>, outcome: DispatchResult | null = null) => {
      if (settled) return;
      settled = true;
      clearTimeout(timer);
      inFlight.delete(key);
      const next = pending.get(key);
      pending.delete(key);
      if (next) run(key, next.command, next.onSettled);
      else patch = { ...patch, overlay: Object.fromEntries(Object.entries(state.overlay).filter(([k]) => k !== key)) };
      set(patch);
      try {
        onSettled?.(outcome);
      } catch (e) {
        report(e);
      }
    };
    const reconcile = (result: unknown): Partial<State> => {
      if (!isDispatchResult(result)) return { status: describeThrown(new Error(MALFORMED_RESPONSE)) };
      if (!result.ok) return { status: describe(command.name, result.error) };
      const status = describeSuccess(command); // before `fresh`, so a throw does not consume the sequence number
      return { ...fresh(result.snapshot), ...(status && { status }) };
    };
    const onResult = (result: unknown) => {
      if (settled) {
        // Late (after a timeout): the key already moved on, so leave its status and overlay alone.
        if (isDispatchResult(result) && result.ok) {
          const late = fresh(result.snapshot);
          if (late.engine) set(late);
        }
        return;
      }
      let patch: Partial<State>;
      try {
        patch = reconcile(result);
      } catch (e) {
        patch = { status: describeThrown(e) };
      }
      settle(patch, isDispatchResult(result) ? result : null);
    };
    const onError = (e: unknown) => settle({ status: describeThrown(e) }); // no-op once settled

    timer = setTimeout(() => {
      try {
        settle({ status: describeThrown(new Error(TIMED_OUT)) });
      } catch (e) {
        report(e);
      }
    }, settleTimeoutMs);
    const call = async (): Promise<DispatchResult> => bridge.dispatch(command); // a sync throw becomes a rejection
    void call().then(onResult, onError).catch(report);
  };

  return {
    bridge,
    getState: () => state,
    subscribe: (listener) => (listeners.add(listener), () => listeners.delete(listener)),
    start() {
      let alive = true;
      let gotPlayhead = false; // a live event is newer than the mount-time snapshot
      bridge.getSnapshot().then(
        (snapshot) => {
          if (!alive || !snapshot) return;
          set({ engine: state.engine ?? snapshot, ...(!gotPlayhead && { playhead: { step: snapshot.playheadStep, playing: snapshot.playing } }) });
        },
        (e) => alive && set({ status: describeThrown(e) }),
      );
      const offPlayhead = bridge.onPlayhead((playhead) => {
        gotPlayhead = true;
        set({ playhead });
      });
      const offSnapshot = bridge.onSnapshot((engine) => set({ engine }));
      return () => {
        alive = false;
        offPlayhead();
        offSnapshot();
      };
    },
    send(key, command, optimistic, onSettled) {
      set({ overlay: { ...state.overlay, [key]: optimistic } });
      if (inFlight.has(key)) {
        const replaced = pending.get(key);
        pending.set(key, { command, onSettled }); // latest wins
        if (replaced?.onSettled) {
          try {
            replaced.onSettled(null);
          } catch (e) {
            report(e);
          }
        }
      } else run(key, command, onSettled);
    },
    setStatus: (status) => set({ status }),
    setDraft: (patch) => set({ draft: { ...state.draft, ...patch } }),
  };
}
