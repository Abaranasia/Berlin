// Test-only helpers: react-dom + act with a spied Bridge backed by the dev mock engine (design D13, no @testing-library).
import { act, type ReactElement } from 'react';
import { createRoot } from 'react-dom/client';
import { vi } from 'vitest';
import { App } from '../App';
import type { Bridge } from '../bridge/bridge';
import { createMockBridge } from '../bridge/mock';
import type { Command } from '../bridge/protocol';
import { StoreProvider } from '../store/context';
import { createStore, type Store } from '../store/store';

(globalThis as { IS_REACT_ACT_ENVIRONMENT?: boolean }).IS_REACT_ACT_ENVIRONMENT = true;

// `over` replaces individual Bridge members; `dispatch` stays a spy either way.
export function makeHarness(over: Partial<Bridge> = {}, options: { busy?: boolean } = {}) {
  const base = createMockBridge(options);
  const dispatch = vi.fn((command: Command) => (over.dispatch ?? base.dispatch)(command));
  const bridge: Bridge = { ...base, ...over, dispatch };
  return { bridge, dispatch, store: createStore(bridge) };
}
export type Harness = ReturnType<typeof makeHarness>;

const mountInto = async (render: (host: HTMLElement) => ReactElement, before: () => () => void) => {
  const host = document.createElement('div');
  document.body.appendChild(host);
  const root = createRoot(host);
  let stop = () => {};
  await act(async () => {
    stop = before();
    root.render(render(host));
  });
  return {
    host,
    unmount: () => {
      act(() => root.unmount());
      stop();
      host.remove();
    },
  };
};

// A component that needs the store: provided and started, as App does.
export const render = async (element: ReactElement, harness: Harness = makeHarness()) => {
  const mounted = await mountInto(() => <StoreProvider store={harness.store}>{element}</StoreProvider>, () => harness.store.start());
  return { ...harness, ...mounted };
};

// The whole App: it provides and starts its own store.
export const renderApp = async (harness: Harness = makeHarness()) => {
  const mounted = await mountInto(() => <App store={harness.store} />, () => () => {});
  return { ...harness, ...mounted };
};

// A pure component (no store).
export const renderPlain = (element: ReactElement) => mountInto(() => element, () => () => {});

export const field = <T extends HTMLElement = HTMLInputElement>(host: HTMLElement, label: string): T => {
  const el = host.querySelector<T>(`[aria-label="${label}"]`);
  if (!el) throw new Error(`no control labelled "${label}"`);
  return el;
};
export const button = (host: HTMLElement, label: string): HTMLButtonElement => {
  const el = [...host.querySelectorAll('button')].find((b) => b.textContent === label);
  if (!el) throw new Error(`no button "${label}"`);
  return el;
};

// Sets a controlled input's value the way a user edit does, bypassing React's value tracker.
export const change = (el: HTMLInputElement | HTMLSelectElement, value: string) =>
  act(async () => {
    const proto = el instanceof HTMLSelectElement ? HTMLSelectElement.prototype : HTMLInputElement.prototype;
    Object.getOwnPropertyDescriptor(proto, 'value')!.set!.call(el, value);
    el.dispatchEvent(new Event(el instanceof HTMLSelectElement ? 'change' : 'input', { bubbles: true }));
  });
export const click = (el: HTMLElement) => act(async () => el.click());
export const pressEnter = (el: HTMLElement) => act(async () => void el.dispatchEvent(new KeyboardEvent('keydown', { key: 'Enter', bubbles: true })));
export const blur = (el: HTMLElement) => act(async () => void el.dispatchEvent(new FocusEvent('focusout', { bubbles: true })));
// Lets pending promise continuations (dispatch responses) run inside act.
export const settle = () => act(async () => {});
