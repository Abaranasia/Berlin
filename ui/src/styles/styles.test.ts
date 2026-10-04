import { readFileSync } from 'node:fs';
import { describe, it, expect } from 'vitest';

// The stylesheets are read as text: CSS is never imported into the jsdom tests.
const read = (name: string) => readFileSync(new URL(`./${name}`, import.meta.url), 'utf8');

const rootBlock = (css: string) => /:root\s*\{([^}]*)\}/.exec(css)?.[1] ?? '';
const declared = (block: string, token: string) => new RegExp(`${token}\s*:\s*([^;]+);`).exec(block)?.[1].trim();

describe('tokens.css', () => {
  const root = rootBlock(read('tokens.css'));

  it.each([
    '--ground', '--surface-1', '--surface-2', '--border', '--text', '--muted',
    '--accent-hover', '--on-accent', '--radius-sm', '--radius-md', '--radius-lg', '--focus-ring',
  ])('defines %s on :root', (token) => {
    expect(declared(root, token)).toBeTruthy();
  });

  it('uses the amber accent', () => {
    expect(declared(root, '--accent')?.toLowerCase()).toBe('#f0a238');
  });

  it('uses the 6/8/12 px radii', () => {
    expect([declared(root, '--radius-sm'), declared(root, '--radius-md'), declared(root, '--radius-lg')]).toEqual(['6px', '8px', '12px']);
  });
});

describe('fonts.css', () => {
  const css = read('fonts.css');
  const urls = [...css.matchAll(/url\(\s*['"]?([^'")]+)['"]?\s*\)/g)].map((m) => m[1]);

  it('declares the four bundled faces', () => {
    expect(css.match(/@font-face/g)).toHaveLength(4);
  });

  it('only references local woff2 files (no network)', () => {
    expect(urls.length).toBeGreaterThan(0);
    for (const url of urls) {
      expect(url).not.toMatch(/^(https?:)?\/\//i);
      expect(url).toMatch(/\.woff2$/);
    }
  });
});
