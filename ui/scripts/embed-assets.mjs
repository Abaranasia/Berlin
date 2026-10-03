// Pre-build entry point: node ui/scripts/embed-assets.mjs --jucer <path>
// Thin wrapper; the logic lives in embed-lib.mjs (unit-tested).
import fs from 'node:fs';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { readWebUiFlag, runPipeline } from './embed-lib.mjs';

const repoRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..');
const uiDir = path.join(repoRoot, 'ui');
const outDir = path.join(repoRoot, 'Source', 'ui', 'generated');

const fail = (code, message) => {
  console.error(`Berlin prebuild : error ${code}: ${message}`);
  process.exit(1);
};

const walk = (dir, base = dir) =>
  fs.existsSync(dir)
    ? fs.readdirSync(dir, { withFileTypes: true }).flatMap((e) => {
        const p = path.join(dir, e.name);
        return e.isDirectory() ? walk(p, base) : [{ path: path.relative(base, p).split(path.sep).join('/'), data: fs.readFileSync(p) }];
      })
    : [];

const stampInputs = () => {
  const inputs = [...walk(path.join(uiDir, 'src')), ...walk(path.join(uiDir, 'scripts'))].map((f) => ({ ...f, path: f.path }));
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
    const tmp = `${p}.tmp`;
    fs.writeFileSync(tmp, content);
    fs.renameSync(tmp, p);
    return true;
  },
};

// Fixed literal arguments only; shell:true is just for pnpm.cmd resolution on Windows.
const spawn = (cmd, args) => spawnSync(cmd, args, { cwd: repoRoot, stdio: 'inherit', shell: process.platform === 'win32' });

const jucerIdx = process.argv.indexOf('--jucer');
if (jucerIdx < 0 || !process.argv[jucerIdx + 1]) fail('BRL004', 'missing --jucer <path>');

try {
  const flagOn = readWebUiFlag(fs.readFileSync(process.argv[jucerIdx + 1], 'utf8'));
  runPipeline({ flagOn, spawn, io });
} catch (e) {
  const m = /^(BRL\d+): (.*)$/.exec(e.message);
  fail(m ? m[1] : 'BRL004', m ? m[2] : e.message);
}
