// [Codex 2026-10-30] Loopback-only server, including the SharedArrayBuffer headers used by Wonderlab.
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import { pathToFileURL } from 'node:url';
import { webDir } from './paths.mjs';
const mime = { '.html': 'text/html', '.js': 'text/javascript', '.wasm': 'application/wasm', '.json': 'application/json',
  '.wav': 'audio/wav', '.mp3': 'audio/mpeg', '.png': 'image/png', '.woff2': 'font/woff2', '.css': 'text/css' };
export async function serve({ dir = webDir, port = 0 } = {}) {
  const base = path.resolve(dir);
  const server = http.createServer((req, res) => {
    res.setHeader('Cross-Origin-Opener-Policy', 'same-origin');
    res.setHeader('Cross-Origin-Embedder-Policy', 'require-corp');
    res.setHeader('Cross-Origin-Resource-Policy', 'same-origin');
    res.setHeader('Cache-Control', 'no-store');
    try {
      const url = new URL(req.url, 'http://localhost');
      if (url.pathname === '/favicon.ico') { res.writeHead(204); res.end(); return; }
      const file = path.resolve(base, '.' + decodeURIComponent(url.pathname === '/' ? '/index.html' : url.pathname));
      const relative = path.relative(base, file);
      if (relative.startsWith('..') || path.isAbsolute(relative) || !mime[path.extname(file)]) {
        res.writeHead(403); res.end('Forbidden'); return;
      }
      const stat = fs.statSync(file);
      if (!stat.isFile()) throw new Error('Not a file');
      res.writeHead(200, { 'Content-Type': mime[path.extname(file)], 'Content-Length': stat.size });
      if (req.method === 'HEAD') res.end(); else fs.createReadStream(file).pipe(res);
    } catch { res.writeHead(404); res.end('Not found'); }
  });
  await new Promise((resolve, reject) => { server.once('error', reject); server.listen(port, '127.0.0.1', resolve); });
  return { url: `http://127.0.0.1:${server.address().port}`, close: () => new Promise(r => server.close(r)) };
}
if (process.argv[1] && import.meta.url === pathToFileURL(path.resolve(process.argv[1])).href) {
  const server = await serve({ port: Number(process.env.PORT || 4173) });
  console.log(`Wonderlab: ${server.url}`);
}
