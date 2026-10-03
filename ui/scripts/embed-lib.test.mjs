import { describe, it, expect } from 'vitest';
import { readWebUiFlag, renderAssetSources, computeStamp, runPipeline, withLock, renameWithRetry, PNPM_TIMEOUT_MS } from './embed-lib.mjs';

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
  it('header does not redeclare getEmbeddedAssets (WebAssets.h owns it) and documents FULL', () => {
    const { header } = renderAssetSources({ mode: 'stub', assets: [] });
    expect(header).not.toContain('getEmbeddedAssets');
    expect(header).toMatch(/#define BERLIN_EMBEDDED_ASSETS_FULL [01]/);
    expect(header).toMatch(/\/\/.*static_assert/);
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
    const optsSeen = [];
    const fn = (cmd, args, opts) => {
      calls.push([cmd, ...args].join(' '));
      optsSeen.push(opts);
      return { status: 0 };
    };
    return { fn, calls, optsSeen };
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

describe('runPipeline pnpm timeout (BRL006)', () => {
  const io = () => ({
    stampInputs: () => [],
    collectAssets: () => [],
    read: () => null,
    writeIfChanged: () => true,
  });

  it('passes a positive timeout to every pnpm spawn', () => {
    const seen = [];
    const spawn = (cmd, args, opts) => (seen.push(opts), { status: 0 });
    runPipeline({ flagOn: true, spawn, io: io() });
    expect(seen).toHaveLength(3);
    for (const o of seen) expect(o.timeout).toBe(PNPM_TIMEOUT_MS);
    expect(PNPM_TIMEOUT_MS).toBeGreaterThan(0);
  });

  it('reports a clear BRL006 error when pnpm times out', () => {
    const spawn = (cmd, args) =>
      args.includes('build') ? { error: Object.assign(new Error('spawnSync ETIMEDOUT'), { code: 'ETIMEDOUT' }) } : { status: 0 };
    expect(() => runPipeline({ flagOn: true, spawn, io: io() })).toThrow(/BRL006.*pnpm.*timed out/);
  });

  it('a timed-out install does not write any output', () => {
    const e = io();
    const writes = [];
    e.writeIfChanged = (n) => writes.push(n);
    const spawn = (cmd, args) =>
      args.includes('install') ? { error: Object.assign(new Error('t'), { code: 'ETIMEDOUT' }) } : { status: 0 };
    expect(() => runPipeline({ flagOn: true, spawn, io: e })).toThrow(/BRL006/);
    expect(writes).toEqual([]);
  });
});

describe('withLock', () => {
  const makeFs = ({ createResults = [true], mtime = null } = {}) => {
    let t = 1_000_000;
    const log = [];
    const queue = [...createResults];
    return {
      log,
      create: () => (log.push('create'), queue.length > 1 ? queue.shift() : queue[0]),
      mtimeMs: () => mtime,
      remove: () => log.push('remove'),
      sleep: (ms) => (log.push('sleep'), (t += ms)),
      now: () => t,
    };
  };
  const opts = { staleMs: 1000, timeoutMs: 5000, pollMs: 100 };

  it('runs fn under the lock and releases it', () => {
    const fs = makeFs();
    expect(withLock(fs, () => 42, opts)).toBe(42);
    expect(fs.log).toEqual(['create', 'remove']);
  });

  it('releases the lock when fn throws', () => {
    const fs = makeFs();
    expect(() => withLock(fs, () => { throw new Error('boom'); }, opts)).toThrow('boom');
    expect(fs.log).toEqual(['create', 'remove']);
  });

  it('waits while another process holds a fresh lock', () => {
    const fs = makeFs({ createResults: [false, false, true], mtime: 1_000_000 });
    withLock(fs, () => 1, opts);
    expect(fs.log).toEqual(['create', 'sleep', 'create', 'sleep', 'create', 'remove']);
  });

  it('breaks a stale lock and then acquires it', () => {
    const fs = makeFs({ createResults: [false, true], mtime: 1_000_000 - 2000 });
    withLock(fs, () => 1, opts);
    expect(fs.log).toEqual(['create', 'remove', 'create', 'remove']);
  });

  it('retries when the lock vanishes between create and stat', () => {
    const fs = makeFs({ createResults: [false, true], mtime: null });
    withLock(fs, () => 1, opts);
    expect(fs.log).toEqual(['create', 'create', 'remove']);
  });

  it('throws BRL005 on timeout without releasing a lock it never owned', () => {
    const fs = makeFs({ createResults: [false], mtime: 1_000_000 + 10_000_000 });
    expect(() => withLock(fs, () => 1, opts)).toThrow(/BRL005.*lock/);
    expect(fs.log).not.toContain('remove');
  });
});

describe('runPipeline concurrency', () => {
  const baseIo = () => {
    const files = {};
    return {
      files,
      stampInputs: () => [{ path: 'package.json', data: Buffer.from('p') }],
      collectAssets: () => [],
      read: (n) => (n in files ? files[n] : null),
      writeIfChanged: (n, c) => ((files[n] = c), true),
    };
  };

  it('re-checks the stamp inside the lock: a peer that finished first means no work', () => {
    const peer = baseIo();
    runPipeline({ flagOn: false, spawn: () => ({ status: 0 }), io: peer });

    const io = baseIo();
    io.withLock = (fn) => {
      Object.assign(io.files, peer.files); // another project generated while we waited
      return fn();
    };
    const writes = [];
    const real = io.writeIfChanged;
    io.writeIfChanged = (n, c) => (writes.push(n), real(n, c));
    const result = runPipeline({ flagOn: false, spawn: () => ({ status: 0 }), io });
    expect(result).toEqual({ skipped: true });
    expect(writes).toEqual([]);
  });

  it('does not take the lock when the quick stamp check already matches', () => {
    const io = baseIo();
    runPipeline({ flagOn: false, spawn: () => ({ status: 0 }), io });
    let locked = 0;
    io.withLock = (fn) => (locked++, fn());
    runPipeline({ flagOn: false, spawn: () => ({ status: 0 }), io });
    expect(locked).toBe(0);
  });

  it('takes the lock exactly once when generating', () => {
    const io = baseIo();
    let locked = 0;
    io.withLock = (fn) => (locked++, fn());
    runPipeline({ flagOn: false, spawn: () => ({ status: 0 }), io });
    expect(locked).toBe(1);
  });
});

describe('renameWithRetry', () => {
  const err = (code) => Object.assign(new Error(code), { code });

  it.each(['EPERM', 'EBUSY', 'ENOENT'])('retries %s then succeeds', (code) => {
    let n = 0;
    const sleeps = [];
    renameWithRetry({ rename: () => { if (n++ < 2) throw err(code); }, sleep: (ms) => sleeps.push(ms) }, 'a', 'b', 5);
    expect(n).toBe(3);
    expect(sleeps).toHaveLength(2);
  });

  it('gives up after the attempt budget and rethrows', () => {
    let n = 0;
    expect(() => renameWithRetry({ rename: () => { n++; throw err('EPERM'); }, sleep: () => {} }, 'a', 'b', 4)).toThrow('EPERM');
    expect(n).toBe(4);
  });

  it('does not retry other errors', () => {
    let n = 0;
    expect(() => renameWithRetry({ rename: () => { n++; throw err('EACCES'); }, sleep: () => {} }, 'a', 'b', 5)).toThrow('EACCES');
    expect(n).toBe(1);
  });
});
