import { useEffect, useRef, useState } from 'react';
import { LIMITS } from '../bridge/limits';
import type { ApplyAudioSettingsRequest, AudioSettings, SettingsBridge, SettingsError } from '../bridge/protocol';
import { formatDb } from '../lib/db';
import { useStoreApi } from '../store/context';
import { useShown } from '../store/hooks';
import { Select, type Option } from './controls/Select';
import { Slider } from './controls/Slider';

const DEFAULT_MASTER_LEVEL = 0.8;
const CHANNELS: readonly Option[] = Array.from({ length: 16 }, (_, i) => String(i + 1));
const PORT_NONE = '';

const ERROR_TEXT: Record<SettingsError, string> = {
  'invalid-request': 'The settings are not valid.',
  'device-unavailable': 'The audio device is not available.',
  'sample-rate-unsupported': 'The device does not support that sample rate.',
  'buffer-size-unsupported': 'The device does not support that buffer size.',
  'midi-port-unavailable': 'The MIDI port is not available.',
  busy: 'Settings are being applied. Try again in a moment.',
};
const errorText = (token: string): string => ERROR_TEXT[token as SettingsError] ?? 'The settings could not be applied.';

const rateLabel = (hz: number): string => `${Number((hz / 1000).toFixed(3))} kHz`;
const FOCUSABLE = 'button:not([disabled]), select:not([disabled]), input:not([disabled]), [href]';

// What the dialog edits: the apply request. The output level is not part of it (it is live, via the store).
type Form = ApplyAudioSettingsRequest;
const toForm = (a: AudioSettings): Form => ({
  deviceType: a.deviceType, device: a.device, sampleRate: a.sampleRate, bufferSize: a.bufferSize,
  midiPortId: a.midiPortId, midiChannel: a.midiChannel,
});

interface SettingsDialogProps {
  settings: SettingsBridge;
  onClose(): void;
}

export function SettingsDialog({ settings, onClose }: SettingsDialogProps) {
  const store = useStoreApi();
  const level = useShown('masterLevel', (s) => s.masterLevel, DEFAULT_MASTER_LEVEL);
  const [audio, setAudio] = useState<AudioSettings | null>(null);
  const [form, setForm] = useState<Form | null>(null);
  const [loadFailed, setLoadFailed] = useState(false);
  const [applyError, setApplyError] = useState<string | null>(null);
  const [busy, setBusy] = useState(false);
  const dialogRef = useRef<HTMLDivElement>(null);
  const closeRef = useRef(onClose);
  closeRef.current = onClose;

  // Each query result replaces the lists and resets the form to what the host reports for that device.
  const query = async (q?: { deviceType: string; device: string }): Promise<AudioSettings | null> => {
    try {
      const next = q ? await settings.get(q) : await settings.get();
      if (!next || !Array.isArray(next.devices)) throw new Error('settings unavailable');
      return next;
    } catch {
      return null;
    }
  };
  const show = (next: AudioSettings, keep?: Partial<Form>) => {
    setAudio(next);
    setForm({ ...toForm(next), ...keep });
  };

  useEffect(() => {
    let alive = true;
    void query().then((next) => {
      if (!alive) return;
      if (next) show(next);
      else setLoadFailed(true);
    });
    return () => {
      alive = false;
    };
    // The dialog is mounted per open, so it lists once on mount.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  useEffect(() => dialogRef.current?.focus(), []);

  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      if (e.key === 'Escape') closeRef.current();
    };
    document.addEventListener('keydown', onKey);
    return () => document.removeEventListener('keydown', onKey);
  }, []);

  // Keep Tab inside the modal.
  const trapTab = (e: React.KeyboardEvent) => {
    if (e.key !== 'Tab' || !dialogRef.current) return;
    const items = [...dialogRef.current.querySelectorAll<HTMLElement>(FOCUSABLE)];
    if (items.length === 0) return;
    const first = items[0];
    const last = items[items.length - 1];
    if (e.shiftKey && (document.activeElement === first || document.activeElement === dialogRef.current)) {
      e.preventDefault();
      last.focus();
    } else if (!e.shiftKey && document.activeElement === last) {
      e.preventDefault();
      first.focus();
    }
  };

  const update = (patch: Partial<Form>) => setForm((f) => (f ? { ...f, ...patch } : f));

  const changeDevice = async (deviceType: string, device: string) => {
    const next = await query({ deviceType, device });
    if (!next) return setApplyError('device-unavailable');
    setApplyError(null);
    show(next, { deviceType, device });
  };

  // A new device type has its own device list: ask for it, then pick its first device.
  const changeType = async (deviceType: string) => {
    const listed = await query({ deviceType, device: '' });
    if (!listed) return setApplyError('device-unavailable');
    const device = listed.devices[0];
    if (device === undefined) return show(listed, { deviceType });
    await changeDevice(deviceType, device);
  };

  const apply = async () => {
    if (!form || busy) return;
    setBusy(true);
    setApplyError(null);
    try {
      const result = await settings.apply(form);
      if (result.ok) onClose();
      else setApplyError(result.error);
    } catch {
      setApplyError('device-unavailable');
    } finally {
      setBusy(false);
    }
  };

  const ready = audio !== null && form !== null;

  return (
    <div className="dialog-backdrop">
      <div
        ref={dialogRef}
        className="dialog"
        role="dialog"
        aria-modal="true"
        aria-labelledby="settings-title"
        tabIndex={-1}
        onKeyDown={trapTab}
      >
        <header className="dialog-head">
          <h2 id="settings-title">Audio &amp; MIDI settings</h2>
          <span className="dialog-badge">Standalone only</span>
          <button type="button" className="dialog-close" aria-label="Close settings" onClick={onClose}>
            <svg width="16" height="16" viewBox="0 0 16 16" fill="none" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" aria-hidden="true">
              <path d="M3 3l10 10M13 3L3 13" />
            </svg>
          </button>
        </header>
        <div className="dialog-body">
          {loadFailed && <p role="alert" className="dialog-error">The audio settings could not be read.</p>}
          {!loadFailed && !ready && <p className="hint">Loading...</p>}
          {ready && (
            <>
              <section className="dialog-section">
                <h3>Audio output</h3>
                <Select label="Device type" value={form.deviceType} options={audio.deviceTypes} onChange={(v) => void changeType(v)} />
                <Select label="Device" value={form.device} options={audio.devices} onChange={(v) => void changeDevice(form.deviceType, v)} />
                <div className="dialog-pair">
                  <Select
                    label="Sample rate"
                    value={String(form.sampleRate)}
                    options={audio.sampleRates.map((r) => ({ value: String(r), label: rateLabel(r) }))}
                    onChange={(v) => update({ sampleRate: Number(v) })}
                  />
                  <Select
                    label="Buffer size"
                    value={String(form.bufferSize)}
                    options={audio.bufferSizes.map((b) => ({ value: String(b), label: `${b} samples` }))}
                    onChange={(v) => update({ bufferSize: Number(v) })}
                  />
                </div>
              </section>
              <section className="dialog-section">
                <h3>MIDI output</h3>
                <Select
                  label="MIDI port"
                  value={form.midiPortId}
                  options={audio.midiPorts.map((p) => ({ value: p.id, label: p.id === PORT_NONE ? 'None' : p.name }))}
                  onChange={(v) => update({ midiPortId: v })}
                />
                <Select label="MIDI channel" value={String(form.midiChannel)} options={CHANNELS} onChange={(v) => update({ midiChannel: Number(v) })} />
              </section>
              <section className="dialog-section">
                <h3>Output level</h3>
                <Slider
                  label="Output level"
                  value={level}
                  min={LIMITS.outputLevel.min}
                  max={LIMITS.outputLevel.max}
                  step={0.01}
                  format={formatDb}
                  onChange={(next) => store.send('masterLevel', { name: 'setMasterLevel', args: { level: next } }, next)}
                />
              </section>
            </>
          )}
          {applyError && <p role="alert" className="dialog-error">{errorText(applyError)}</p>}
        </div>
        <footer className="dialog-foot">
          <button type="button" onClick={onClose}>Cancel</button>
          {ready && (
            <button type="button" className="primary" disabled={busy} onClick={() => void apply()}>
              Apply
            </button>
          )}
        </footer>
      </div>
    </div>
  );
}
