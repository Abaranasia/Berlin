import { copyText, type CopyText } from '../lib/clipboard';
import { delayRecommendations } from '../lib/delayRecs';
import { useStoreApi } from '../store/context';
import { useShown } from '../store/hooks';
import { Card } from './Card';

// One chip per tempo division; a click copies the whole-millisecond value for use in an external delay.
// `copy` is injectable for tests; a synchronous throw is treated like a rejection.
export function DelayTimesCard({ copy = copyText }: { copy?: CopyText }) {
  const store = useStoreApi();
  const bpm = useShown('bpm', (s) => s.bpm, 120);

  const copyMs = (ms: number) => {
    Promise.resolve()
      .then(() => copy(String(ms)))
      .then(
        () => store.setStatus({ text: `Copied ${ms} ms`, error: false }),
        () => store.setStatus({ text: 'Copy failed', error: true }),
      );
  };

  return (
    <Card title="Delay times">
      <p className="hint">Click a value to copy it for an external delay</p>
      <div className="chips">
        {delayRecommendations(bpm).map(({ label, ms }) => (
          <button key={label} type="button" className="chip" onClick={() => copyMs(ms)}>
            <span>{label}</span> <span>{ms} ms</span>
          </button>
        ))}
      </div>
    </Card>
  );
}
