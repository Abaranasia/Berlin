// Dependency-free external store (design D8): engine snapshot, optimistic overlay, playhead, status, draft.
import type { Bridge } from '../bridge/bridge';
import type { Command, DispatchResult, PlayheadEvent, Snapshot } from '../bridge/protocol';
import { describe, describeSuccess, describeThrown, type Status } from './status';

export interface State {
  engine: Snapshot | null;
  overlay: Record<string, unknown>; // optimistic value per key until that key settles
  playhead: PlayheadEvent;
  status: Status | null;
  draft: { lastManualDelay: number | null };
}

export interface Store {
  readonly bridge: Bridge;
  getState(): State;
  subscribe(listener: () => void): () => void;
  start(): () => void;
  send(key: string, command: Command, optimistic: unknown): void;
  setStatus(status: Status | null): void;
  setDraft(patch: Partial<State['draft']>): void;
}

// What a control displays: its optimistic value while the key is unsettled, otherwise the engine value.
export const shown = <T>(state: State, key: string, fromEngine: T): T => (key in state.overlay ? (state.overlay[key] as T) : fromEngine);

export function createStore(bridge: Bridge): Store {
  let state: State = { engine: null, overlay: {}, playhead: { step: 0, playing: false }, status: null, draft: { lastManualDelay: null } };
  const listeners = new Set<() => void>();
  const set = (patch: Partial<State>) => {
    state = { ...state, ...patch };
    listeners.forEach((l) => l());
  };

  const inFlight = new Set<string>();
  const pending = new Map<string, Command>();
  let seq = 0;
  let lastAppliedSeq = 0;

  const run = (key: string, command: Command): void => {
    inFlight.add(key);
    const mine = ++seq;
    const call = async (): Promise<DispatchResult> => bridge.dispatch(command); // a sync throw becomes a rejection
    void call()
      .then(
        (result): Partial<State> => {
          if (!result.ok) return { status: describe(command.name, result.error) };
          const applied = mine > lastAppliedSeq;
          if (applied) lastAppliedSeq = mine;
          const status = describeSuccess(command);
          return { ...(applied && { engine: result.snapshot }), ...(status && { status }) };
        },
        (e): Partial<State> => ({ status: describeThrown(e) }),
      )
      .then((patch) => {
        inFlight.delete(key);
        const next = pending.get(key);
        pending.delete(key);
        if (next) run(key, next);
        else patch = { ...patch, overlay: Object.fromEntries(Object.entries(state.overlay).filter(([k]) => k !== key)) };
        set(patch);
      });
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
    send(key, command, optimistic) {
      set({ overlay: { ...state.overlay, [key]: optimistic } });
      if (inFlight.has(key)) pending.set(key, command); // latest wins
      else run(key, command);
    },
    setStatus: (status) => set({ status }),
    setDraft: (patch) => set({ draft: { ...state.draft, ...patch } }),
  };
}
