import { GENERATION_LIMITS } from '../bridge/limits';
import { RHYTHM_MODES, type RhythmMode } from '../bridge/protocol';
import { sendGen } from '../store/actions';
import { useStoreApi } from '../store/context';
import { useShown } from '../store/hooks';
import { Select } from './controls/Select';
import { Slider } from './controls/Slider';

const { pulses: PULSES, rotation: ROTATION } = GENERATION_LIMITS;

// Every edit is sent live (spec "Live Generation Params"); nothing waits for Generate.
export function RhythmPanel() {
  const store = useStoreApi();
  const mode = useShown('gen.mode', (s) => s.generationParams.mode, 'random');
  const pulses = useShown('gen.pulses', (s) => s.generationParams.pulses, 5);
  const rotation = useShown('gen.rotation', (s) => s.generationParams.rotation, 0);
  const chance = useShown('gen.stepProbability', (s) => s.generationParams.stepProbability, 0.5);

  return (
    <section>
      <Select label="Rhythm mode" value={mode} options={RHYTHM_MODES} onChange={(next) => sendGen(store, 'mode', next as RhythmMode)} />{' '}
      <Slider label="Pulses" value={pulses} min={PULSES.min} max={PULSES.max} step={1} onChange={(next) => sendGen(store, 'pulses', next)} />{' '}
      <Slider label="Rotation" value={rotation} min={ROTATION.min} max={ROTATION.max} step={1} onChange={(next) => sendGen(store, 'rotation', next)} />{' '}
      <Slider label="Chance %" value={Math.round(chance * 100)} min={0} max={100} step={1} onChange={(next) => sendGen(store, 'stepProbability', next / 100)} />
    </section>
  );
}
