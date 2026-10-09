import { createContext, useContext, useSyncExternalStore, type ReactNode } from 'react';
import type { State, Store } from './store';

const StoreContext = createContext<Store | null>(null);

export const StoreProvider = ({ store, children }: { store: Store; children: ReactNode }) => (
  <StoreContext.Provider value={store}>{children}</StoreContext.Provider>
);

export function useStoreApi(): Store {
  const store = useContext(StoreContext);
  if (!store) throw new Error('useStore must be used inside a StoreProvider');
  return store;
}

// The selector must return a primitive or an existing reference, never a fresh object.
export function useStore<T>(selector: (state: State) => T): T {
  const store = useStoreApi();
  return useSyncExternalStore(store.subscribe, () => selector(store.getState()));
}
