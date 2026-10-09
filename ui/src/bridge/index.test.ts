// @vitest-environment jsdom
import { describe, it, expect, vi } from 'vitest';
import { resolveBridge, resolveBridgeOrNative } from './index';
import { nativeBridge } from './native';
import type { Bridge } from './bridge';

const fakeMock = { name: 'mock' } as unknown as Bridge;

describe('resolveBridge({hasHost, loadMock})', () => {
  it('host present -> native, the mock is never loaded', async () => {
    const loadMock = vi.fn(async () => fakeMock);
    expect(await resolveBridge({ hasHost: true, loadMock })).toBe(nativeBridge);
    expect(loadMock).not.toHaveBeenCalled();
  });

  it('no host + loadMock -> mock', async () => {
    expect(await resolveBridge({ hasHost: false, loadMock: async () => fakeMock })).toBe(fakeMock);
  });

  it('no host + no loadMock (release build) -> native, which degrades', async () => {
    expect(await resolveBridge({ hasHost: false, loadMock: undefined })).toBe(nativeBridge);
  });
});

describe('resolveBridgeOrNative (main.tsx entry point)', () => {
  it('returns the resolved bridge and logs nothing on success', async () => {
    const log = vi.fn();
    expect(await resolveBridgeOrNative({ hasHost: false, loadMock: async () => fakeMock }, log)).toBe(fakeMock);
    expect(log).not.toHaveBeenCalled();
  });

  it('falls back to nativeBridge and logs when the mock import rejects', async () => {
    const log = vi.fn();
    const boom = new Error('chunk failed to load');
    const bridge = await resolveBridgeOrNative({ hasHost: false, loadMock: async () => { throw boom; } }, log);
    expect(bridge).toBe(nativeBridge);
    expect(log).toHaveBeenCalledTimes(1);
    expect(log.mock.calls[0]).toContain(boom);
  });

  it('falls back when loadMock throws synchronously', async () => {
    const log = vi.fn();
    const bridge = await resolveBridgeOrNative({ hasHost: false, loadMock: () => { throw new Error('sync'); } }, log);
    expect(bridge).toBe(nativeBridge);
    expect(log).toHaveBeenCalledTimes(1);
  });
});
