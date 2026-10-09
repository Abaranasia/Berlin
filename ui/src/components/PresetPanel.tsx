import { useState } from 'react';
import { useStore, useStoreApi } from '../store/context';
import { describeThrown } from '../store/status';
import { Button } from './controls/Button';
import { Select } from './controls/Select';

const NONE = '';
const NO_PRESETS: readonly string[] = [];

// Save flow (spec "Save Flow"): savePreset {overwrite:false}; on `exists`, ask the host to confirm; only then overwrite:true.
export function PresetPanel() {
  const store = useStoreApi();
  const presetNames = useStore((s) => s.engine?.presetNames ?? NO_PRESETS);
  const [name, setName] = useState('');
  const [selected, setSelected] = useState(NONE);
  const choice = presetNames.includes(selected) ? selected : NONE; // a preset that vanished from the list is no selection
  const trimmed = name.trim();

  const overwrite = async (presetName: string) => {
    try {
      store.setStatus(null); // the `exists` failure is a question here, not an error
      if (await store.bridge.confirm('Overwrite preset', `A preset named "${presetName}" already exists. Overwrite it?`)) {
        store.send('savePreset', { name: 'savePreset', args: { name: presetName, overwrite: true } }, presetName);
      }
    } catch (e) {
      store.setStatus(describeThrown(e)); // a rejected dialog never escapes
    }
  };
  const save = () => {
    setName(trimmed);
    store.send('savePreset', { name: 'savePreset', args: { name: trimmed, overwrite: false } }, trimmed, (outcome) => {
      if (outcome && !outcome.ok && outcome.error === 'exists') void overwrite(trimmed);
    });
  };
  const load = () => {
    store.send('loadPreset', { name: 'loadPreset', args: { name: choice } }, choice, (outcome) => {
      if (outcome?.ok) setName(choice); // the name field follows a successful load only
    });
  };

  return (
    <section>
      <label>
        Preset name{' '}
        <input type="text" aria-label="Preset name" value={name} onChange={(e) => setName(e.target.value)} />
      </label>{' '}
      <Button disabled={trimmed === ''} onClick={save}>
        Save
      </Button>{' '}
      <Select label="Presets" value={choice} options={[NONE, ...presetNames]} onChange={setSelected} />{' '}
      <Button disabled={choice === NONE} onClick={load}>
        Load
      </Button>
    </section>
  );
}
