import type { Bridge } from './bridge';
import { nativeBridge } from './native';

export { hasNativeBridge } from './native';

// The mock arrives only through `loadMock`, which main.tsx guards with import.meta.env.DEV so Rollup
// drops the mock chunk from release builds (a runtime flag here would not).
export type ResolveOptions = { hasHost: boolean; loadMock?: () => Promise<Bridge> };

export async function resolveBridge({ hasHost, loadMock }: ResolveOptions): Promise<Bridge> {
  if (hasHost || !loadMock) return nativeBridge;
  return loadMock();
}

// Entry-point wrapper: a failed mock import must not leave a blank page, so it degrades to nativeBridge.
export async function resolveBridgeOrNative(options: ResolveOptions, log: (...args: unknown[]) => void = console.error): Promise<Bridge> {
  try {
    return await resolveBridge(options);
  } catch (e) {
    log('resolveBridge failed; falling back to nativeBridge', e);
    return nativeBridge;
  }
}
