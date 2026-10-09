import { useState } from 'react';
import { LIMITS } from '../bridge/limits';
import { useStoreApi } from '../store/context';
import { shownNow, useShown } from '../store/hooks';
import { Button } from './controls/Button';
import { Slider } from './controls/Slider';

const DEFAULT_BPM = 120;
const DEFAULT_MASTER_LEVEL = 0.8;
const clampBpm = (bpm: number) => Math.min(LIMITS.bpm.max, Math.max(LIMITS.bpm.min, Math.round(bpm)));

export function Transport() {
  const store = useStoreApi();
  const playing = useShown('playing', (s) => s.playing, false);
  const bpm = useShown('bpm', (s) => s.bpm, DEFAULT_BPM);
  const level = useShown('masterLevel', (s) => s.masterLevel, DEFAULT_MASTER_LEVEL);
  const [text, setText] = useState<string | null>(null); // BPM entry in progress, committed on Enter or blur

  const sendBpm = (next: number) => store.send('bpm', { name: 'setBpm', args: { bpm: next } }, next);
  // Read the displayed value at click time, not from a render closure, so rapid presses accumulate.
  const step = (delta: number) => sendBpm(clampBpm(shownNow(store.getState(), 'bpm', (s) => s.bpm, DEFAULT_BPM) + delta));
  const commit = () => {
    if (text === null) return;
    setText(null);
    if (text.trim() !== '' && Number.isFinite(Number(text))) sendBpm(clampBpm(Number(text)));
  };

  return (
    <section>
      <Button onClick={() => store.send('playing', { name: 'setPlaying', args: { playing: !playing } }, !playing)}>{playing ? 'Stop' : 'Play'}</Button>{' '}
      <Button onClick={() => step(-1)}>-</Button>{' '}
      <input
        type="number"
        aria-label="BPM"
        value={text ?? bpm}
        onChange={(e) => setText(e.target.value)}
        onBlur={commit}
        onKeyDown={(e) => e.key === 'Enter' && commit()}
      />{' '}
      <Button onClick={() => step(1)}>+</Button> <small>BPM</small>{' '}
      <Slider
        label="Master level"
        value={level}
        min={LIMITS.outputLevel.min}
        max={LIMITS.outputLevel.max}
        step={0.01}
        onChange={(next) => store.send('masterLevel', { name: 'setMasterLevel', args: { level: next } }, next)}
      />
    </section>
  );
}
