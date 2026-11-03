// [Codex 2026-10-30] Optional end-to-end AI harness check using a deliberately damaged screenshot copy.
// This never updates refs and must never be counted as a defect in the browser demo.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { PNG } from 'pngjs';
import { outDir } from './paths.mjs';
import config from './config.mjs';
import { compareScreenshot } from './visual.mjs';
import { reviewDifferences } from './ai-review.mjs';

const source = path.resolve(process.argv[2] || path.join(outDir, 'desktop-baseline.png'));
if (!fs.existsSync(source)) throw new Error(`Run the browser suite first; missing screenshot: ${source}`);
const directory = path.join(outDir, 'ai-selftest');
fs.mkdirSync(directory, { recursive: true });
const reference = path.join(directory, 'reference.png');
const actual = path.join(directory, 'actual.png');
const diff = path.join(directory, 'diff.png');
fs.copyFileSync(source, reference);
const png = PNG.sync.read(fs.readFileSync(reference));
// Obscure the upper-left editor region (the scope in Tertiary) with an unmistakable test patch.
const rectangle = {
  x: Math.round(png.width * .245), y: Math.round(png.height * .075),
  width: Math.round(png.width * .25), height: Math.round(png.height * .20)
};
function pixel(x, y, colour) {
  if (x < 0 || y < 0 || x >= png.width || y >= png.height) return;
  const offset = (y * png.width + x) * 4;
  png.data.set([...colour, 255], offset);
}
for (let y = rectangle.y; y < rectangle.y + rectangle.height; y++)
  for (let x = rectangle.x; x < rectangle.x + rectangle.width; x++) pixel(x, y, [210, 0, 100]);
// A small built-in bitmap label keeps this diagnostic image unmistakable when viewed alone.
const glyphs = {
  S: ['11111','10000','10000','11111','00001','00001','11111'],
  Y: ['10001','10001','01010','00100','00100','00100','00100'],
  N: ['10001','11001','11001','10101','10011','10011','10001'],
  T: ['11111','00100','00100','00100','00100','00100','00100'],
  H: ['10001','10001','10001','11111','10001','10001','10001'],
  E: ['11111','10000','10000','11110','10000','10000','11111'],
  I: ['11111','00100','00100','00100','00100','00100','11111'],
  C: ['11111','10000','10000','10000','10000','10000','11111'],
  ' ': ['00000','00000','00000','00000','00000','00000','00000']
};
const label = 'SYNTHETIC TEST';
const scale = Math.max(1, Math.min(4, Math.floor((rectangle.width - 12) / (label.length * 6))));
const left = rectangle.x + Math.floor((rectangle.width - label.length * 6 * scale) / 2);
const top = rectangle.y + Math.floor((rectangle.height - 7 * scale) / 2);
for (let letter = 0; letter < label.length; letter++) {
  const glyph = glyphs[label[letter]];
  for (let y = 0; y < 7; y++) for (let x = 0; x < 5; x++) if (glyph[y][x] === '1')
    for (let sy = 0; sy < scale; sy++) for (let sx = 0; sx < scale; sx++)
      pixel(left + (letter * 6 + x) * scale + sx, top + y * scale + sy, [255, 255, 255]);
}
fs.writeFileSync(actual, PNG.sync.write(png));
const comparison = compareScreenshot({ actual, reference, diff });
assert.equal(comparison.pass, false, 'Synthetic UI obstruction must fail pixel comparison');
assert.ok(comparison.changedPixels > 0 && fs.existsSync(diff), 'Synthetic failure must produce a diff PNG');
const difference = { name: `HARNESS SELF-TEST: synthetic ${config.scope ? 'missing-scope' : 'obscured-editor'} patch, not a product bug`, actual, reference, diff };
fs.writeFileSync(path.join(directory, 'differences.json'), JSON.stringify([difference], null, 2));
console.log(`Synthetic pixel regression detected: ${comparison.changedPixels} pixels (${(comparison.ratio * 100).toFixed(2)}%).`);
const review = await reviewDifferences([difference], { outDir: directory });
const result = {
  kind: 'synthetic-harness-self-test', productBug: false, source, rectangle, comparison, review
};
fs.writeFileSync(path.join(directory, 'result.json'), JSON.stringify(result, null, 2));
console.log(review.reason || `Advisory status: ${review.reviews[0]?.status}. Verdict: ${review.report}`);
// AI is still advisory: unavailable Codex does not turn this optional check into a CI requirement.
if (review.reviews[0]?.verdict) console.log(review.reviews[0].verdict);
