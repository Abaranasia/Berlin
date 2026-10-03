// Pre-build entry point: node ui/scripts/embed-assets.mjs --jucer <path>
// Thin wrapper; the logic lives in embed-lib.mjs (unit-tested).
import fs from 'node:fs';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { randomBytes } from 'node:crypto';
import { fileURLToPath } from 'node:url';
import { readWebUiFlag, runPipeline, withLock, renameWithRetry } from './embed-lib.mjs';

const repoRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..');
const uiDir = path.join(repoRoot, 'ui');
const outDir = path.join(repoRoot, 'Source', 'ui', 'generated');

const fail = (code, message) => {
  console.error(`Berlin prebuild : error ${code}: ${message}`);
  process.exit(1);
};

const sleep = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
const lockPath = path.join(outDir, '.lock');

const lockFs = {
  create: () => {
    fs.mkdirSync(outDir, { recursive: true });
    try {
      fs.closeSync(fs.openSync(lockPath, 'wx'));
      return true;
    } catch (e) {
      if (e.code === 'EEXIST') return false;
      throw e;
    }
  },
  mtimeMs: () => {
    try {
      return fs.statSync(lockPath).mtimeMs;
    } catch (e) {
      if (e.code === 'ENOENT') return null;
      throw e;
    }
  },
  remove: () => fs.rmSync(lockPath, { force: true }),
  sleep,
  now: () => Date.now(),
};

const walk = (dir, base = dir) =>
  fs.existsSync(dir)
    ? fs.readdirSync(dir, { withFileTypes: true }).flatMap((e) => {
        const p = path.join(dir, e.name);
        return e.isDirectory() ? walk(p, base) : [{ path: path.relative(base, p).split(path.sep).join('/'), data: fs.readFileSync(p) }];
      })
    : [];

const stampInputs = () => {
  const inputs = [...walk(path.join(uiDir, 'src')), ...walk(path.join(uiDir, 'scripts'))];
  for (const name of fs.existsSync(uiDir) ? fs.readdirSync(uiDir) : []) {
    if (/^(index\.html|package\.json|pnpm-lock\.yaml|vite\.config\.ts|tsconfig.*\.json)$/.test(name)) {
      inputs.push({ path: name, data: fs.readFileSync(path.join(uiDir, name)) });
    }
  }
  return inputs;
};

const io = {
  stampInputs,
  collectAssets: () => walk(path.join(uiDir, 'dist')),
  read: (name) => {
    const p = path.join(outDir, name);
    return fs.existsSync(p) ? fs.readFileSync(p, 'utf8') : null;
  },
  writeIfChanged: (name, content) => {
    const p = path.join(outDir, name);
    if (fs.existsSync(p) && fs.readFileSync(p, 'utf8') === content) return false;
    fs.mkdirSync(outDir, { recursive: true });
    // Unique per process so concurrent pre-builds never share a temp file.
    const tmp = `${p}.${process.pid}-${randomBytes(4).toString('hex')}.tmp`;
    fs.writeFileSync(tmp, content);
    try {
      renameWithRetry({ rename: fs.renameSync, sleep }, tmp, p);
    } catch (e) {
      fs.rmSync(tmp, { force: true });
      throw e;
    }
    return true;
  },
  withLock: (fn) => withLock(lockFs, fn),
};

// Fixed literal arguments only; shell:true is just for pnpm.cmd resolution on Windows.
const spawn = (cmd, args, opts = {}) =>
  spawnSync(cmd, args, { cwd: repoRoot, stdio: 'inherit', shell: process.platform === 'win32', timeout: opts.timeout });

const jucerIdx = process.argv.indexOf('--jucer');
if (jucerIdx < 0 || !process.argv[jucerIdx + 1]) fail('BRL004', 'missing --jucer <path>');

try {
  const flagOn = readWebUiFlag(fs.readFileSync(process.argv[jucerIdx + 1], 'utf8'));
  runPipeline({ flagOn, spawn, io });
} catch (e) {
  const m = /^(BRL\d+): (.*)$/.exec(e.message);
  fail(m ? m[1] : 'BRL004', m ? m[2] : e.message);
}
