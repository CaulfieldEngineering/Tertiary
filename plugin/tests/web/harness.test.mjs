// [Codex 2026-10-30] Prove visual regressions fail and AI advice cannot silently replace the pixel gate.
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { PNG } from 'pngjs';
import { compareScreenshot } from './visual.mjs';
import { findCodex, reviewDifferences } from './ai-review.mjs';

function fixture(t) {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'wonderlab-web-test-'));
  t.after(() => {
    const resolved = path.resolve(directory);
    assert.equal(path.dirname(resolved), path.resolve(os.tmpdir()));
    assert.ok(path.basename(resolved).startsWith('wonderlab-web-test-'));
    fs.rmSync(resolved, { recursive: true, force: true });
  });
  const files = Object.fromEntries(['actual', 'reference', 'diff'].map(key => [key, path.join(directory, `${key}.png`)]));
  const write = (file, { width = 100, height = 100, changed = 0, value = 255 } = {}) => {
    const png = new PNG({ width, height });
    for (let index = 0; index < width * height; index++) {
      const offset = index * 4;
      png.data[offset] = png.data[offset + 1] = png.data[offset + 2] = index < changed ? value : 0;
      png.data[offset + 3] = 255;
    }
    fs.writeFileSync(file, PNG.sync.write(png));
  };
  write(files.actual);
  return { directory, files, write, difference: { name: 'desktop', ...files } };
}

test('a missing reference fails without silently blessing the screenshot', t => {
  const { files } = fixture(t);
  const result = compareScreenshot(files);
  assert.equal(result.pass, false);
  assert.match(result.reason, /Missing approved reference/);
  assert.equal(fs.existsSync(files.reference), false);
});

test('references are created or replaced only by explicit update', t => {
  const { files, write } = fixture(t);
  assert.equal(compareScreenshot({ ...files, update: true }).referenceCreated, true);
  write(files.actual, { changed: 1000 });
  assert.equal(compareScreenshot(files).pass, false);
  assert.equal(compareScreenshot({ ...files, update: true }).referenceUpdated, true);
  assert.equal(compareScreenshot(files).pass, true);
  assert.equal(fs.existsSync(files.diff), false);
});

test('a visible regression fails and produces a readable diff image', t => {
  const { files, write } = fixture(t);
  write(files.reference);
  write(files.actual, { changed: 1000 });
  const result = compareScreenshot(files);
  assert.equal(result.pass, false);
  assert.ok(result.changedPixels > 900);
  assert.ok(result.ratio > 0.09);
  assert.equal(PNG.sync.read(fs.readFileSync(files.diff)).width, 100);
});

test('small pixel noise stays within the configured tolerance', t => {
  const { files, write } = fixture(t);
  write(files.reference);
  write(files.actual, { changed: 5 });
  const result = compareScreenshot(files);
  assert.equal(result.pass, true);
  assert.ok(result.changedPixels > 0);
  assert.ok(result.ratio <= 0.001);
});

test('dimension changes fail even when their pixels fit the tolerance', t => {
  const { files, write } = fixture(t);
  write(files.reference, { width: 101 });
  const result = compareScreenshot({ ...files, maxDiffRatio: 1 });
  assert.equal(result.pass, false);
  assert.match(result.reason, /dimensions changed/);
  assert.equal(PNG.sync.read(fs.readFileSync(files.diff)).width, 101);
});

test('comparison rejects overwriting the approved reference with a diff', t => {
  const { files, write } = fixture(t);
  write(files.reference);
  assert.throws(() => compareScreenshot({ ...files, diff: files.reference }), /different files/);
});

test('Codex absence skips cleanly and writes a report', async t => {
  const { directory, difference } = fixture(t);
  assert.equal(findCodex({ PATH: '' }), null);
  const result = await reviewDifferences([difference], { outDir: directory, env: { PATH: '' } });
  assert.equal(result.skipped, true);
  assert.match(fs.readFileSync(result.report, 'utf8'), /codex is not available/);
});

test('CI and identical screenshots never invoke Codex', async t => {
  const { directory, difference } = fixture(t);
  const run = () => { assert.fail('Advisory review must not run'); };
  assert.equal((await reviewDifferences([difference], { outDir: directory, env: { CI: 'true' }, run })).skipped, true);
  assert.equal((await reviewDifferences([], { outDir: directory, env: {}, run })).skipped, true);
});

test('a changed screenshot passes both images to read-only Codex without a shell or model override', async t => {
  const { directory, difference, files, write } = fixture(t);
  write(files.reference);
  let calls = 0;
  const result = await reviewDifferences([difference], {
    outDir: directory, env: {}, command: { file: 'codex-test', args: [] },
    run: async (file, args, options) => {
      calls++;
      assert.equal(file, 'codex-test');
      assert.equal(args[args.indexOf('--sandbox') + 1], 'read-only');
      assert.ok(args.includes('approval_policy="never"'));
      assert.equal(args.filter(argument => argument === '--image').length, 2);
      assert.ok(args.includes(files.reference));
      assert.ok(args.includes(files.actual));
      assert.equal(args.includes('--model'), false);
      assert.equal(args.at(-1), '-');
      assert.match(options.input, /clipping.*overlapping.*wrong colours.*missing scope/);
      return { code: 0, stdout: 'Looks broken: the scope is missing.' };
    }
  });
  assert.equal(calls, 1);
  assert.equal(result.reviews[0].status, 'reviewed');
  assert.match(fs.readFileSync(result.report, 'utf8'), /Looks broken: the scope is missing/);
});

test('Codex failure remains advisory and does not claim a visual verdict', async t => {
  const { directory, difference, files, write } = fixture(t);
  write(files.reference);
  const result = await reviewDifferences([difference], {
    outDir: directory, env: {}, command: { file: 'codex-test' },
    run: async () => ({ code: 1, stdout: '' })
  });
  assert.equal(result.reviews[0].status, 'error');
  assert.match(fs.readFileSync(result.report, 'utf8'), /pixel result is unchanged/);
});
