// The only surface the UI uses to reach the host (nativeBridge) or a dev stand-in (mockBridge).
import type { ChooseExportResult, Command, DispatchResult, PlayheadEvent, SettingsBridge, Snapshot } from './protocol';

export interface Bridge {
  dispatch(command: Command): Promise<DispatchResult>;
  getSnapshot(): Promise<Snapshot | null>;
  onPlayhead(listener: (event: PlayheadEvent) => void): () => void;
  onSnapshot(listener: (snapshot: Snapshot) => void): () => void;
  chooseExportFile(): Promise<ChooseExportResult>;
  confirm(title: string, message: string): Promise<boolean>;
  // Present only in the standalone host (capability-detected); the plugin has no settings.
  settings?: SettingsBridge;
}
