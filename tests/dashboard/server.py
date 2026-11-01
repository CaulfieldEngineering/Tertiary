#!/usr/bin/env python3
"""[Claude 2026-10-31] Live test dashboard: one page for the whole test run, local machines and GitHub.

Joe: watch the whole testing sequence live, local and GitHub, "reports and screenshots and statuses print
here", and the final report should be the same thing he watches. It reads only what the runners already
write: each machine's tests/results/<run>/progress.jsonl (format v1, tests/README.md), its <ID>.json results
and their artifacts. The Mac is read over SSH (the tailnet is private). GitHub jobs come from the API with
the token from `git credential`, kept in this process and never sent to the page.

  python tests/dashboard/server.py                    serve on http://127.0.0.1:4120 (newest run per machine)
  python tests/dashboard/server.py --host 100.x.y.z    also reachable on this tailnet address (never 0.0.0.0)
  python tests/dashboard/server.py --run win=tests/results/2026-10-31_1400 --run mac=2026-10-31_1402
  python tests/dashboard/server.py --export tests/reports/rc-1.1.0.html --run win=... --run mac=... --github-run ID

Machines come from config.cmake: wTEST_DASHBOARD_MAC (ssh user@host) and wTEST_DASHBOARD_MAC_REPO (its
checkout). This machine is always included. No new dependencies; Pillow is used for export thumbnails if present.
"""
import argparse
import base64
import datetime
import html
import http.server
import io
import ipaddress
import json
import mimetypes
import os
from pathlib import Path, PurePosixPath
import subprocess
import sys
import threading
import time
import urllib.parse
import urllib.request

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
sys.path.insert(0, str(ROOT / 'tests' / 'lib'))
from common import PLATFORM, config  # noqa: E402

IMAGE = ('.png', '.jpg', '.jpeg')
TEXT = ('.log', '.txt', '.json', '.md')


class Source:
    """One machine's results folder: this checkout, or another machine's checkout over SSH."""

    def __init__(self, name, ssh=None, repo=None, pinned=None):
        self.name, self.ssh, self.repo, self.pinned = name, ssh, repo, pinned
        self.base = (PurePosixPath(repo) / 'tests/results') if ssh else ROOT / 'tests/results'
        self.cache, self.error = {}, None

    def _ssh(self, command, timeout=15):
        p = subprocess.run(['ssh', '-o', 'BatchMode=yes', '-o', 'ConnectTimeout=6', self.ssh, command],
                           capture_output=True, timeout=timeout)
        if p.returncode:
            raise OSError(p.stderr.decode(errors='replace').strip() or f'ssh exit {p.returncode}')
        return p.stdout

    @staticmethod
    def remote(path):
        """A double-quoted remote path; a leading ~ becomes $HOME. Characters the shell would expand are refused."""
        if any(c in path for c in ('"', '$', '`', '\\', '\n')):
            raise PermissionError('unsupported character in path')
        return '"' + ('$HOME/' + path[2:] if path.startswith('~/') else path) + '"'

    def run_dir(self):
        if self.pinned:
            return self.pinned
        if self.ssh:
            line = self._ssh(f'cd {self.remote(str(self.base))} 2>/dev/null && ls -t */progress.jsonl 2>/dev/null | head -1').decode().strip()
            return line.split('/')[0] if line else None
        runs = sorted(self.base.glob('*/progress.jsonl'), key=lambda p: p.stat().st_mtime, reverse=True)
        return runs[0].parent.name if runs else None

    def read(self, relative, limit=None):
        """Bytes of a file inside this machine's run folder. Paths are checked; nothing outside is read."""
        run = self.run_dir()
        rel = PurePosixPath(str(relative).replace('\\', '/'))
        if not run or rel.is_absolute() or '..' in rel.parts:
            raise PermissionError('path outside the run folder')
        if self.ssh:
            quoted = self.remote(f'{self.base}/{run}/{rel}')
            cmd = f'tail -c {int(limit)} {quoted}' if limit else f'cat {quoted}'
            return self._ssh(cmd, timeout=30)
        path = (self.base / run / rel).resolve()
        if (self.base / run).resolve() not in path.parents:
            raise PermissionError('path outside the run folder')
        data = path.read_bytes()
        return data[-limit:] if limit else data

    def state(self):
        # Cached: 2 s normally; 30 s after an error, so an unreachable machine never stalls the page.
        hit = self.cache.get('state')
        if hit and time.time() - hit[0] < (30 if hit[1]['error'] else 2):
            return hit[1]
        result = self._state()
        self.cache['state'] = (time.time(), result)
        return result

    def _state(self):
        try:
            run = self.run_dir()
            events = [json.loads(x) for x in self.read('progress.jsonl').decode('utf-8', 'replace').splitlines() if x.strip()] if run else []
            self.error = None
        except Exception as error:
            run, events, self.error = None, [], str(error)
        return dict(name=self.name, run=run, events=events, error=self.error, remote=bool(self.ssh))


def github_token():
    try:
        out = subprocess.run(['git', 'credential', 'fill'], input='protocol=https\nhost=github.com\n\n',
                             capture_output=True, text=True, timeout=10).stdout
        return next((line[9:] for line in out.splitlines() if line.startswith('password=')), None)
    except Exception:
        return None


def github_repo():
    import re
    url = subprocess.run(['git', '-C', str(ROOT), 'remote', 'get-url', 'origin'], capture_output=True, text=True).stdout.strip()
    m = re.search(r'github\.com[:/]([^/]+/[^/]+?)(?:\.git)?$', url)
    return m.group(1) if m else None


class GitHub:
    def __init__(self, run_id=None):
        self.run_id, self.token, self.repo = run_id, github_token(), github_repo()
        self.cache, self.lock = {}, threading.Lock()

    def get(self, url):
        request = urllib.request.Request(url, headers={'Accept': 'application/vnd.github+json'})
        if self.token:
            request.add_header('Authorization', 'Bearer ' + self.token)
        with urllib.request.urlopen(request, timeout=20) as r:
            return json.load(r)

    def state(self, commit):
        """Runs and jobs for this commit (or the pinned run). Cached 20 s; the token never leaves here."""
        key = self.run_id or commit
        with self.lock:
            hit = self.cache.get(key)
            if hit and time.time() - hit[0] < 20:
                return hit[1]
        result = dict(runs=[], error=None)
        try:
            api = f'https://api.github.com/repos/{self.repo}/actions/runs'
            runs = [self.get(f'{api}/{self.run_id}')] if self.run_id else \
                self.get(f'{api}?head_sha={commit}&per_page=10').get('workflow_runs', []) if commit else []
            for run in runs:
                jobs = self.get(f"{api}/{run['id']}/jobs?per_page=100").get('jobs', [])
                result['runs'].append(dict(id=run['id'], name=run.get('name'), url=run.get('html_url'),
                    status=run.get('status'), conclusion=run.get('conclusion'), sha=run.get('head_sha'),
                    started=run.get('run_started_at') or run.get('created_at'), updated=run.get('updated_at'),
                    jobs=[dict(name=j['name'], status=j.get('status'), conclusion=j.get('conclusion'),
                               started=j.get('started_at'), completed=j.get('completed_at'), url=j.get('html_url')) for j in jobs]))
        except Exception as error:
            result['error'] = str(error)
        with self.lock:
            self.cache[key] = (time.time(), result)
        return result


def detail(source, test_id):
    """A test's checks, log tails and image list, from its <ID>.json."""
    data = json.loads(source.read(test_id + '.json'))
    artifacts = [a.replace('\\', '/') for a in data.get('artifacts', [])]
    logs = []
    for a in artifacts:
        if a.endswith('.log') and len(logs) < 6:
            try:
                logs.append(dict(path=a, tail=source.read(a, limit=12000).decode('utf-8', 'replace')))
            except Exception as error:
                logs.append(dict(path=a, tail='(could not read: %s)' % error))
    return dict(id=data.get('id'), title=data.get('title'), status=data.get('status'), build=data.get('build'),
                host=data.get('host'), checks=data.get('checks', []), logs=logs,
                images=[a for a in artifacts if a.lower().endswith(IMAGE)],
                files=[a for a in artifacts if a.lower().endswith(TEXT)])


def sources_from(args):
    cfg = config()
    pinned = dict(x.split('=', 1) for x in args.run or [])
    local = 'win' if PLATFORM == 'win' else 'mac'
    sources = [Source(local, pinned=Path(pinned[local]).name if local in pinned else None)]
    if local in pinned and Path(pinned[local]).is_absolute():
        sources[0].base = Path(pinned[local]).parent
    mac, repo = cfg.get('wTEST_DASHBOARD_MAC'), cfg.get('wTEST_DASHBOARD_MAC_REPO')
    if local == 'win' and mac and repo:
        sources.append(Source('mac', ssh=mac, repo=repo, pinned=Path(pinned['mac']).name if 'mac' in pinned else None))
    for name, folder in pinned.items():   # any other pinned local folder (for example a Mac run copied here)
        if name not in [s.name for s in sources]:
            s = Source(name, pinned=Path(folder).name)
            s.base = Path(folder).resolve().parent
            sources.append(s)
        elif name == 'mac' and Path(folder).exists():   # a copied Mac run beats SSH
            s = next(x for x in sources if x.name == 'mac')
            s.ssh, s.base, s.pinned = None, Path(folder).resolve().parent, Path(folder).name
    return sources


def snapshot(sources, github):
    machines = [s.state() for s in sources]
    commits = {e.get('commit') for m in machines for e in m['events'] if e.get('event') == 'run-start'}
    commit = next(iter(commits)) if len(commits) == 1 else None
    # [Claude 2026-10-31] The plan: tests/sequence.json, so the page lists every test in run order before it starts.
    try:
        rows = json.loads((ROOT / 'tests' / 'sequence.json').read_text(encoding='utf-8'))
        sequence = [dict(id=r['id'], name=r.get('name', r['id']), scope=r.get('scope'), suite_ids=r.get('suite_ids', []),
                         scripts=sorted(r.get('scripts', {})), ci_ok=r.get('ci_ok', False),
                         requires_format=r.get('requires_format')) for r in rows]
    except Exception:
        sequence = []
    cfg = config()
    return dict(product=cfg['wPLUGIN_NAME'], time=datetime.datetime.now().astimezone().isoformat(timespec='seconds'), machines=machines,
                commit=commit, commits=sorted(c for c in commits if c), github=github.state(commit),
                sequence=sequence, formats=cfg.get('wFORMATS', ''))


def thumbnail(data, name):
    """Images for the saved page: JPEG at most 1600 px wide when Pillow is present, else the original."""
    try:
        from PIL import Image
        image = Image.open(io.BytesIO(data))
        image.thumbnail((1600, 1600))
        out = io.BytesIO()
        image.convert('RGB').save(out, 'JPEG', quality=82)
        return 'data:image/jpeg;base64,' + base64.b64encode(out.getvalue()).decode()
    except Exception:
        kind = mimetypes.guess_type(name)[0] or 'image/png'
        return f'data:{kind};base64,' + base64.b64encode(data).decode()


def export(sources, github, target):
    """The same view, frozen: every test's detail and its screenshots embedded, so the page stands alone."""
    state = snapshot(sources, github)
    details = {}
    for source, machine in zip(sources, state['machines']):
        ids = {e['id'] for e in machine['events'] if e.get('event') == 'test-finish'}
        for test_id in sorted(ids):
            try:
                d = detail(source, test_id)
                d['images'] = [dict(path=p, src=thumbnail(source.read(p), p)) for p in d['images']]
            except Exception as error:
                d = dict(id=test_id, checks=[], logs=[], images=[], error=str(error))
            details[machine['name'] + '/' + test_id] = d
    state['details'] = details
    page = (HERE / 'index.html').read_text(encoding='utf-8')
    blob = json.dumps(state, ensure_ascii=False).replace('</', '<\\/')
    page = page.replace('<script id="snapshot"></script>', '<script id="snapshot">window.SNAPSHOT=' + blob + ';</script>', 1)
    Path(target).parent.mkdir(parents=True, exist_ok=True)
    Path(target).write_text(page, encoding='utf-8')
    return target


class Handler(http.server.BaseHTTPRequestHandler):
    sources, github = [], None

    def log_message(self, *a):
        pass

    def send(self, code, body, kind='application/json'):
        body = body if isinstance(body, bytes) else body.encode('utf-8')
        self.send_response(code)
        self.send_header('Content-Type', kind)
        self.send_header('Cache-Control', 'no-store')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        url = urllib.parse.urlparse(self.path)
        q = dict(urllib.parse.parse_qsl(url.query))
        try:
            if url.path in ('/', '/index.html'):
                return self.send(200, (HERE / 'index.html').read_bytes(), 'text/html; charset=utf-8')
            if url.path == '/api/state':
                return self.send(200, json.dumps(snapshot(self.sources, self.github)))
            source = next((s for s in self.sources if s.name == q.get('machine')), None)
            if not source:
                return self.send(404, '{"error": "unknown machine"}')
            if url.path == '/api/test':
                return self.send(200, json.dumps(detail(source, q['id'])))
            if url.path == '/file':
                path = q['path']
                if not path.lower().endswith(IMAGE + TEXT):
                    return self.send(403, '{"error": "type not served"}')
                kind = mimetypes.guess_type(path)[0] or 'text/plain; charset=utf-8'
                return self.send(200, source.read(path), kind)
            return self.send(404, '{"error": "not found"}')
        except PermissionError as error:
            return self.send(403, json.dumps(dict(error=str(error))))
        except Exception as error:
            return self.send(500, json.dumps(dict(error=str(error))))


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--host', default='127.0.0.1', help='127.0.0.1 (default) or this machine\'s tailnet (100.64.0.0/10) address')
    p.add_argument('--port', type=int, default=4120)
    p.add_argument('--run', action='append', metavar='MACHINE=DIR', help='pin a machine to one results folder')
    p.add_argument('--github-run', help='pin the GitHub run instead of looking it up by commit')
    p.add_argument('--export', metavar='HTML', help='write the frozen, self-contained page and exit')
    args = p.parse_args()
    sources, github = sources_from(args), GitHub(args.github_run)
    if args.export:
        print('Wrote ' + str(export(sources, github, args.export)))
        return 0
    address = ipaddress.ip_address(args.host)
    if not (address.is_loopback or address in ipaddress.ip_network('100.64.0.0/10')):
        raise SystemExit('Bind to 127.0.0.1 or a tailnet address only, never a public or LAN address')
    Handler.sources, Handler.github = sources, github
    server = http.server.ThreadingHTTPServer((args.host, args.port), Handler)
    print(f'Test dashboard: http://{args.host}:{args.port}/  (machines: {", ".join(s.name for s in sources)})', flush=True)
    server.serve_forever()


if __name__ == '__main__':
    sys.exit(main())
