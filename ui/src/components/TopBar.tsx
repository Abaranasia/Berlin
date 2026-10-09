import type { Ref } from 'react';
import { ExportButton } from './ExportButton';
import { PresetPanel } from './PresetPanel';
import { Transport } from './Transport';

interface TopBarProps {
  synthOpen: boolean;
  onToggleSynth(): void;
  // Standalone only: when absent (plugin) no Settings button is rendered.
  onOpenSettings?(): void;
  settingsButtonRef?: Ref<HTMLButtonElement>;
}

export function TopBar({ synthOpen, onToggleSynth, onOpenSettings, settingsButtonRef }: TopBarProps) {
  return (
    <header className="topbar">
      <h1 className="wordmark">
        Berlin<small>Sequencer</small>
      </h1>
      <Transport />
      <PresetPanel />
      <ExportButton />
      {onOpenSettings && (
        <button type="button" ref={settingsButtonRef} aria-label="Settings" onClick={onOpenSettings}>
          Settings
        </button>
      )}
      <button type="button" aria-label="Synth panel" aria-expanded={synthOpen} aria-controls="synth-panel" onClick={onToggleSynth}>
        Synth
      </button>
    </header>
  );
}
