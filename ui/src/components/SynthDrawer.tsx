import { DelayPanel } from './DelayPanel';
import { EngineToggles } from './EngineToggles';
import { ReverbPanel } from './ReverbPanel';
import { SynthPanel } from './SynthPanel';

// Always mounted: `hidden` only collapses it, so every control stays addressable.
export function SynthDrawer({ open }: { open: boolean }) {
  return (
    <aside id="synth-panel" className="drawer" aria-label="Internal synth" hidden={!open}>
      <EngineToggles />
      <SynthPanel />
      <DelayPanel />
      <ReverbPanel />
    </aside>
  );
}
