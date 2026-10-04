export const SYNTH_PANEL_KEY = 'berlin.ui.synthPanelOpen';

const defaultStorage = (): Storage => window.localStorage;

// Only "1" reads as open. Reaching localStorage can itself throw (SecurityError), so the getter runs inside the try.
export function readSynthPanelOpen(getStorage: () => Storage = defaultStorage): boolean {
  try {
    return getStorage().getItem(SYNTH_PANEL_KEY) === '1';
  } catch {
    return false;
  }
}

export function writeSynthPanelOpen(open: boolean, getStorage: () => Storage = defaultStorage): void {
  try {
    getStorage().setItem(SYNTH_PANEL_KEY, open ? '1' : '0');
  } catch {
    // Persistence is best effort: the panel still toggles for this session.
  }
}
