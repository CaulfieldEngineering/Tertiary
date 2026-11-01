#!/usr/bin/env python3
"""[Claude 2026-10-31] Build-step results in the same shape as tests/ (tests/README.md, "Results" and "Live progress").

Every step script calls `begin` when it starts and `end` when it stops. That writes
<results>/progress.jsonl (format v1: run-start, test-start, test-finish, run-end) and <results>/<ID>.json,
so the test dashboard and combine.py can read builds the way they read tests. Build-All / build-all.sh call
`run-start` and `run-end` and set WLA_BUILD_NESTED=1, so a step run alone writes its own run-start/run-end.

  python scripts/lib/report.py begin --id B-01 --title "Plug-in" [--results DIR]
  python scripts/lib/report.py end   --id B-01 --status PASS --detail "..." [--artifact build/x.log] [--results DIR]
  python scripts/lib/report.py run-start --run build-all [--results DIR]     (prints the folder)
  python scripts/lib/report.py run-end   --run build-all [--results DIR]     (writes summary.md/json; exit 1 on FAIL)

The results folder is --results, else $WLA_BUILD_RESULTS, else a new scripts/results/YYYY-MM-DD_HHMM/.
Exit codes of the step scripts follow tests/: 0 PASS, 1 FAIL, 2 SKIP, 3 PLANNED.
"""
import argparse
import datetime as dt
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
PLATFORM = {'Windows': 'win', 'Darwin': 'mac'}.get(platform.system(), 'linux')
CODES = ('PASS', 'FAIL', 'SKIP', 'PLANNED')


def now():
    return dt.datetime.now().astimezone().isoformat(timespec='seconds')


def git_state():
    run = lambda *a: subprocess.run(['git', '-C', str(ROOT), *a], capture_output=True, text=True).stdout.rstrip()
    return run('rev-parse', 'HEAD').strip(), [x[3:] for x in run('status', '--porcelain', '--untracked-files=no').splitlines()]


def folder(value):
    value = value or os.environ.get('WLA_BUILD_RESULTS')
    if value:
        path = Path(value)
        path = path if path.is_absolute() else ROOT / path
    else:
        base = ROOT / 'scripts/results' / dt.datetime.now().strftime('%Y-%m-%d_%H%M')
        path, n = base, 0
        while path.exists():
            n += 1
            path = base.with_name(f'{base.name}-{n:02}')
    path.mkdir(parents=True, exist_ok=True)
    return path.resolve()


def progress(results, event, **fields):
    with open(results / 'progress.jsonl', 'a', encoding='utf-8') as f:
        f.write(json.dumps(dict(v=1, time=now(), event=event, **fields), ensure_ascii=False) + '\n')
        f.flush()


def profile():
    return 'ci' if os.environ.get('GITHUB_ACTIONS') == 'true' else 'local'


def run_start(results, name):
    commit, dirty = git_state()
    progress(results, 'run-start', run=name, machine=platform.node(), platform=PLATFORM, profile=profile(),
             commit=commit, dirty=dirty, build=None, kind='build')
    (results / '.run-start').write_text(str(time.time()), encoding='utf-8')


def duration(seconds):
    seconds = int(round(seconds or 0))
    h, rest = divmod(seconds, 3600)
    m, s = divmod(rest, 60)
    return ' '.join(x for x in (f'{h} h' if h else '', f'{m} min' if (h or m) else '', f'{s} s') if x)


def run_end(results, name):
    reports = []
    for line in (results / 'progress.jsonl').read_text(encoding='utf-8').splitlines():
        e = json.loads(line)
        if e['event'] == 'test-finish' and (results / (e['id'] + '.json')).exists():
            reports.append(json.loads((results / (e['id'] + '.json')).read_text(encoding='utf-8')))
    counts = {s: sum(c['status'] == s for r in reports for c in r['checks']) for s in CODES}
    status = 'FAIL' if counts['FAIL'] else 'PASS'
    started = float((results / '.run-start').read_text()) if (results / '.run-start').exists() else time.time()
    seconds = round(time.time() - started, 1)
    commit, dirty = git_state()
    data = dict(kind='build', platform=PLATFORM, profile=profile(), finished=now(), commit=commit, dirty=dirty,
                status=status, counts=counts, results=reports)
    (results / 'summary.json').write_text(json.dumps(data, indent=2) + '\n', encoding='utf-8')
    lines = [f'# Build {status}', '', f'Commit `{commit}` on {platform.node()} ({PLATFORM}), {data["finished"]}, took {duration(seconds)}.', '',
             '| ID | Step | Status | Seconds | Detail |', '|---|---|---|---|---|']
    for r in reports:
        detail = '; '.join(c['detail'] for c in r['checks'])[:300].replace('|', '/').replace('\n', ' ')
        lines.append(f"| {r['id']} | {r['title']} | {r['status']} | {r.get('seconds', '')} | {detail} |")
    (results / 'summary.md').write_text('\n'.join(lines) + '\n', encoding='utf-8')
    progress(results, 'run-end', run=name, status=status, counts=counts, seconds=seconds)
    print('\n'.join(lines))
    print(f'Build {name}: took {duration(seconds)}. Results: {results}')
    return 1 if status == 'FAIL' else 0


def begin(results, step_id, title):
    if os.environ.get('WLA_BUILD_NESTED') != '1':
        run_start(results, step_id)
    progress(results, 'test-start', id=step_id, title=title)
    (results / f'.{step_id}.start').write_text(json.dumps(dict(t=time.time(), started=now(), title=title)), encoding='utf-8')


def end(results, step_id, status, detail, artifacts, title=None):
    marker = results / f'.{step_id}.start'
    info = json.loads(marker.read_text(encoding='utf-8')) if marker.exists() else dict(t=time.time(), started=now(), title=title or step_id)
    seconds = round(time.time() - info['t'], 1)
    rel = []
    for a in artifacts:
        p = Path(a).resolve()
        try:
            rel.append(os.path.relpath(p, results).replace('\\', '/'))
        except ValueError:
            rel.append(str(p))
    data = dict(id=step_id, name=step_id, title=title or info['title'], suite_ids=[], profile=profile(), platform=PLATFORM,
                status=status, started=info['started'], finished=now(), seconds=seconds, kind='build',
                checks=[dict(test_id=step_id, id=step_id.lower(), status=status, detail=detail)], artifacts=rel)
    (results / f'{step_id}.json').write_text(json.dumps(data, indent=2) + '\n', encoding='utf-8')
    progress(results, 'test-finish', id=step_id, status=status, counts={s: int(s == status) for s in CODES}, seconds=seconds)
    print(f'[{step_id}] {status}: {detail} ({duration(seconds)})', flush=True)
    if os.environ.get('WLA_BUILD_NESTED') != '1':
        run_end(results, step_id)
    return CODES.index(status)


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('action', choices=['begin', 'end', 'run-start', 'run-end', 'folder'])
    p.add_argument('--results')
    p.add_argument('--id')
    p.add_argument('--title', default='')
    p.add_argument('--run', default='build-all')
    p.add_argument('--status', choices=CODES)
    p.add_argument('--detail', default='')
    p.add_argument('--artifact', action='append', default=[])
    a = p.parse_args()
    results = folder(a.results)
    if a.action == 'folder':
        print(results)
        return 0
    if a.action == 'run-start':
        run_start(results, a.run)
        print(results)
        return 0
    if a.action == 'run-end':
        return run_end(results, a.run)
    if a.action == 'begin':
        begin(results, a.id, a.title or a.id)
        return 0
    return end(results, a.id, a.status, a.detail, a.artifact, a.title or None)


if __name__ == '__main__':
    sys.exit(main())
