// Thin wrapper over the JUCE WebView interop (window.__JUCE__). Degrades to no-ops without a host.
import { getNativeFunction } from './juce/index.js';
import type { Bridge } from './bridge';
import type { ChooseExportResult, DispatchResult, Snapshot } from './protocol';

interface JuceHost {
  initialisationData: { __juce__functions: string[] };
  backend: {
    addEventListener(id: string, fn: (payload: unknown) => void): unknown;
    removeEventListener(token: unknown): void;
  };
}

const host = (): JuceHost | undefined => (window as unknown as { __JUCE__?: JuceHost }).__JUCE__;

// The JUCE script installs a placeholder host when none exists, so look for the registered function.
export const hasNativeBridge = (): boolean => host()?.initialisationData?.__juce__functions?.includes('dispatch') === true;

const native = (name: string) => (hasNativeBridge() ? getNativeFunction(name) : undefined);
const nativeDispatch = native('dispatch');
const nativeSnapshot = native('getSnapshot');
const nativeChooseExport = native('chooseExportFile');
const nativeConfirm = native('confirm');

const subscribe = <T>(event: string, listener: (payload: T) => void): (() => void) => {
  const backend = host()?.backend;
  if (!hasNativeBridge() || !backend) return () => {};
  const token = backend.addEventListener(event, (payload) => listener(payload as T));
  return () => backend.removeEventListener(token);
};

export const nativeBridge: Bridge = {
  async dispatch(command) {
    if (!nativeDispatch) return { ok: false, error: 'native bridge unavailable' };
    return (await nativeDispatch(command.name, command.args)) as DispatchResult;
  },
  async getSnapshot() {
    return nativeSnapshot ? (((await nativeSnapshot()) as Snapshot | null) ?? null) : null;
  },
  onPlayhead: (listener) => subscribe('playhead', listener),
  onSnapshot: (listener) => subscribe('snapshot', listener),
  async chooseExportFile() {
    return nativeChooseExport ? ((await nativeChooseExport()) as ChooseExportResult) : { cancelled: true, path: '' };
  },
  async confirm(title, message) {
    return nativeConfirm ? (await nativeConfirm(title, message)) === true : false;
  },
};
