import { createRoot } from 'react-dom/client';
import { App } from './App';
import { hasNativeBridge, resolveBridge } from './bridge';
import { createStore } from './store/store';

// The dynamic import is lexically guarded by import.meta.env.DEV so Rollup drops the mock chunk from release builds.
const loadMock = import.meta.env.DEV ? () => import('./bridge/mock').then((m) => m.mockBridge) : undefined;

void resolveBridge({ hasHost: hasNativeBridge(), loadMock }).then((bridge) => {
  createRoot(document.getElementById('root')!).render(<App store={createStore(bridge)} />);
});
