import { useEffect } from 'react';
import { DelayPanel } from './components/DelayPanel';
import { EngineToggles } from './components/EngineToggles';
import { EvolutionPanel } from './components/EvolutionPanel';
import { ExportButton } from './components/ExportButton';
import { GenerationPanel } from './components/GenerationPanel';
import { PitchPanel } from './components/PitchPanel';
import { PresetPanel } from './components/PresetPanel';
import { ReverbPanel } from './components/ReverbPanel';
import { RhythmPanel } from './components/RhythmPanel';
import { StatusLine } from './components/StatusLine';
import { SynthPanel } from './components/SynthPanel';
import { Transport } from './components/Transport';
import { StoreProvider, useStore } from './store/context';
import type { Store } from './store/store';
import { STEP_COUNT, stepRow } from './state/steps';

// The pattern row: the playhead highlight plus each step's note and gate from the snapshot.
function Steps() {
  const playhead = useStore((s) => s.playhead);
  const steps = useStore((s) => s.engine?.steps);
  const row = stepRow(playhead.step, playhead.playing);
  return (
    <div className="steps">
      {Array.from({ length: STEP_COUNT }, (_, i) => (
        <span
          key={i}
          className={row[i] ? 'step active' : 'step'}
          data-step={i}
          data-active={row[i]}
          data-note={steps?.[i]?.note}
          data-gate={steps?.[i]?.active}
        />
      ))}
    </div>
  );
}

function Main() {
  return (
    <main>
      <h1>Berlin</h1>
      <Transport />
      <EngineToggles />
      <GenerationPanel />
      <RhythmPanel />
      <PitchPanel />
      <EvolutionPanel />
      <SynthPanel />
      <DelayPanel />
      <ReverbPanel />
      <PresetPanel />
      <ExportButton />
      <Steps />
      <StatusLine />
    </main>
  );
}

export function App({ store }: { store: Store }) {
  useEffect(() => store.start(), [store]);
  return (
    <StoreProvider store={store}>
      <Main />
    </StoreProvider>
  );
}
