import { LIMITS } from '../bridge/limits';
import { LFO_DESTINATIONS, WAVEFORMS, type LfoDestination, type Waveform } from '../bridge/protocol';
import { SKEWS } from '../lib/skew';
import { sendPatch } from '../store/actions';
import { useStoreApi } from '../store/context';
import { useShown } from '../store/hooks';
import { Select } from './controls/Select';
import { Slider } from './controls/Slider';

// Cutoff, resonance, ADSR times and LFO rate use the legacy skewed mappings; the rest are linear.
export function SynthPanel() {
  const store = useStoreApi();
  const waveform = useShown('patch.waveform', (s) => s.patch.waveform, 'saw');
  const pulseWidth = useShown('patch.pulseWidth', (s) => s.patch.pulseWidth, 0.5);
  const cutoff = useShown('patch.cutoffHz', (s) => s.patch.cutoffHz, 1000);
  const resonance = useShown('patch.resonance', (s) => s.patch.resonance, 1);
  const attack = useShown('patch.attack', (s) => s.patch.attack, 0.01);
  const decay = useShown('patch.decay', (s) => s.patch.decay, 0.2);
  const sustain = useShown('patch.sustain', (s) => s.patch.sustain, 0.7);
  const release = useShown('patch.release', (s) => s.patch.release, 0.3);
  const lfoDestination = useShown('patch.lfoDestination', (s) => s.patch.lfoDestination, 'cutoff');
  const lfoRate = useShown('patch.lfoRateHz', (s) => s.patch.lfoRateHz, 2);
  const lfoDepth = useShown('patch.lfoDepth', (s) => s.patch.lfoDepth, 0);

  return (
    <section>
      <Select label="Waveform" value={waveform} options={WAVEFORMS} onChange={(next) => sendPatch(store, 'waveform', next as Waveform)} />{' '}
      <Slider label="Pulse width" value={pulseWidth} min={LIMITS.pulseWidth.min} max={LIMITS.pulseWidth.max} step={0.01} onChange={(v) => sendPatch(store, 'pulseWidth', v)} />{' '}
      <Slider label="Cutoff" value={cutoff} min={LIMITS.cutoffHz.min} max={LIMITS.cutoffHz.max} skew={SKEWS.cutoffHz} onChange={(v) => sendPatch(store, 'cutoffHz', v)} />{' '}
      <Slider label="Resonance" value={resonance} min={LIMITS.resonance.min} max={LIMITS.resonance.max} skew={SKEWS.resonance} onChange={(v) => sendPatch(store, 'resonance', v)} />{' '}
      <Slider label="Attack" value={attack} min={LIMITS.attackSeconds.min} max={LIMITS.attackSeconds.max} skew={SKEWS.attack} onChange={(v) => sendPatch(store, 'attack', v)} />{' '}
      <Slider label="Decay" value={decay} min={LIMITS.decaySeconds.min} max={LIMITS.decaySeconds.max} skew={SKEWS.decay} onChange={(v) => sendPatch(store, 'decay', v)} />{' '}
      <Slider label="Sustain" value={sustain} min={LIMITS.sustain.min} max={LIMITS.sustain.max} step={0.01} onChange={(v) => sendPatch(store, 'sustain', v)} />{' '}
      <Slider label="Release" value={release} min={LIMITS.releaseSeconds.min} max={LIMITS.releaseSeconds.max} skew={SKEWS.release} onChange={(v) => sendPatch(store, 'release', v)} />{' '}
      <Select label="LFO destination" value={lfoDestination} options={LFO_DESTINATIONS} onChange={(next) => sendPatch(store, 'lfoDestination', next as LfoDestination)} />{' '}
      <Slider label="LFO rate" value={lfoRate} min={LIMITS.lfoRateHz.min} max={LIMITS.lfoRateHz.max} skew={SKEWS.lfoRateHz} onChange={(v) => sendPatch(store, 'lfoRateHz', v)} />{' '}
      <Slider label="LFO depth" value={lfoDepth} min={LIMITS.lfoDepth.min} max={LIMITS.lfoDepth.max} step={0.01} onChange={(v) => sendPatch(store, 'lfoDepth', v)} />
    </section>
  );
}
