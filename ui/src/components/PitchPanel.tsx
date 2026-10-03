import { GENERATION_LIMITS } from '../bridge/limits';
import { SCALE_TYPES, type ScaleType } from '../bridge/protocol';
import { sendGen, sendRange } from '../store/actions';
import { useStore, useStoreApi } from '../store/context';
import { shownNow, useShown } from '../store/hooks';
import type { State } from '../store/store';
import { Select } from './controls/Select';
import { Slider } from './controls/Slider';

const ROOT_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
const ROOTS = ROOT_NAMES.map((label, i) => ({ value: String(i), label }));
const DEFAULT_RANGE: [number, number] = [36, 72];
const { rangeLow: PITCH } = GENERATION_LIMITS; // rangeLow and rangeHigh share the 0..127 bounds
const rangeOf = (state: State) =>
  shownNow(state, 'gen.range', (s) => [s.generationParams.rangeLow, s.generationParams.rangeHigh], DEFAULT_RANGE);

export function PitchPanel() {
  const store = useStoreApi();
  const scale = useShown('gen.scaleType', (s) => s.generationParams.scaleType, 'minor');
  const root = useShown('gen.rootPitchClass', (s) => s.generationParams.rootPitchClass, 0);
  // gen.range is a tuple and a selector must return a primitive, so each end is selected as its own number.
  const low = useStore((s) => rangeOf(s)[0]);
  const high = useStore((s) => rangeOf(s)[1]);
  // Read the pair at event time so the other end is never a stale render value; both always travel together.
  const currentRange = () => rangeOf(store.getState());

  return (
    <section>
      <Select label="Scale" value={scale} options={SCALE_TYPES} onChange={(next) => sendGen(store, 'scaleType', next as ScaleType)} />{' '}
      <Select label="Root" value={String(root)} options={ROOTS} onChange={(next) => sendGen(store, 'rootPitchClass', Number(next))} />{' '}
      <Slider label="Range low" value={low} min={PITCH.min} max={PITCH.max} step={1} onChange={(next) => sendRange(store, next, currentRange()[1])} />{' '}
      <Slider label="Range high" value={high} min={PITCH.min} max={PITCH.max} step={1} onChange={(next) => sendRange(store, currentRange()[0], next)} />
    </section>
  );
}
