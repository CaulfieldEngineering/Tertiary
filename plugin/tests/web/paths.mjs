// [Codex 2026-10-30] All product names come from config.cmake.
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
export const here = path.dirname(fileURLToPath(import.meta.url));
export const root = path.resolve(here, '../../..');
const cmake = fs.readFileSync(path.join(root, 'config.cmake'), 'utf8');
export function setting(key) {
  const value = cmake.match(new RegExp(`set\\(${key}\\s+"([^"]+)"\\)`))?.[1];
  if (!value) throw new Error(`Missing ${key} in config.cmake`);
  return value;
}
export const name = setting('wPLUGIN_NAME');
export const version = setting('wPROJECT_VERSION');
export const webDir = path.resolve(process.env.WEB_BUILD_DIR || path.join(root, 'build/web'));
export const outDir = path.join(here, 'out');
export const refsDir = path.join(here, 'refs');
