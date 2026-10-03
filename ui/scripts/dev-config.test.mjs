import { describe, it, expect } from 'vitest';
import fs from 'node:fs';
import config from '../vite.config';

const pkg = JSON.parse(fs.readFileSync(new URL('../package.json', import.meta.url), 'utf8'));

describe('dev server config', () => {
  it('has a vite dev script', () => {
    expect(pkg.scripts.dev).toBe('vite');
  });
  it('pins port 5173 with strictPort (matches BERLIN_WEB_UI_DEV_URL)', () => {
    expect(config.server).toEqual({ port: 5173, strictPort: true });
  });
});
