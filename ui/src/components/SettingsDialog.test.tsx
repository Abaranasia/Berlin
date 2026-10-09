// @vitest-environment jsdom
import { describe, it, expect, afterEach, vi } from 'vitest';
import { act } from 'react';
import { SettingsDialog } from './SettingsDialog';
import { change, click, field, makeHarness, render, settle } from '../testing/harness';
import type { ApplyAudioSettingsResult, AudioSettings, SettingsBridge } from '../bridge/protocol';

const AUDIO: AudioSettings = {
  deviceTypes: ['Windows Audio', 'ASIO'], deviceType: 'Windows Audio',
  devices: ['Speakers', 'Headphones'], device: 'Speakers',
  sampleRates: [44100, 48000], sampleRate: 48000,
  bufferSizes: [256, 512], bufferSize: 256,
  midiPorts: [{ id: '', name: 'None' }, { id: 'a', name: 'Synth A' }, { id: 'b', name: 'Synth B' }],
  midiPortId: 'a', midiChannel: 3,
};
const HEADPHONES: AudioSettings = { ...AUDIO, device: 'Headphones', sampleRates: [96000], sampleRate: 96000, bufferSizes: [128], bufferSize: 128 };

let unmount = () => {};
afterEach(() => unmount());

function makeSettings(over: Partial<SettingsBridge> = {}) {
  const get = vi.fn<SettingsBridge['get']>(async (query) => (query?.device === 'Headphones' ? HEADPHONES : AUDIO));
  const apply = vi.fn<SettingsBridge['apply']>(async (): Promise<ApplyAudioSettingsResult> => ({ ok: true }));
  return { settings: { get, apply, ...over } as SettingsBridge, get, apply };
}

async function open(settingsOver: Partial<SettingsBridge> = {}) {
  const s = makeSettings(settingsOver);
  const onClose = vi.fn();
  const harness = makeHarness();
  const mounted = await render(<SettingsDialog settings={s.settings} onClose={onClose} />, harness);
  unmount = mounted.unmount;
  await settle();
  return { ...mounted, ...s, onClose };
}
const select = (host: HTMLElement, label: string) => field<HTMLSelectElement>(host, label);
const buttonByText = (host: HTMLElement, text: string) => [...host.querySelectorAll('button')].find((b) => b.textContent === text)!;
const pressEscape = () => act(async () => void document.dispatchEvent(new KeyboardEvent('keydown', { key: 'Escape', bubbles: true })));
const readout = (host: HTMLElement) => field(host, 'Output level').parentElement!.querySelector('output')!.textContent;

describe('SettingsDialog contents', () => {
  it('renders a modal dialog populated from get()', async () => {
    const { host, get } = await open();
    expect(host.querySelector('[role="dialog"]')!.getAttribute('aria-modal')).toBe('true');
    expect(get).toHaveBeenCalledWith();
    expect(select(host, 'Device type').value).toBe('Windows Audio');
    expect(select(host, 'Device').value).toBe('Speakers');
    expect(select(host, 'Sample rate').value).toBe('48000');
    expect(select(host, 'Buffer size').value).toBe('256');
    expect(select(host, 'MIDI port').value).toBe('a');
    expect(select(host, 'MIDI channel').value).toBe('3');
    expect(select(host, 'MIDI channel').options).toHaveLength(16);
  });

  it('offers None as a selectable port', async () => {
    const { host } = await open();
    const port = select(host, 'MIDI port');
    expect([...port.options].map((o) => o.textContent)).toContain('None');
    await change(port, '');
    expect(port.value).toBe('');
  });

  it('shows an error status and no form when get() fails', async () => {
    const { host } = await open({ get: async () => { throw new Error('boom'); } });
    expect(host.querySelector('[role="alert"]')).not.toBeNull();
    expect(host.querySelector('[aria-label="Device"]')).toBeNull();
    expect(buttonByText(host, 'Apply')).toBeUndefined();
  });

  it('treats an {ok:false} answer from get() as a failure', async () => {
    const { host } = await open({ get: async () => ({ ok: false, error: 'busy' }) as unknown as AudioSettings });
    expect(host.querySelector('[role="alert"]')).not.toBeNull();
    expect(host.querySelector('[aria-label="Device"]')).toBeNull();
  });

  it('re-queries on a device change and resets rate and buffer to that device', async () => {
    const { host, get } = await open();
    await change(select(host, 'Device'), 'Headphones');
    await settle();
    expect(get).toHaveBeenLastCalledWith({ deviceType: 'Windows Audio', device: 'Headphones' });
    expect(select(host, 'Device').value).toBe('Headphones');
    expect([...select(host, 'Sample rate').options].map((o) => o.value)).toEqual(['96000']);
    expect(select(host, 'Sample rate').value).toBe('96000');
    expect(select(host, 'Buffer size').value).toBe('128');
  });

  it('shows the output level with a dB readout', async () => {
    const { host } = await open();
    expect(field(host, 'Output level').type).toBe('range');
    expect(readout(host)).toMatch(/dB$/);
  });
});

describe('SettingsDialog apply and cancel', () => {
  it('sends the exact request and closes on ok', async () => {
    const { host, apply, onClose } = await open();
    await change(select(host, 'Sample rate'), '44100');
    await change(select(host, 'MIDI port'), 'b');
    await change(select(host, 'MIDI channel'), '5');
    await click(buttonByText(host, 'Apply'));
    expect(apply).toHaveBeenCalledWith({ deviceType: 'Windows Audio', device: 'Speakers', sampleRate: 44100, bufferSize: 256, midiPortId: 'b', midiChannel: 5 });
    expect(onClose).toHaveBeenCalledTimes(1);
  });

  it('sends the None port as an empty id', async () => {
    const { host, apply } = await open();
    await change(select(host, 'MIDI port'), '');
    await click(buttonByText(host, 'Apply'));
    expect(apply.mock.calls[0][0].midiPortId).toBe('');
  });

  it('stays open with an alert and keeps the edits on an error token', async () => {
    const { host, onClose } = await open({ apply: async () => ({ ok: false, error: 'midi-port-unavailable' }) });
    await change(select(host, 'MIDI channel'), '7');
    await click(buttonByText(host, 'Apply'));
    expect(onClose).not.toHaveBeenCalled();
    expect(host.querySelector('[role="alert"]')).not.toBeNull();
    expect(select(host, 'MIDI channel').value).toBe('7');
  });

  it.each(['Cancel', 'Close', 'Escape'])('%s closes without applying', async (how) => {
    const { host, apply, onClose } = await open();
    await change(select(host, 'MIDI channel'), '9');
    if (how === 'Cancel') await click(buttonByText(host, 'Cancel'));
    else if (how === 'Close') await click(field<HTMLButtonElement>(host, 'Close settings'));
    else await pressEscape();
    expect(onClose).toHaveBeenCalledTimes(1);
    expect(apply).not.toHaveBeenCalled();
  });

  it('re-lists on every open', async () => {
    const first = await open();
    expect(first.get).toHaveBeenCalledTimes(1);
    first.unmount();
    const second = await open({ get: vi.fn(async () => ({ ...AUDIO, devices: ['Speakers', 'USB DAC'] })) });
    expect([...select(second.host, 'Device').options].map((o) => o.value)).toEqual(['Speakers', 'USB DAC']);
  });
});

describe('SettingsDialog output level', () => {
  it('dispatches setMasterLevel, never applies, and is not reverted by Cancel', async () => {
    const { host, dispatch, apply, onClose } = await open();
    await change(field(host, 'Output level'), '0.5');
    expect(dispatch).toHaveBeenCalledWith({ name: 'setMasterLevel', args: { level: 0.5 } });
    expect(apply).not.toHaveBeenCalled();
    expect(readout(host)).toBe('-6.0 dB');
    await click(buttonByText(host, 'Cancel'));
    expect(onClose).toHaveBeenCalled();
    expect(dispatch).toHaveBeenCalledTimes(1);
  });
});
