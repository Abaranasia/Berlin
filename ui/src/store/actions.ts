// Typed senders: one place maps a control to its command, key and optimistic value (see the key convention in store.ts).
import type { GenerationParams, Patch } from '../bridge/protocol';
import type { Store, StoreKey } from './store';

type LiveGenerationField = Exclude<keyof GenerationParams, 'rangeLow' | 'rangeHigh'>;

export function sendPatch<F extends keyof Patch>(store: Store, field: F, value: Patch[F]): void {
  store.send(`patch.${field}` as StoreKey, { name: 'setPatch', args: { [field]: value } as Partial<Patch> }, value as never);
}

export function sendGen<F extends LiveGenerationField>(store: Store, field: F, value: GenerationParams[F]): void {
  store.send(`gen.${field}` as StoreKey, { name: 'setGenerationParams', args: { [field]: value } as Partial<GenerationParams> }, value as never);
}

// rangeLow and rangeHigh always travel together so the host normalizes them as a pair.
export const sendRange = (store: Store, rangeLow: number, rangeHigh: number): void =>
  store.send('gen.range', { name: 'setGenerationParams', args: { rangeLow, rangeHigh } }, [rangeLow, rangeHigh]);
