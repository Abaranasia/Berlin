import type { Snapshot } from '../bridge/protocol';
import { useStore } from './context';
import { shown, type KeyValues, type State, type StoreKey } from './store';

// The displayed value of a control: its optimistic value, else the snapshot field, else a default before the first snapshot.
export const shownNow = <K extends StoreKey>(state: State, key: K, read: (snapshot: Snapshot) => KeyValues[K], fallback: KeyValues[K]): KeyValues[K] =>
  shown(state, key, state.engine ? read(state.engine) : fallback);

// Selector hook form. The value must be a primitive (tuples such as gen.range are read per element).
export const useShown = <K extends StoreKey>(key: K, read: (snapshot: Snapshot) => KeyValues[K], fallback: KeyValues[K]): KeyValues[K] =>
  useStore((state) => shownNow(state, key, read, fallback));
