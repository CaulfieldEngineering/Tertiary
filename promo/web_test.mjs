// Loads the local copy of the web demo in headless Chrome and plays every track through the Tertiary WASM build.
import { spawn } from 'node:child_process';
const port = 9333, url = process.argv[2];
const chrome = spawn('C:/Program Files/Google/Chrome/Application/chrome.exe', ['--headless=new', `--remote-debugging-port=${port}`,
  '--autoplay-policy=no-user-gesture-required', '--user-data-dir=' + process.argv[3], 'about:blank'], { stdio: 'ignore' });
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
let targets;
for (let i = 0; i < 50; i++) { try { targets = await (await fetch(`http://127.0.0.1:${port}/json`)).json(); break; } catch { await sleep(200); } }
const ws = new WebSocket(targets.find((t) => t.type === 'page').webSocketDebuggerUrl);
await new Promise((r) => ws.addEventListener('open', r));
let id = 0; const pending = new Map(); const logs = [];
ws.addEventListener('message', (e) => { const m = JSON.parse(e.data);
  if (m.id && pending.has(m.id)) { pending.get(m.id)(m); pending.delete(m.id); }
  if (m.method === 'Runtime.consoleAPICalled') logs.push(m.params.type + ': ' + m.params.args.map((a) => a.value ?? a.description).join(' '));
  if (m.method === 'Runtime.exceptionThrown') logs.push('EXCEPTION: ' + m.params.exceptionDetails.text + ' ' + (m.params.exceptionDetails.exception?.description || ''));
});
const send = (method, params = {}) => new Promise((r) => { const i = ++id; pending.set(i, r); ws.send(JSON.stringify({ id: i, method, params })); });
const ev = async (expr) => (await send('Runtime.evaluate', { expression: expr, awaitPromise: true, returnByValue: true })).result?.result?.value;
await send('Runtime.enable'); await send('Page.enable');
await send('Page.navigate', { url });
await sleep(8000);
console.log('tracks in config:', await ev('JSON.stringify(TRACKS.map(t => t.title))'));
const n = await ev('TRACKS.length');
for (let i = 0; i < n; i++) {
  await ev(`selectTrack(${i})`);
  await sleep(2500);
  const r = await ev(`JSON.stringify({ title: TRACKS[${i}].title, shown: trackName.textContent, dur: audioBuffer && +audioBuffer.duration.toFixed(2), ch: audioBuffer && audioBuffer.numberOfChannels, rate: audioCtx.sampleRate, playing, ctx: audioCtx.state, worklet: typeof processorNode !== 'undefined' && !!processorNode })`);
  // jump to 1 s before the end and check the player wraps round instead of stopping
  await ev(`play(audioBuffer.duration - 1)`);
  await sleep(2500);
  const after = await ev(`JSON.stringify({ playing, looping, sourceLoop: !!(sourceNode && sourceNode.loop) })`);
  console.log(r, after);
}
console.log('console:', logs.filter((l) => /error|exception|fail/i.test(l)).slice(0, 10).join('\n  ') || 'no errors');
console.log('engine:', await ev(`(document.querySelectorAll('#juce-container canvas').length) + ' JUCE canvas(es); status ' + statusEl.textContent`));
ws.close(); chrome.kill();
process.exit(0);
