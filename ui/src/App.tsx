import { useEffect, useRef, useState } from 'react';
import { Card } from './components/Card';
import { DelayTimesCard } from './components/DelayTimesCard';
import { EvolutionPanel } from './components/EvolutionPanel';
import { GenerationPanel } from './components/GenerationPanel';
import { PitchPanel } from './components/PitchPanel';
import { RhythmPanel } from './components/RhythmPanel';
import { SettingsDialog } from './components/SettingsDialog';
import { StatusLine } from './components/StatusLine';
import { StepView } from './components/StepView';
import { SynthDrawer } from './components/SynthDrawer';
import { TopBar } from './components/TopBar';
import { readSynthPanelOpen, writeSynthPanelOpen } from './lib/panelState';
import { StoreProvider, useStoreApi } from './store/context';
import type { Store } from './store/store';

function Shell() {
  const settings = useStoreApi().bridge.settings;
  const [settingsOpen, setSettingsOpen] = useState(false);
  const settingsButton = useRef<HTMLButtonElement>(null);
  const closeSettings = () => {
    setSettingsOpen(false);
    settingsButton.current?.focus();
  };
  const [synthOpen, setSynthOpen] = useState(() => readSynthPanelOpen());
  const toggleSynth = () => {
    const next = !synthOpen;
    writeSynthPanelOpen(next);
    setSynthOpen(next);
  };
  return (
    <div className="app">
      <TopBar
        synthOpen={synthOpen}
        onToggleSynth={toggleSynth}
        onOpenSettings={settings ? () => setSettingsOpen(true) : undefined}
        settingsButtonRef={settingsButton}
      />
      <main className="main-area">
        <Card title="Sequence">
          <StepView />
        </Card>
        <div className="card-grid">
          <Card title="Generation">
            <GenerationPanel />
          </Card>
          <Card title="Rhythm">
            <RhythmPanel />
          </Card>
          <Card title="Pitch">
            <PitchPanel />
          </Card>
          <Card title="Evolution">
            <EvolutionPanel />
          </Card>
        </div>
        <DelayTimesCard />
        <StatusLine />
      </main>
      <SynthDrawer open={synthOpen} />
      {settings && settingsOpen && <SettingsDialog settings={settings} onClose={closeSettings} />}
    </div>
  );
}

export function App({ store }: { store: Store }) {
  useEffect(() => store.start(), [store]);
  return (
    <StoreProvider store={store}>
      <Shell />
    </StoreProvider>
  );
}
