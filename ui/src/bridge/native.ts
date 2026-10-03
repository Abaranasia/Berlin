// Thin wrapper over the JUCE WebView interop (window.__JUCE__). Degrades to no-ops without a host.
import { getNativeFunction } from './juce/index.js';

export interface Snapshot {
  bpm: number;
  playing: boolean;
  playheadStep: number;
}

export interface DispatchResult {
  ok: boolean;
  error: string;
  snapshot?: Snapshot;
}

export interface PlayheadEvent {
  step: number;
  playing: boolean;
}

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

const nativeDispatch = hasNativeBridge() ? getNativeFunction('dispatch') : undefined;
const nativeSnapshot = hasNativeBridge() ? getNativeFunction('getSnapshot') : undefined;

export async function dispatch(command: string, args: Record<string, unknown> = {}): Promise<DispatchResult> {
  if (!nativeDispatch) return { ok: false, error: 'native bridge unavailable' };
  return (await nativeDispatch(command, args)) as DispatchResult;
}

export async function getSnapshot(): Promise<Snapshot | null> {
  if (!nativeSnapshot) return null;
  return ((await nativeSnapshot()) as Snapshot | null) ?? null;
}

export function onPlayhead(listener: (event: PlayheadEvent) => void): () => void {
  const backend = host()?.backend;
  if (!hasNativeBridge() || !backend) return () => {};
  const token = backend.addEventListener('playhead', (payload) => listener(payload as PlayheadEvent));
  return () => backend.removeEventListener(token);
}
