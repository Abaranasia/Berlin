import { LIMITS } from '../bridge/limits';
import { DELAY_DIVISIONS, type DelayDivision } from '../bridge/protocol';
import { formatDelayRecommendations } from '../lib/delayRecs';
import { sendPatch } from '../store/actions';
import { useStoreApi } from '../store/context';
import { shownNow, useShown } from '../store/hooks';
import { Select } from './controls/Select';
import { Slider } from './controls/Slider';
import { Toggle } from './controls/Toggle';

const DEFAULT_TIME = 0.5;

// Delay is part of the effects chain: every control is disabled while FX is off, and Time also while Sync is on.
export function DelayPanel() {
  const store = useStoreApi();
  const fx = useShown('effectsEnabled', (s) => s.effectsEnabled, true);
  const bpm = useShown('bpm', (s) => s.bpm, 120);
  const synced = useShown('patch.delaySynced', (s) => s.patch.delaySynced, false);
  const division = useShown('patch.delayDivision', (s) => s.patch.delayDivision, 'quarter');
  const time = useShown('patch.delayTimeSeconds', (s) => s.patch.delayTimeSeconds, DEFAULT_TIME);
  const feedback = useShown('patch.delayFeedback', (s) => s.patch.delayFeedback, 0.3);
  const mix = useShown('patch.delayMix', (s) => s.patch.delayMix, 0.2);

  // Sync on remembers the manual time; Sync off puts it back in the same command, so the host does not
  // overwrite it with a tempo-derived one first.
  const setSync = (next: boolean) => {
    if (next) {
      store.setDraft({ lastManualDelay: shownNow(store.getState(), 'patch.delayTimeSeconds', (s) => s.patch.delayTimeSeconds, DEFAULT_TIME) });
      sendPatch(store, 'delaySynced', true);
      return;
    }
    const { lastManualDelay } = store.getState().draft;
    store.send('patch.delaySynced', { name: 'setPatch', args: { delaySynced: false, ...(lastManualDelay !== null && { delayTimeSeconds: lastManualDelay }) } }, false);
  };

  return (
    <section>
      <Toggle label="Delay sync" checked={synced} disabled={!fx} onChange={setSync} />{' '}
      <Select label="Delay division" value={division} options={DELAY_DIVISIONS} disabled={!fx} onChange={(next) => sendPatch(store, 'delayDivision', next as DelayDivision)} />{' '}
      <Slider label="Delay time" value={time} min={LIMITS.delayTimeSeconds.min} max={LIMITS.delayTimeSeconds.max} step={0.01} disabled={!fx || synced} onChange={(v) => sendPatch(store, 'delayTimeSeconds', v)} />{' '}
      <Slider label="Delay feedback" value={feedback} min={LIMITS.delayFeedback.min} max={LIMITS.delayFeedback.max} step={0.01} disabled={!fx} onChange={(v) => sendPatch(store, 'delayFeedback', v)} />{' '}
      <Slider label="Delay mix" value={mix} min={LIMITS.delayMix.min} max={LIMITS.delayMix.max} step={0.01} disabled={!fx} onChange={(v) => sendPatch(store, 'delayMix', v)} />{' '}
      <small data-role="delay-recommendations">{formatDelayRecommendations(bpm)}</small>
    </section>
  );
}
