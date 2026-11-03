// [Codex 2026-10-30] Pixel regressions fail independently of the advisory AI review.
import fs from 'node:fs';
import path from 'node:path';
import { PNG } from 'pngjs';
import pixelmatch from 'pixelmatch';

export function compareScreenshot({ actual, reference, diff, update = false, threshold = 0.12, maxDiffRatio = 0.001 }) {
  if (!actual || !reference || !diff) throw new Error('actual, reference and diff paths are required');
  if (new Set([actual, reference, diff].map(value => path.resolve(value))).size !== 3)
    throw new Error('actual, reference and diff must be different files');
  if (![threshold, maxDiffRatio].every(value => Number.isFinite(value) && value >= 0 && value <= 1))
    throw new Error('Pixel threshold and maximum difference ratio must be between zero and one');
  const current = PNG.sync.read(fs.readFileSync(actual));
  const exists = fs.existsSync(reference);
  const result = { actual, reference, diff, pass: false, changedPixels: 0, ratio: 0, referenceCreated: false };
  if (update) {
    fs.mkdirSync(path.dirname(reference), { recursive: true });
    fs.copyFileSync(actual, reference);
    fs.rmSync(diff, { force: true });
    return { ...result, pass: true, referenceCreated: !exists, referenceUpdated: exists };
  }
  if (!exists) return { ...result, reason: 'Missing approved reference; use --update-refs only after reviewing this build' };

  const approved = PNG.sync.read(fs.readFileSync(reference));
  const sameSize = current.width === approved.width && current.height === approved.height;
  const width = Math.max(current.width, approved.width);
  const height = Math.max(current.height, approved.height);
  // Pad unequal images so a size regression still has a useful diff artifact.
  const pad = source => {
    if (source.width === width && source.height === height) return source;
    const padded = new PNG({ width, height });
    PNG.bitblt(source, padded, 0, 0, source.width, source.height, 0, 0);
    return padded;
  };
  const difference = new PNG({ width, height });
  const changedPixels = pixelmatch(pad(approved).data, pad(current).data, difference.data, width, height, { threshold });
  const ratio = changedPixels / (width * height);
  const pass = sameSize && ratio <= maxDiffRatio;
  if (changedPixels || !sameSize) {
    fs.mkdirSync(path.dirname(diff), { recursive: true });
    fs.writeFileSync(diff, PNG.sync.write(difference));
  } else {
    fs.rmSync(diff, { force: true });
  }
  return {
    ...result, pass, changedPixels, ratio,
    actualSize: { width: current.width, height: current.height },
    referenceSize: { width: approved.width, height: approved.height },
    reason: !sameSize ? 'Screenshot dimensions changed' : !pass ? 'Pixel difference exceeds tolerance' : undefined
  };
}
