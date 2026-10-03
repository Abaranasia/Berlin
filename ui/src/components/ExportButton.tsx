import { useStoreApi } from '../store/context';
import { describe, describeThrown } from '../store/status';
import { Button } from './controls/Button';

// Export flow (spec "Export Flow"): native chooser first, then exportMidi with the chosen path.
export function ExportButton() {
  const store = useStoreApi();

  const run = async () => {
    try {
      const chosen = await store.bridge.chooseExportFile();
      if (chosen.error) return store.setStatus(describe('exportMidi', chosen.error)); // busy: no dispatch
      if (chosen.cancelled) return; // cancel is a silent no-op
      store.send('exportMidi', { name: 'exportMidi', args: { path: chosen.path } }, chosen.path);
    } catch (e) {
      store.setStatus(describeThrown(e)); // rejected chooser or malformed result: never an unhandled rejection
    }
  };

  return <Button onClick={() => void run()}>Export MIDI</Button>;
}
