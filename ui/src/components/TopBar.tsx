import { ExportButton } from './ExportButton';
import { PresetPanel } from './PresetPanel';
import { Transport } from './Transport';

interface TopBarProps {
  synthOpen: boolean;
  onToggleSynth(): void;
}

export function TopBar({ synthOpen, onToggleSynth }: TopBarProps) {
  return (
    <header className="topbar">
      <h1 className="wordmark">
        Berlin<small>Sequencer</small>
      </h1>
      <Transport />
      <PresetPanel />
      <ExportButton />
      <button type="button" aria-label="Settings" disabled>
        Settings
      </button>
      <button type="button" aria-label="Synth panel" aria-expanded={synthOpen} aria-controls="synth-panel" onClick={onToggleSynth}>
        Synth
      </button>
    </header>
  );
}
