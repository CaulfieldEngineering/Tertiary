// [Claude 2026-10-31] Collect the web demo into build/web-bundle: exactly the files that get deployed, plus
// build-commit.txt and build-manifest.sha256 (sha256sum format, "<hash>  <path>"). Moved from workflow.yml
// "Collect the Web Demo Files" so the desk and GitHub make the same bundle; the tag release checks it file by file.
//   node scripts/web/bundle.mjs [--web build/web] [--out build/web-bundle] [--commit <sha>]
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import { execFileSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const arg = (flag, fallback) => { const i = process.argv.indexOf(flag); return i > 0 ? process.argv[i + 1] : fallback; };
const cmake = fs.readFileSync(path.join(root, 'config.cmake'), 'utf8');
const name = cmake.match(/set\(wPLUGIN_NAME\s+"([^"]+)"\)/)[1];
const src = path.resolve(root, arg('--web', process.env.WEB_BUILD_DIR || 'build/web'));
const out = path.resolve(root, arg('--out', 'build/web-bundle'));
const commit = arg('--commit', process.env.GITHUB_SHA || execFileSync('git', ['rev-parse', 'HEAD'], { cwd: root, encoding: 'utf8' }).trim());

fs.rmSync(out, { recursive: true, force: true });
fs.mkdirSync(out, { recursive: true });
for (const f of ['index.html', 'plugin-config.js', `${name}Web.js`, `${name}Web.wasm`])
  fs.copyFileSync(path.join(src, f), path.join(out, f));
if (fs.existsSync(path.join(src, `${name}WebFallback.js`)))
  for (const f of [`${name}WebFallback.js`, `${name}WebFallback.wasm`]) fs.copyFileSync(path.join(src, f), path.join(out, f));
if (fs.existsSync(path.join(src, 'audio'))) fs.cpSync(path.join(src, 'audio'), path.join(out, 'audio'), { recursive: true });

function walk(dir, rel = '') {
  return fs.readdirSync(dir, { withFileTypes: true }).flatMap(e =>
    e.isDirectory() ? walk(path.join(dir, e.name), rel + e.name + '/') : e.isFile() ? [rel + e.name] : []);
}
// File names the checksum list and the remote commands can carry safely (no backslash, quote or newline)
const bad = walk(out).filter(f => /[\\'"\n]/.test(f));
if (bad.length) {
  console.error(`::error::Web demo file names must not contain a backslash, a quote or a newline: ${bad.join(', ')}`);
  process.exit(1);
}
fs.writeFileSync(path.join(out, 'build-commit.txt'), commit + '\n');
const files = walk(out).filter(f => f !== 'build-manifest.sha256').sort((a, b) => (a < b ? -1 : a > b ? 1 : 0));
const lines = files.map(f => crypto.createHash('sha256').update(fs.readFileSync(path.join(out, f))).digest('hex') + '  ' + f);
fs.writeFileSync(path.join(out, 'build-manifest.sha256'), lines.join('\n') + '\n');
console.log(`Web demo bundle (commit ${commit}):`);
console.log(lines.join('\n'));
