// @vitest-environment jsdom
import { describe, it, expect, vi } from 'vitest';
import { resolveBridge } from './index';
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
