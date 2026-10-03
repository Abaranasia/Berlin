import { describe, it, expect } from 'vitest';
import { readWebUiFlag, renderAssetSources, computeStamp, runPipeline } from './embed-lib.mjs';

const jucer = (defines, extra = '') =>
  `<?xml version="1.0"?>\n<JUCERPROJECT id="x" name="Berlin" ${defines === null ? '' : `defines="${defines}"`}>\n${extra}</JUCERPROJECT>`;

describe('readWebUiFlag', () => {
  it('is on for BERLIN_WEB_UI=1', () => {
    expect(readWebUiFlag(jucer('BERLIN_WEB_UI=1'))).toBe(true);
  });
  it('is off when absent, no defines, or =0', () => {
    expect(readWebUiFlag(jucer('FOO=1'))).toBe(false);
    expect(readWebUiFlag(jucer(null))).toBe(false);
    expect(readWebUiFlag(jucer('BERLIN_WEB_UI=0'))).toBe(false);
  });
  it.each([['&#10;'], [','], [' ']])('is on next to the plugin define separated by %j', (sep) => {
    expect(readWebUiFlag(jucer(`JUCE_VST3_CAN_REPLACE_VST2=0${sep}BERLIN_WEB_UI=1`))).toBe(true);
  });
  it('throws when the flag appears outside JUCERPROJECT defines', () => {
    const xml = jucer('FOO=1', '<CONFIGURATION defines="BERLIN_WEB_UI=1"/>');
    expect(() => readWebUiFlag(xml)).toThrow(/BERLIN_WEB_UI/);
  });
});

describe('renderAssetSources', () => {
  it('stub mode emits an empty table and FULL 0', () => {
    const { header, source } = renderAssetSources({ mode: 'stub', assets: [] });
    expect(header).toContain('#define BERLIN_EMBEDDED_ASSETS_FULL 0');
    expect(source).toContain('getEmbeddedAssets');
    expect(source).toContain('{ nullptr, 0 }');
  });
  it('full mode emits FULL 1 and one entry per file with normalised paths', () => {
    const assets = [
      { path: 'assets\\a.js', data: Buffer.from([0, 255, 16]) },
      { path: 'index.html', data: Buffer.from('hi') },
    ];
    const { header, source } = renderAssetSources({ mode: 'full', assets });
    expect(header).toContain('#define BERLIN_EMBEDDED_ASSETS_FULL 1');
    expect(source).toContain('"assets/a.js"');
    expect(source).toContain('"index.html"');
    expect(source).toContain('0x00,0xff,0x10');
    expect(source).toContain('0x68,0x69');
    expect(source).toContain(', 3 }');
  });
  it('is deterministic', () => {
    const assets = [{ path: 'index.html', data: Buffer.from('x') }];
    expect(renderAssetSources({ mode: 'full', assets })).toEqual(renderAssetSources({ mode: 'full', assets }));
  });
});

describe('computeStamp', () => {
  const inputs = [
    { path: 'src/App.tsx', data: Buffer.from('a') },
    { path: 'package.json', data: Buffer.from('b') },
  ];
  it('is stable for same mode and inputs', () => {
    expect(computeStamp('full', inputs)).toBe(computeStamp('full', inputs));
  });
  it('changes when a byte changes', () => {
    const changed = [inputs[0], { path: 'package.json', data: Buffer.from('c') }];
    expect(computeStamp('full', changed)).not.toBe(computeStamp('full', inputs));
  });
  it('changes with mode', () => {
    expect(computeStamp('stub', inputs)).not.toBe(computeStamp('full', inputs));
  });
});

describe('runPipeline', () => {
  const makeIo = (initial = {}) => {
    const files = { ...initial };
    const writes = [];
    return {
      files,
      writes,
      stampInputs: () => [{ path: 'package.json', data: Buffer.from('p') }],
      collectAssets: () => [{ path: 'index.html', data: Buffer.from('x') }],
      read: (name) => (name in files ? files[name] : null),
      writeIfChanged: (name, content) => {
        if (files[name] === content) return false;
        files[name] = content;
        writes.push(name);
        return true;
      },
    };
  };
  const okSpawn = () => {
    const calls = [];
    const fn = (cmd, args) => {
      calls.push([cmd, ...args].join(' '));
      return { status: 0 };
    };
    return { fn, calls };
  };

  it('flag off does not spawn and writes the stub', () => {
    const io = makeIo();
    const s = okSpawn();
    runPipeline({ flagOn: false, spawn: s.fn, io });
    expect(s.calls).toEqual([]);
    expect(io.files['EmbeddedAssets.h']).toContain('FULL 0');
    expect(io.files['.stamp']).toBeTruthy();
  });

  it('flag on runs install, build, then embeds in order', () => {
    const io = makeIo();
    const s = okSpawn();
    runPipeline({ flagOn: true, spawn: s.fn, io });
    expect(s.calls).toEqual([
      'pnpm --version',
      'pnpm --dir ui install --frozen-lockfile',
      'pnpm --dir ui run build',
    ]);
    expect(io.files['EmbeddedAssets.h']).toContain('FULL 1');
  });

  it('reports pnpm not found (BRL002)', () => {
    const io = makeIo();
    const spawn = () => ({ error: Object.assign(new Error('x'), { code: 'ENOENT' }) });
    expect(() => runPipeline({ flagOn: true, spawn, io })).toThrow(/BRL002.*pnpm not found/);
    expect(io.writes).toEqual([]);
  });

  it('non-zero build exit throws and embed is not run', () => {
    const io = makeIo();
    const spawn = (cmd, args) => ({ status: args.includes('build') ? 2 : 0 });
    expect(() => runPipeline({ flagOn: true, spawn, io })).toThrow(/BRL003/);
    expect(io.writes).toEqual([]);
  });

  it('skips everything when stamp matches and files are present', () => {
    const io = makeIo();
    const s = okSpawn();
    runPipeline({ flagOn: true, spawn: s.fn, io });
    s.calls.length = 0;
    io.writes.length = 0;
    runPipeline({ flagOn: true, spawn: s.fn, io });
    expect(s.calls).toEqual([]);
    expect(io.writes).toEqual([]);
  });

  it('does not rewrite identical content', () => {
    const io = makeIo();
    runPipeline({ flagOn: false, spawn: okSpawn().fn, io });
    delete io.files['.stamp'];
    io.writes.length = 0;
    runPipeline({ flagOn: false, spawn: okSpawn().fn, io });
    expect(io.writes).toEqual(['.stamp']);
  });
});
