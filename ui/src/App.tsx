import { useEffect, useRef, useState } from 'react';
import { dispatch, getSnapshot, onPlayhead } from './bridge/native';
import { stepRow } from './state/steps';

const DEFAULT_BPM = 120;

export function App() {
  const [bpm, setBpm] = useState(DEFAULT_BPM);
  const [playhead, setPlayhead] = useState({ step: 0, playing: false });
  const gotEvent = useRef(false); // a live event is newer than the mount-time snapshot

  useEffect(() => {
    let alive = true;
    void getSnapshot().then((snapshot) => {
      if (!alive || !snapshot) return;
      setBpm(snapshot.bpm);
      if (!gotEvent.current) setPlayhead({ step: snapshot.playheadStep, playing: snapshot.playing });
    });
    const off = onPlayhead((event) => {
      gotEvent.current = true;
      setPlayhead(event);
    });
    return () => {
      alive = false;
      off();
    };
  }, []);

  const changeBpm = (delta: number) => {
    void dispatch('setBpm', { bpm: bpm + delta }).then((result) => {
      if (result.ok && result.snapshot) setBpm(result.snapshot.bpm);
    });
  };

  const row = stepRow(playhead.step, playhead.playing);

  return (
    <main>
      <h1>Berlin</h1>
      <div className="bpm">
        <button type="button" onClick={() => changeBpm(-1)}>-</button>
        <span>{bpm}</span>
        <button type="button" onClick={() => changeBpm(1)}>+</button>
        <small>BPM</small>
      </div>
      <div className="steps">
        {row.map((active, i) => (
          <span key={i} className={active ? 'step active' : 'step'} data-step={i} data-active={active} />
        ))}
      </div>
    </main>
  );
}
