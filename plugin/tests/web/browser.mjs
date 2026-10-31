// [Codex 2026-10-30] Real Playwright gestures; observation never sets plugin parameters.
import assert from 'node:assert/strict';
export async function snapshot(page) {
  return page.evaluate(() => JSON.parse(Module.UTF8ToString(Module._webtest_snapshot())));
}
export async function until(fn, predicate, message, timeout = 5000) {
  const end = Date.now() + timeout; let value;
  do { value = await fn(); if (predicate(value)) return value; await new Promise(r => setTimeout(r, 80)); } while (Date.now() < end);
  throw new Error(`${message}: ${JSON.stringify(value).slice(0, 700)}`);
}
export async function point(page, x, y) {
  return page.evaluate(({x,y}) => {
    const c = document.getElementById(window._juceMainCanvasId), r = c.getBoundingClientRect();
    const state = JSON.parse(Module.UTF8ToString(Module._webtest_snapshot()));
    return { x: r.left + (x + (state.originX||0)) * r.width / c.width, y: r.top + (y + (state.originY||0)) * r.height / c.height };
  }, { x, y });
}
export async function clickAt(page, x, y) { const p = await point(page, x, y); await page.mouse.click(p.x, p.y); }
export async function clickControl(page, c) { await clickAt(page, c.x + c.width / 2, c.y + c.height / 2); }
export async function dragControl(page, c, delta = -45) {
  const p = await point(page, c.x + c.width / 2, c.y + c.height / 2);
  await page.mouse.move(p.x, p.y); await page.mouse.down();
  await page.mouse.move(p.x + (c.width > c.height * 2 ? -delta : 0), p.y + (c.width > c.height * 2 ? 0 : delta), { steps: 16 });
  await page.mouse.up(); await page.mouse.move(1, 1);
}
export function controlChanged(before, after) {
  return !!after && (before.value !== after.value || before.text !== after.text);
}
export async function selectItem(page, combo, text) {
  await clickControl(page, combo);
  const menu = await until(() => snapshot(page), s => s.controls.some(c => c.key.startsWith('popup/') && c.visible), 'Menu did not open');
  const item = menu.controls.find(c => c.key.startsWith('popup/') && c.visible && (c.title === text || c.text === text));
  if (!item) throw new Error(`Menu item ${text} missing: ${JSON.stringify(menu.controls.filter(c=>c.key.startsWith('popup/')))}`);
  await page.waitForTimeout(220);
  await clickControl(page, item);
  return until(() => snapshot(page), s => s.controls.find(c => c.key === combo.key)?.text === text, `Selection did not become ${text}`);
}
export function parameterChanges(a, b) {
  return b.parameters.filter((p, i) => Math.abs(p.value - a.parameters[i].value) > 1e-6).map(p => p.name);
}
export async function audioSummary(page, ms = 1800) {
  return page.evaluate(async ms => {
    const analyser = audioCtx.createAnalyser(); analyser.fftSize = 2048;
    masterGain.connect(analyser);
    const samples = new Float32Array(analyser.fftSize), levels = [];
    const end = performance.now() + ms;
    while (performance.now() < end) {
      analyser.getFloatTimeDomainData(samples);
      levels.push(Math.sqrt(samples.reduce((sum, x) => sum + x*x, 0) / samples.length));
      await new Promise(r => setTimeout(r, 20));
    }
    masterGain.disconnect(analyser); analyser.disconnect();
    const mean = levels.reduce((a,b)=>a+b,0)/levels.length;
    return { state: audioCtx.state, mean, min: Math.min(...levels), max: Math.max(...levels),
      variation: Math.sqrt(levels.reduce((s,v)=>s+(v-mean)**2,0)/levels.length), samples: levels.length };
  }, ms);
}
// Use the shipping JUCE audio entry points with the live AudioContext suspended, avoiding concurrent DSP calls.
// Deterministic multitone input exercises all three bands. No parameters are written here.
export async function renderAudio(page) {
  return page.evaluate(async () => {
    if (audioCtx?.state === 'running') await audioCtx.suspend();
    const n = 128, rate = 44100, blocks = 1034;
    Module._juce_prepareAudio(rate, n);
    const ptr = Array.from({length:4}, () => Module._malloc(n*4));
    let energy = 0, count = 0; const envelope = [];
    try {
      for (let block = 0; block < blocks; block++) {
        let heap = heapF32();
        for (let i=0;i<n;i++) {
          const t = (block*n+i)/rate;
          const v = .17*(Math.sin(2*Math.PI*110*t)+Math.sin(2*Math.PI*997*t)+Math.sin(2*Math.PI*6007*t));
          heap[(ptr[0]>>2)+i] = v; heap[(ptr[1]>>2)+i] = v;
        }
        Module._juce_processAudio(...ptr,n); heap = heapF32();
        let power=0;
        for(let i=0;i<n;i++){ const v=heap[(ptr[2]>>2)+i]; power += v*v; }
        if(block>100) { energy += power; count+=n; envelope.push(Math.sqrt(power/n)); }
      }
    } finally { ptr.forEach(p=>Module._free(p)); }
    return { rms: Math.sqrt(energy/count), envelope };
  });
}
export function soundDifference(a, b) {
  const rmse = Math.sqrt(a.envelope.reduce((s,x,i)=>s+(x-b.envelope[i])**2,0)/a.envelope.length);
  return rmse / Math.max(a.rms, b.rms, 1e-9);
}
export function toneWav(seconds = 6) {
  const rate = 44100, frames = rate*seconds, b = Buffer.alloc(44+frames*4);
  b.write('RIFF'); b.writeUInt32LE(b.length-8,4); b.write('WAVEfmt ',8); b.writeUInt32LE(16,16);
  b.writeUInt16LE(1,20); b.writeUInt16LE(2,22); b.writeUInt32LE(rate,24); b.writeUInt32LE(rate*4,28);
  b.writeUInt16LE(4,32); b.writeUInt16LE(16,34); b.write('data',36); b.writeUInt32LE(frames*4,40);
  for(let i=0;i<frames;i++){ const t=i/rate, v=.17*(Math.sin(2*Math.PI*110*t)+Math.sin(2*Math.PI*997*t)+Math.sin(2*Math.PI*6007*t));
    b.writeInt16LE(Math.round(v*32767),44+i*4); b.writeInt16LE(Math.round(v*32767),46+i*4); }
  return b;
}
export async function start(page) {
  await page.locator('#big-play').click();
  await page.waitForFunction(() => typeof audioCtx !== 'undefined' && audioCtx?.state === 'running' && juceReady && playing, null, { timeout: 20000 });
}
