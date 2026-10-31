// [Codex 2026-10-30] Assemble with Wonderlab, then apply the repository's web-only fixes.
// Used for shipping builds too; no test inspector or test-only behaviour here.
import fs from 'node:fs';
import path from 'node:path';
import { execFileSync } from 'node:child_process';
import { patchShell } from './patch-shell.mjs';

const args = process.argv.slice(2);
const value = key => args[args.indexOf(key) + 1];
if (!args.includes('--wonderlab') || !args.includes('--build'))
  throw new Error('Usage: assemble.mjs --wonderlab <checkout> --build <output> [Wonderlab assembly options]');
const wonderlab = path.resolve(value('--wonderlab'));
execFileSync(process.execPath, [path.join(wonderlab, 'scripts/assemble-web.js'), ...args], {stdio: 'inherit'});
const html = path.resolve(value('--build'), 'index.html');
fs.writeFileSync(html, patchShell(fs.readFileSync(html, 'utf8')));
