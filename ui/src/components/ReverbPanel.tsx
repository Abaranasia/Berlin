import { LIMITS } from '../bridge/limits';
import { sendPatch } from '../store/actions';
import { useStoreApi } from '../store/context';
import { useShown } from '../store/hooks';
import { Slider } from './controls/Slider';

// Reverb is part of the effects chain: every control is disabled while FX is off.
export function ReverbPanel() {
  const store = useStoreApi();
  const fx = useShown('effectsEnabled', (s) => s.effectsEnabled, true);
  const room = useShown('patch.reverbRoomSize', (s) => s.patch.reverbRoomSize, 0.4);
  const damping = useShown('patch.reverbDamping', (s) => s.patch.reverbDamping, 0.5);
  const wet = useShown('patch.reverbWetLevel', (s) => s.patch.reverbWetLevel, 0.2);
  const dry = useShown('patch.reverbDryLevel', (s) => s.patch.reverbDryLevel, 0.8);

  return (
    <section>
      <Slider label="Room" value={room} min={LIMITS.reverbRoomSize.min} max={LIMITS.reverbRoomSize.max} step={0.01} disabled={!fx} onChange={(v) => sendPatch(store, 'reverbRoomSize', v)} />{' '}
      <Slider label="Damping" value={damping} min={LIMITS.reverbDamping.min} max={LIMITS.reverbDamping.max} step={0.01} disabled={!fx} onChange={(v) => sendPatch(store, 'reverbDamping', v)} />{' '}
      <Slider label="Wet" value={wet} min={LIMITS.reverbWetLevel.min} max={LIMITS.reverbWetLevel.max} step={0.01} disabled={!fx} onChange={(v) => sendPatch(store, 'reverbWetLevel', v)} />{' '}
      <Slider label="Dry" value={dry} min={LIMITS.reverbDryLevel.min} max={LIMITS.reverbDryLevel.max} step={0.01} disabled={!fx} onChange={(v) => sendPatch(store, 'reverbDryLevel', v)} />
    </section>
  );
}
