import { createRoot } from 'react-dom/client';
import { App } from './App';
import { hasNativeBridge, resolveBridgeOrNative } from './bridge';
import { createStore } from './store/store';
import './styles/fonts.css';
import './styles/tokens.css';
import './styles/base.css';
import './styles/layout.css';
import './styles/components.css';

// The dynamic import is lexically guarded by import.meta.env.DEV so Rollup drops the mock chunk from release builds.
const loadMock = import.meta.env.DEV ? () => import('./bridge/mock').then((m) => m.mockBridge) : undefined;

void resolveBridgeOrNative({ hasHost: hasNativeBridge(), loadMock }).then((bridge) => {
  createRoot(document.getElementById('root')!).render(<App store={createStore(bridge)} />);
});
