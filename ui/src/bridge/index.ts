import type { Bridge } from './bridge';
import { nativeBridge } from './native';

export { hasNativeBridge } from './native';

// The mock arrives only through `loadMock`, which main.tsx guards with import.meta.env.DEV so Rollup
// drops the mock chunk from release builds (a runtime flag here would not).
export async function resolveBridge({ hasHost, loadMock }: { hasHost: boolean; loadMock?: () => Promise<Bridge> }): Promise<Bridge> {
  if (hasHost || !loadMock) return nativeBridge;
  return loadMock();
}
